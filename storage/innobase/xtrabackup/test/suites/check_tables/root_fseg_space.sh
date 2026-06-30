############################################################################
# PXB-3804 / Issue 4 (HIGH)
#
# xtrabackup --prepare --check-tables crashes in btr_root_fseg_validate()
# on multi-field root-page corruption (the file-segment header space id no
# longer matches the tablespace).
#
#   Location : btr0btr.cc:156  btr_root_fseg_validate()
#              ut_a(mach_read_from_4(seg_header + FSEG_HDR_SPACE) == space)
#   Trigger  : on the root page set
#                PAGE_LEVEL  (offset 90) = 99
#                PAGE_N_RECS (offset 86) = 0xFFFF
#                FIL_PAGE_NEXT (offset 12) = 0
#
# Expected (after fix):
#   - index reported "is corrupted", "Table check failed"
#   - non-zero exit, clean shutdown (NO assertion / signal 6)
#
# Actual (bug):
#   InnoDB: Assertion failure: btr0btr.cc:156
#   mysqld got signal 6
#
# --innodb-checksum-algorithm=none is required so the corrupted page is
# accepted by the buffer pool and reaches the --check-tables path.
############################################################################

. inc/common.sh


start_server --innodb_file_per_table

vlog "Create test_users and populate 10000 rows (multi-page B-tree)"
mysql test <<'EOF'
SET SESSION cte_max_recursion_depth = 20000;
CREATE TABLE test_users (id INT PRIMARY KEY, name VARCHAR(100));
INSERT INTO test_users
WITH RECURSIVE seq(n) AS (
  SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 10000
)
SELECT n, CONCAT('user', n) FROM seq;
EOF

vlog "Full backup"
xtrabackup --backup --target-dir=$topdir/backup

vlog "Prepare with --apply-log-only (leave the backup re-preparable)"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup

IBD=$topdir/backup/test/test_users.ibd
PAGE_SZ=16384
PAGE_NO=4

FSIZE=$(stat -c %s "$IBD")
NEED=$(( (PAGE_NO + 1) * PAGE_SZ ))
[ "$FSIZE" -ge "$NEED" ] || \
  die "root_fseg_space: $IBD is only $FSIZE bytes; need >= $NEED"

vlog "Apply multi-field corruption to root page $PAGE_NO"
# PAGE_LEVEL (offset 90) = 99  -> 0x0063 (big-endian, 2 bytes)
mach_write_2 "$IBD" $PAGE_NO 90 0x0063
# PAGE_N_RECS (offset 86) = 0xFFFF
mach_write_2 "$IBD" $PAGE_NO 86 0xFFFF
# FIL_PAGE_NEXT (offset 12) = 0
mach_write_4 "$IBD" $PAGE_NO 12 0

vlog "Prepare with --check-tables: must detect corruption gracefully, not crash"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup 2>&1 | tee $topdir/check.log

if grep -qiE "Assertion failure|got signal|intentionally generate a memory trap" \
     $topdir/check.log; then
  die "root_fseg_space: xtrabackup CRASHED on multi-field root corruption instead of reporting corruption (PXB-3804)"
fi

grep -q "Starting table checks" $topdir/check.log || \
  die "root_fseg_space: check-tables did not start"
grep -q "is corrupted" $topdir/check.log || \
  die "root_fseg_space: corruption not reported"
grep -q "Table check failed" $topdir/check.log || \
  die "root_fseg_space: 'Table check failed' message not found"

vlog "root_fseg_space passed: multi-field root corruption reported gracefully, no crash"
