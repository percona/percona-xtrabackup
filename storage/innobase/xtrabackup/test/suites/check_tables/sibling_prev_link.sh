############################################################################
# PXB-3804 / Issue 1 (CRITICAL)
#
# xtrabackup --prepare --check-tables crashes inside btr_validate_level()
# when a leaf page has a broken sibling link (FIL_PAGE_PREV corrupted).
#
#   Location : btr0btr.cc:4402  btr_validate_level()
#   Trigger  : FIL_PAGE_PREV (offset 8) on a leaf page set to 0xDEADBEEF
#
# Expected (after fix):
#   - "broken FIL_PAGE_NEXT or FIL_PAGE_PREV" logged
#   - index reported "is corrupted", "Table check failed"
#   - non-zero exit, clean InnoDB shutdown (NO assertion / signal 6)
#
# Actual (bug):
#   InnoDB: Assertion failure: btr0btr.cc:4402:siblings_link_correct
#   mysqld got signal 6
#
# --innodb-checksum-algorithm=none is required so the corrupted page is
# accepted by the buffer pool and reaches the --check-tables validation
# path (mirrors the original reproduction).
############################################################################

. inc/common.sh

require_debug_pxb_version

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
PAGE_NO=10

FSIZE=$(stat -c %s "$IBD")
NEED=$(( (PAGE_NO + 1) * PAGE_SZ ))
[ "$FSIZE" -ge "$NEED" ] || \
  die "sibling_prev_link: $IBD is only $FSIZE bytes; need >= $NEED (not enough rows for page $PAGE_NO)"

vlog "Corrupt FIL_PAGE_PREV (offset 8) of page $PAGE_NO to 0xDEADBEEF"
mach_write_4 "$IBD" $PAGE_NO 8 0xDEADBEEF

vlog "Prepare with --check-tables: must detect corruption gracefully, not crash"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup 2>&1 | tee $topdir/check.log

if grep -qiE "Assertion failure|got signal|intentionally generate a memory trap" \
     $topdir/check.log; then
  die "sibling_prev_link: xtrabackup CRASHED on broken FIL_PAGE_PREV instead of reporting corruption (PXB-3804)"
fi

grep -q "Starting table checks" $topdir/check.log || \
  die "sibling_prev_link: check-tables did not start"
grep -q "is corrupted" $topdir/check.log || \
  die "sibling_prev_link: corruption not reported"
grep -q "Table check failed" $topdir/check.log || \
  die "sibling_prev_link: 'Table check failed' message not found"

vlog "sibling_prev_link passed: broken sibling link reported gracefully, no crash"
