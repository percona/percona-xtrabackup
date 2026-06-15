############################################################################
# PXB-3804 / Issue 2 (CRITICAL)
#
# xtrabackup --prepare --check-tables HANGS INDEFINITELY when a leaf page
# has a corrupt forward sibling link (FIL_PAGE_NEXT). InnoDB follows the
# bogus page number and tries to preallocate a ~61 PB file.
#
#   Location : sibling traversal in btr_validate_level()
#   Trigger  : FIL_PAGE_NEXT (offset 12) on a leaf page set to 0xDEADBEEF
#
# Expected (after fix):
#   - index reported "is corrupted", "Table check failed"
#   - completes within seconds, non-zero exit (NO hang, NO 61PB fallocate)
#
# Actual (bug):
#   [MY-012144] posix_fallocate(): ... desired size 61209453068288 bytes
#   process hangs forever (exit 124 under `timeout`)
#
# The check-tables invocation is wrapped in `timeout` so a regression of
# the hang fails the test deterministically instead of stalling the suite.
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
PAGE_SIZE=16384
PAGE_NO=10

FSIZE=$(stat -c %s "$IBD")
NEED=$(( (PAGE_NO + 1) * PAGE_SIZE ))
[ "$FSIZE" -ge "$NEED" ] || \
  die "sibling_next_out_of_bounds: $IBD is only $FSIZE bytes; need >= $NEED (not enough rows for page $PAGE_NO)"

vlog "Corrupt FIL_PAGE_NEXT (offset 12) of page $PAGE_NO to 0xDEADBEEF"
mach_write_4 "$IBD" $PAGE_NO 12 0xDEADBEEF

CHECK_TIMEOUT=120
vlog "Prepare with --check-tables under a ${CHECK_TIMEOUT}s timeout (must not hang)"
set +e
timeout $CHECK_TIMEOUT $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup 2>&1 | tee $topdir/check.log
RC=${PIPESTATUS[0]}
set -e
vlog "check-tables exit code: $RC"

if [ "$RC" -eq 124 ]; then
  die "sibling_next_out_of_bounds: --check-tables HUNG (timed out after ${CHECK_TIMEOUT}s) on corrupt FIL_PAGE_NEXT (PXB-3804)"
fi
if grep -qi "posix_fallocate" $topdir/check.log; then
  die "sibling_next_out_of_bounds: xtrabackup attempted an absurd preallocation following the corrupt FIL_PAGE_NEXT (PXB-3804)"
fi
if grep -qiE "Assertion failure|got signal|intentionally generate a memory trap" \
     $topdir/check.log; then
  die "sibling_next_out_of_bounds: xtrabackup CRASHED instead of reporting corruption (PXB-3804)"
fi
if [ "$RC" -eq 0 ]; then
  die "sibling_next_out_of_bounds: --check-tables succeeded on a corrupt tablespace"
fi

grep -q "is corrupted" $topdir/check.log || \
  die "sibling_next_out_of_bounds: corruption not reported"
grep -q "Table check failed" $topdir/check.log || \
  die "sibling_next_out_of_bounds: 'Table check failed' message not found"

vlog "sibling_next_out_of_bounds passed: corrupt FIL_PAGE_NEXT reported gracefully, no hang"
