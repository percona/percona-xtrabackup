############################################################################
# PXB-3804 : --check-tables gracefully rejects a record whose REC_STATUS is
# corrupted to an out-of-range value, instead of aborting.
#
# This is the scenario rec_validate_page_chain() (rem/rec.cc) exists to catch.
# A compact record's 3-bit status (REC_NEW_STATUS, low bits of the byte at
# rec_origin-3) only has four legal values: ORDINARY(0), NODE_PTR(1),
# INFIMUM(2), SUPREMUM(3). Setting it to 4 leaves the record's next-link,
# owned-count, directory slots, PAGE_N_RECS and PAGE_HEAP_TOP all intact -- so
# page_simple_validate_new() (which never inspects the status) accepts the
# page. The record would then reach rec_get_offsets(), whose status switch
# ends in "default: ut_error" (rec.cc), aborting xtrabackup with a fatal
# signal on BOTH debug and release builds.
#
# rec_validate_page_chain() walks infimum -> ... -> supremum reading only the
# masked status bits and the page-bounded next offset (neither asserts under
# XTRABACKUP), and rejects the page the moment it sees a status that is not
# ORDINARY/NODE_PTR/SUPREMUM. --check-tables then reports "invalid record
# chain" / "is corrupted" and exits non-zero, with no abort and no hang.
#
# Regression guard: with rec_validate_page_chain() removed this test CRASHES
# (Assertion failure: rec.cc, ut_error) instead of failing cleanly -- so it
# fails if that check is reverted, which is exactly its purpose.
#
# Checksums are turned off for the run so the single-byte edit reaches the
# structural validators rather than tripping the checksum-on-read path first.
#
# within-page offsets: PAGE_NEW_INFIMUM origin = 99; a record's status byte is
# at (origin - 3); the low 3 bits are the status, the high 5 bits info-bits.
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

vlog "Create a table with many rows so a clustered leaf page holds many records"
mysql test <<'EOF'
SET SESSION cte_max_recursion_depth = 20000;
CREATE TABLE t1 (id INT PRIMARY KEY, pad VARCHAR(100));
INSERT INTO t1
WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 5000)
SELECT n, CONCAT('p', n) FROM seq;
EOF

vlog "Full backup + apply-log-only prepare"
xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
IBD=$topdir/backup/test/t1.ibd

vlog "Locate a clustered-index leaf page and its first user record"
read MAXLEVEL LEAF L1 L2 <<< "$(find_clustered_pages_by_level "$IBD")"
[ "$LEAF" != "NONE" ] || die "could not locate a clustered leaf page"
ORIGIN=$(find_first_user_rec_origin "$IBD" "$LEAF")
STATUS_BYTE_OFF=$(( ORIGIN - 3 ))
[ "$ORIGIN" -gt 0 ] || die "could not resolve first user record origin"
vlog "leaf page $LEAF : first user record origin=$ORIGIN status byte at $STATUS_BYTE_OFF"

#
# Control: a clean backup must pass --check-tables.
#
vlog "=== Control: clean --check-tables ==="
cp -r $topdir/backup $topdir/backup_ctrl
xtrabackup --prepare --check-tables --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup_ctrl 2>&1 | tee $topdir/ctrl.log
grep -q "All table checks passed" $topdir/ctrl.log || \
  die "Control: clean backup unexpectedly failed --check-tables"
vlog "Control passed"

#
# Negative: set the first user record's status to 4 (invalid), keeping the
# info-bits in the high 5 bits of the same byte unchanged.
#
vlog "=== Negative: corrupt REC_STATUS (0 -> 4) on leaf page $LEAF ==="
cp -r $topdir/backup $topdir/backup_bad
CIBD=$topdir/backup_bad/test/t1.ibd
ORIG_BYTE=$(mach_read_n "$CIBD" "$LEAF" "$STATUS_BYTE_OFF" 1)
NEW_BYTE=$(( (ORIG_BYTE & 0xF8) | 0x4 ))
vlog "status byte 0x$(printf %02x $ORIG_BYTE) -> 0x$(printf %02x $NEW_BYTE)"
mach_write_n "$CIBD" "$LEAF" "$STATUS_BYTE_OFF" "$NEW_BYTE" 1

SIZE_BEFORE=$(stat -c %s "$CIBD")
set +e
timeout 120 $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup_bad 2>&1 | tee $topdir/bad.log
RC=${PIPESTATUS[0]}
set -e
SIZE_AFTER=$(stat -c %s "$CIBD")
vlog "check-tables exit code: $RC"

[ "$RC" -ne 124 ] || die "record_status_invalid: --check-tables HUNG on a corrupt record status"
grep -qiE "Assertion failure|got signal|ut_error|ib::fatal triggered" $topdir/bad.log && \
  die "record_status_invalid: --check-tables ABORTED on a corrupt record status (chain check missing?)"
[ "$SIZE_AFTER" = "$SIZE_BEFORE" ] || \
  die "record_status_invalid: backup .ibd grew during --check-tables ($SIZE_BEFORE -> $SIZE_AFTER)"
[ "$RC" -ne 0 ] || die "record_status_invalid: --check-tables passed a record with an invalid status"
grep -q "invalid record chain" $topdir/bad.log || \
  die "record_status_invalid: \"invalid record chain\" not reported"
grep -q "is corrupted" $topdir/bad.log || \
  die "record_status_invalid: corruption not reported (\"is corrupted\")"
grep -q "Table check failed" $topdir/bad.log || \
  die "record_status_invalid: \"Table check failed\" not reported"

vlog "record_status_invalid passed: invalid record status reported gracefully (rc=$RC)"
