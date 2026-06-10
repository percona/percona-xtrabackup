############################################################################
# PXB-3804 / Issue 5 (HIGH)
#
# xtrabackup --prepare --check-tables crashes in rec_get_offsets() while
# LOADING THE DICTIONARY from SDI -- before btr_validate_index() is ever
# called -- when a system record on the SDI index root page (page 3) is
# corrupted so its REC_STATUS becomes invalid.
#
#   Location : rec.cc:385  rec_get_offsets()  (default: ut_error in the
#              REC_STATUS switch -- status not in ORDINARY/NODE_PTR/
#              INFIMUM/SUPREMUM)
#   Path     : check_tables_thread_func -> dict_load_..._from_space_id
#              -> ib_sdi_get_keys -> ib_cursor_first -> row_search_mvcc
#              -> rec_get_offsets
#
# What the corruption REALLY is (the JIRA labels it "PAGE_INDEX_ID=99999",
# but that is mislabeled -- PAGE_INDEX_ID lives at offset 66, not 92):
#   The 8-byte write at offset 92 spans bytes 92..99 of page 3, which is:
#     92-93 : FSEG_HDR_OFFSET of PAGE_BTR_SEG_TOP
#     94-98 : the infimum system record's 5-byte header
#             (PAGE_DATA=94; PAGE_NEW_INFIMUM origin=99; the 3-bit
#              REC_NEW_STATUS sits at origin-3 = offset 96)
#     99    : infimum record origin
#   So it overwrites the infimum record header -- including its REC_STATUS
#   byte at 96 and its next-record pointer -- not PAGE_INDEX_ID.  When the
#   SDI cursor scans the page it reads a record whose status is garbage and
#   rec_get_offsets() hits the default: ut_error.
#
#   The parsing index here is the FIXED SDI index (ib_sdi_open_table opens
#   by dict_sdi_get_table_id), so the crash is purely from the unparseable
#   record header, not from any index-id confusion.
#
# Expected (after fix):
#   - dictionary load fails -> "Cannot load dictionary" / table reported
#     corrupt, "Table check failed"
#   - non-zero exit, clean shutdown (NO assertion / signal 6)
#
# Actual (bug):
#   InnoDB: Assertion failure: rec.cc:385
#   mysqld got signal 6
#
# --innodb-checksum-algorithm=none is required so the corrupted page is
# accepted by the buffer pool and reaches the SDI read path.
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
PAGE_NO=3

FSIZE=$(stat -c %s "$IBD")
NEED=$(( (PAGE_NO + 1) * PAGE_SZ ))
[ "$FSIZE" -ge "$NEED" ] || \
  die "sdi_record_header: $IBD is only $FSIZE bytes; need >= $NEED"

# Faithful to the JIRA repro: 8-byte big-endian write of 99999 at offset 92
# of the SDI root page.  This overwrites the infimum system-record header
# (bytes 94-98, incl. the REC_STATUS byte at 96), corrupting its record
# status -- despite the JIRA calling it "PAGE_INDEX_ID" (see header above).
vlog "Corrupt infimum record header (offset 92, 8 bytes) of SDI root page $PAGE_NO"
# 99999 = 0x0001869F as a big-endian 8-byte value
mach_write_8 "$IBD" $PAGE_NO 92 0x1869F

vlog "Prepare with --check-tables: must detect corruption gracefully, not crash"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup 2>&1 | tee $topdir/check.log

if grep -qiE "Assertion failure|got signal|intentionally generate a memory trap" \
     $topdir/check.log; then
  die "sdi_record_header: xtrabackup CRASHED in SDI/dictionary load instead of reporting corruption (PXB-3804)"
fi

grep -q "Starting table checks" $topdir/check.log || \
  die "sdi_record_header: check-tables did not start"
grep -qiE "is corrupted|corrupt|cannot load|Table check failed" $topdir/check.log || \
  die "sdi_record_header: corruption not reported"
grep -q "Table check failed" $topdir/check.log || \
  die "sdi_record_header: 'Table check failed' message not found"

vlog "sdi_record_header passed: corrupt SDI PAGE_INDEX_ID reported gracefully, no crash"
