############################################################################
# PXB-3804 : --check-tables must not abort on a corrupt root PAGE_LEVEL.
#
# Theory (Gap 1): btr_validate_index() reads the root level up front with
#   ulint n = btr_page_get_level(root);
# and loops btr_validate_level(n-i) for i in [0..n]. That btr_page_get_level()
# is NOT guarded:
#   * debug   -> aborts on ut_ad(level <= BTR_MAX_NODE_LEVEL) (btr0btr.ic)
#   * release -> n becomes huge (e.g. 0xFFFF), spinning up to 65536 passes.
# This is ABOVE btr_validate_level(), so the descent/page_validate level
# guards do not cover it.
#
# Repro: smash the clustered-index ROOT page's PAGE_LEVEL (abs offset
# PAGE_HEADER+PAGE_LEVEL = 38+26 = 64) to 0xFFFF. Expectation after fix:
# graceful corruption report, non-zero exit, no abort.
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
read MAXLEVEL L0 L1 L2 <<< "$(find_clustered_pages_by_level "$IBD")"
[ "$MAXLEVEL" -ge 1 ] || die "need a multi-level clustered index (MAXLEVEL=$MAXLEVEL)"
# The single page at the top level is the clustered-index root.
case "$MAXLEVEL" in
  1) ROOT=$L1 ;;
  2) ROOT=$L2 ;;
  *) die "unexpected MAXLEVEL=$MAXLEVEL" ;;
esac
[ "$ROOT" != "NONE" ] || die "could not locate the clustered root page"
PAGE_LEVEL_OFF=$(( 38 + 26 ))   # PAGE_HEADER + PAGE_LEVEL
vlog "root page=$ROOT, PAGE_LEVEL at abs offset $PAGE_LEVEL_OFF"

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
# Negative: smash root PAGE_LEVEL to 0xFFFF.
#
vlog "=== Negative: root PAGE_LEVEL -> 0xFFFF on page $ROOT ==="
cp -r $topdir/backup $topdir/backup_bad
CIBD=$topdir/backup_bad/test/t1.ibd
ORIG=$(mach_read_n "$CIBD" "$ROOT" "$PAGE_LEVEL_OFF" 2)
mach_write_n "$CIBD" "$ROOT" "$PAGE_LEVEL_OFF" 65535 2
vlog "root level $ORIG -> 65535"

SIZE_BEFORE=$(stat -c %s "$CIBD")
set +e
timeout 120 $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup_bad 2>&1 | tee $topdir/bad.log
RC=${PIPESTATUS[0]}
set -e
SIZE_AFTER=$(stat -c %s "$CIBD")
vlog "check-tables exit code: $RC"

[ "$RC" -ne 124 ] || die "root_level_corrupt: --check-tables HUNG"
grep -qiE "Assertion failure|got signal|ut_error|ib::fatal triggered" $topdir/bad.log && \
  die "root_level_corrupt: --check-tables ABORTED on a corrupt root level"
[ "$SIZE_AFTER" = "$SIZE_BEFORE" ] || \
  die "root_level_corrupt: .ibd grew during --check-tables ($SIZE_BEFORE -> $SIZE_AFTER)"
[ "$RC" -ne 0 ] || die "root_level_corrupt: --check-tables passed a corrupt root level"
grep -qiE "out-of-range|B-tree corruption|Table check failed" $topdir/bad.log || \
  die "root_level_corrupt: corruption not reported"

vlog "root_level_corrupt passed: corrupt root level reported gracefully (rc=$RC)"
