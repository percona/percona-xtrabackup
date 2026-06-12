############################################################################
# PXB-3804 : --check-tables must not hang on a cyclic sibling chain.
#
# Theory (Gap 2): btr_validate_level() scans a level left->right via
# FIL_PAGE_NEXT with NO visited tracking and NO iteration bound. The existing
# guards do not stop a *consistent, in-bounds* cycle:
#   * btr_page_no_in_bounds  -> only rejects out-of-tablespace page numbers
#   * page_is_empty          -> only the all-zero FIL_PAGE_NEXT=0 case
#   * back-link check        -> only inconsistent prev/next
#   * key-order check        -> sets ret=false but does NOT stop the loop
# check-tables passes trx=nullptr, so trx_is_interrupted() never breaks out.
#
# Repro: make the two leftmost leaves a closed cycle A<->B:
#   A.next=B (orig), B.prev=A (orig), and corrupt B.next=A, A.prev=B.
# The scan starts at A (the descent target), so there is no entry edge whose
# back-link could be checked, and A.next=B / B.next=A are mutually consistent.
# Expectation BEFORE fix: infinite loop -> timeout. AFTER fix: the bounded
# scan reports the cycle and exits non-zero.
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

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

# leftmost leaf A (FIL_PAGE_PREV == FIL_NULL) and its right sibling B.
read A B <<< "$(find_leftmost_leaf "$IBD")"
[ "$A" != "NONE" ] && [ "$B" != "4294967295" ] || \
  die "need a leftmost leaf with a right sibling (got '$A' '$B')"
vlog "leftmost leaf A=$A, right sibling B=$B"

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
# Negative: close the A<->B cycle (A.prev=B, B.next=A).
#  FIL_PAGE_PREV offset = 8, FIL_PAGE_NEXT offset = 12 (4 bytes each).
#
vlog "=== Negative: make sibling chain a cycle A<->B ==="
cp -r $topdir/backup $topdir/backup_bad
CIBD=$topdir/backup_bad/test/t1.ibd
mach_write_n "$CIBD" "$A" 8  "$B" 4    # A.FIL_PAGE_PREV = B
mach_write_n "$CIBD" "$B" 12 "$A" 4    # B.FIL_PAGE_NEXT = A

SIZE_BEFORE=$(stat -c %s "$CIBD")
set +e
timeout 90 $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup_bad 2>&1 | tee $topdir/bad.log
RC=${PIPESTATUS[0]}
set -e
SIZE_AFTER=$(stat -c %s "$CIBD")
vlog "check-tables exit code: $RC"

[ "$RC" -ne 124 ] || die "sibling_link_cycle: --check-tables HUNG on a cyclic sibling chain"
grep -qiE "Assertion failure|got signal|ut_error|ib::fatal triggered" $topdir/bad.log && \
  die "sibling_link_cycle: --check-tables ABORTED"
[ "$SIZE_AFTER" = "$SIZE_BEFORE" ] || \
  die "sibling_link_cycle: .ibd grew during --check-tables ($SIZE_BEFORE -> $SIZE_AFTER)"
[ "$RC" -ne 0 ] || die "sibling_link_cycle: --check-tables passed a cyclic sibling chain"
grep -qiE "cycle|too many pages|sibling|Table check failed" $topdir/bad.log || \
  die "sibling_link_cycle: corruption not reported"

vlog "sibling_link_cycle passed: cyclic sibling chain reported gracefully (rc=$RC)"
