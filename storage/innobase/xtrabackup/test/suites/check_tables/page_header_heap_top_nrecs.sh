############################################################################
# PXB-3804 : --check-tables gracefully detects two corrupt index page-header
# fields -- PAGE_HEAP_TOP and PAGE_N_RECS -- instead of crashing or hanging.
#
# These are validated by page_simple_validate_new(), which xtrabackup's
# page_validate() runs for every B-tree page (just after the rec chain
# pre-check):
#   * every user record's origin must be at or below PAGE_HEAP_TOP, and the
#     record heap must not overlap the page directory ("Record N is above rec
#     heap top ..."); and
#   * PAGE_N_RECS must equal the number of records on the page reached by
#     walking infimum -> ... -> supremum ("n recs wrong ...").
# A violation must be reported as corruption and exit non-zero, with no
# ib::fatal abort and no tablespace extend/hang.
#
# The corruption is applied to a single header field and the stored checksum
# is NOT recomputed, so the run uses --innodb-checksum-algorithm=none: that is
# what forces the page through the structural validators rather than tripping
# the checksum-on-read abort first (a separate, already-covered path).
#
# This is a regression guard for the page_simple_validate_new() coverage under
# --check-tables; it fails if that validation is bypassed (crash/hang) or
# silently passes a structurally inconsistent page.
#
# within-page byte offsets (PAGE_HEADER = FIL_PAGE_DATA = 38):
#   PAGE_N_DIR_SLOTS = 38 + 0  = 38
#   PAGE_HEAP_TOP    = 38 + 2  = 40
#   PAGE_N_RECS      = 38 + 16 = 54
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

vlog "Locate a clustered-index leaf page (level 0)"
read MAXLEVEL LEAF L1 L2 <<< "$(find_clustered_pages_by_level "$IBD")"
[ "$LEAF" != "NONE" ] || die "could not locate a clustered leaf page"
NRECS=$(mach_read_2 "$IBD" "$LEAF" 54)
HEAPTOP=$(mach_read_2 "$IBD" "$LEAF" 40)
vlog "leaf page $LEAF : PAGE_N_RECS=$NRECS PAGE_HEAP_TOP=$HEAPTOP"
[ "$NRECS" -gt 1 ] || die "leaf page $LEAF has too few records ($NRECS) for the test"

# run_check_tables_corrupt <dir> <tag> <expected-corruption-regex>
#   Run --check-tables on <dir> under a timeout (with checksums off so the
#   structural validators are reached), then assert the run was graceful:
#   no abort, no hang, no tablespace extend, corruption reported, non-zero exit.
run_check_tables_corrupt() {
  local DIR="$1" TAG="$2" RE="$3"
  local SIZE_BEFORE SIZE_AFTER RC
  SIZE_BEFORE=$(stat -c %s "$DIR/test/t1.ibd")

  set +e
  timeout 120 $XB_BIN $XB_ARGS --prepare --check-tables \
    --innodb-checksum-algorithm=none \
    --target-dir="$DIR" 2>&1 | tee $topdir/$TAG.log
  RC=${PIPESTATUS[0]}
  set -e
  vlog "$TAG: exit code $RC"

  SIZE_AFTER=$(stat -c %s "$DIR/test/t1.ibd")

  [ "$RC" -ne 124 ] || die "$TAG: --check-tables HUNG (timeout) on corrupt page header"
  grep -qiE "Assertion failure|got signal|ib::fatal triggered" $topdir/$TAG.log && \
    die "$TAG: --check-tables aborted instead of reporting corruption gracefully"
  grep -qi "posix_fallocate" $topdir/$TAG.log && \
    die "$TAG: --check-tables attempted a tablespace extension"
  [ "$SIZE_AFTER" = "$SIZE_BEFORE" ] || \
    die "$TAG: backup .ibd grew during --check-tables ($SIZE_BEFORE -> $SIZE_AFTER)"
  [ "$RC" -ne 0 ] || die "$TAG: --check-tables passed a corrupt page header"
  grep -qiE "$RE" $topdir/$TAG.log || \
    die "$TAG: expected validator message /$RE/ not found"
  grep -q "is corrupted" $topdir/$TAG.log || \
    die "$TAG: corruption not reported (\"is corrupted\")"
  grep -q "Table check failed" $topdir/$TAG.log || \
    die "$TAG: \"Table check failed\" not reported"
  vlog "$TAG passed: corruption reported gracefully (rc=$RC)"
}

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
# Negative 1: PAGE_HEAP_TOP below the user-record area -> records sit above the
# (bogus) heap top.
#
vlog "=== Negative: corrupt PAGE_HEAP_TOP on leaf page $LEAF ==="
cp -r $topdir/backup $topdir/backup_heap
mach_write_2 "$topdir/backup_heap/test/t1.ibd" "$LEAF" 40 100
run_check_tables_corrupt "$topdir/backup_heap" heaptop "above rec heap top|heap.*overlap"

#
# Negative 2: PAGE_N_RECS disagreeing with the walked record count.
#
vlog "=== Negative: corrupt PAGE_N_RECS on leaf page $LEAF ($NRECS -> $((NRECS + 1))) ==="
cp -r $topdir/backup $topdir/backup_nrecs
mach_write_2 "$topdir/backup_nrecs/test/t1.ibd" "$LEAF" 54 $((NRECS + 1))
run_check_tables_corrupt "$topdir/backup_nrecs" nrecs "n recs wrong"

vlog "All page_header_heap_top_nrecs sub-tests passed"
