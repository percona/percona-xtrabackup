############################################################################
# PXB-3804 : --check-tables must not abort while comparing adjacent keys
# across sibling leaf pages.
#
# btr_validate_level() scans each B-tree level left to right. For the current
# page it parses the last user record, and for its RIGHT sibling it parses the
# first user record (right_rec), to verify the cross-page key ordering. The
# right sibling, however, is only run through page_validate() on the NEXT
# iteration (when it becomes the current page). So a right sibling whose first
# user record carries a corrupt REC_STATUS reaches
#   rec_get_offsets(right_rec)  ->  rec.cc "default: ut_error"
# and aborts xtrabackup (debug AND release) BEFORE that sibling is validated.
#
# This is distinct from record_status_invalid.sh: there the corrupted page is
# the descent target and page_validate() catches it as the CURRENT page; here
# the corruption is on a sibling and is touched one iteration early.
#
# The fix validates the right sibling's record chain before the adjacent-key
# comparison parses it, so the corruption is reported gracefully instead.
#
# Regression guard: revert the right-sibling check in btr_validate_level() and
# this test CRASHES (Assertion failure: rec.cc) instead of failing cleanly.
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

vlog "Create a table large enough for several clustered leaf pages"
mysql test <<'EOF'
SET SESSION cte_max_recursion_depth = 20000;
CREATE TABLE t1 (id INT PRIMARY KEY, pad VARCHAR(100));
INSERT INTO t1
WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 5000)
SELECT n, CONCAT('p', n) FROM seq;
EOF

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
IBD=$topdir/backup/test/t1.ibd

PAGE_SIZE=$(get_page_size "$IBD")

# Locate the LEFTMOST leaf (FIL_PAGE_PREV == FIL_NULL) and its right sibling
# (FIL_PAGE_NEXT). The descent lands on the leftmost leaf, so the sibling is
# parsed as right_rec while still unvalidated -- the path we want to exercise.
read LEFT SIBLING <<< "$(find_leftmost_leaf "$IBD")"
[ "$LEFT" != "NONE" ] && [ "$SIBLING" != "4294967295" ] || \
  die "could not find a leftmost leaf with a right sibling (got '$LEFT' '$SIBLING')"
vlog "leftmost leaf=$LEFT, right sibling=$SIBLING"

ORIGIN=$(find_first_user_rec_origin "$IBD" "$SIBLING")
[ "$ORIGIN" -gt 0 ] || die "could not resolve first user record origin on sibling"
STATUS_BYTE_OFF=$(( ORIGIN - 3 ))

#
# Control: clean backup passes.
#
vlog "=== Control: clean --check-tables ==="
cp -r $topdir/backup $topdir/backup_ctrl
xtrabackup --prepare --check-tables --target-dir=$topdir/backup_ctrl 2>&1 \
  | tee $topdir/ctrl.log
grep -q "All table checks passed" $topdir/ctrl.log || \
  die "Control: clean backup unexpectedly failed --check-tables"
vlog "Control passed"

#
# Negative: corrupt the RIGHT SIBLING's first user record status to 4.
#
vlog "=== Negative: corrupt REC_STATUS (->4) on right sibling page $SIBLING ==="
cp -r $topdir/backup $topdir/backup_bad
CIBD=$topdir/backup_bad/test/t1.ibd
ORIG_BYTE=$(mach_read_n "$CIBD" "$SIBLING" "$STATUS_BYTE_OFF" 1)
NEW_BYTE=$(( (ORIG_BYTE & 0xF8) | 0x4 ))
vlog "status byte 0x$(printf %02x $ORIG_BYTE) -> 0x$(printf %02x $NEW_BYTE)"
mach_write_n "$CIBD" "$SIBLING" "$STATUS_BYTE_OFF" "$NEW_BYTE" 1

SIZE_BEFORE=$(stat -c %s "$CIBD")
set +e
timeout 120 $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup_bad 2>&1 | tee $topdir/bad.log
RC=${PIPESTATUS[0]}
set -e
SIZE_AFTER=$(stat -c %s "$CIBD")
vlog "check-tables exit code: $RC"

[ "$RC" -ne 124 ] || die "sibling_record_status: --check-tables HUNG"
grep -qiE "Assertion failure|got signal|ut_error|ib::fatal triggered" $topdir/bad.log && \
  die "sibling_record_status: --check-tables ABORTED on a corrupt sibling record"
[ "$SIZE_AFTER" = "$SIZE_BEFORE" ] || \
  die "sibling_record_status: .ibd grew during --check-tables ($SIZE_BEFORE -> $SIZE_AFTER)"
[ "$RC" -ne 0 ] || die "sibling_record_status: --check-tables passed a corrupt sibling record"
grep -qiE "right sibling page|invalid record chain|Table check failed" $topdir/bad.log || \
  die "sibling_record_status: corruption not reported"

vlog "sibling_record_status passed: corrupt right sibling reported gracefully (rc=$RC)"
