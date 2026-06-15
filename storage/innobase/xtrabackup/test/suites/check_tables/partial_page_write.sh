############################################################################
# PXB-3804 : --check-tables must gracefully reject a partially-written
# (torn) index page instead of aborting.
#
# Real-world trigger: a write interrupted mid-page (network/stream cut,
# crash during flush) leaves the first part of the page body overwritten.
# Simulated here by filling the first half of a clustered LEAF page body with
# 0xFF. That clobbers, in one shot:
#   * the index header -- including PAGE_LEVEL (abs offset 64), set to 0xFFFF,
#   * infimum/supremum system records,
#   * the first user records.
#
# Why this crashed before the fix: btr_validate_level()'s B-tree DESCENT
# reads a page's level (btr_page_get_level) and parses its first node pointer
# (rec_get_offsets) BEFORE page_validate() runs on it. With PAGE_LEVEL =
# 0xFFFF the leaf is mistaken for an internal page, and:
#   * debug build  -> btr_page_get_level() aborts on
#                     ut_ad(level <= BTR_MAX_NODE_LEVEL)   (btr0btr.ic)
#   * release build-> the bogus level is accepted, descent keeps going and
#                     rec_get_offsets() aborts on "default: ut_error" (rec.cc).
# Either way xtrabackup dies with a fatal signal instead of reporting damage.
#
# The fix stops btr_page_get_level() from asserting on the level under
# XTRABACKUP and adds btr_page_level_is_sane(); the descent guards (root and
# each child) and page_validate() use it to reject an out-of-range PAGE_LEVEL,
# so the corruption is reported and --check-tables exits non-zero with no abort.
#
# Two runs, to show the checksum algorithm does not matter:
#   A) --innodb-checksum-algorithm=none  : checksum skipped, corruption
#      reaches the B-tree validator directly.
#   B) recompute a VALID CRC32 checksum after corrupting, then run with the
#      DEFAULT algorithm: the page passes checksum validation and STILL
#      reaches the validator.
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

vlog "Create a table large enough for a multi-level clustered B-tree"
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
read MAXLEVEL LEAF L1 L2 <<< "$(find_clustered_pages_by_level "$IBD")"
[ "$LEAF" != "NONE" ] || die "could not locate a clustered leaf page"
[ "$MAXLEVEL" -ge 1 ] || die "clustered index is single-level; need a node-ptr level to exercise descent"
MID=$(( (38 + PAGE_SIZE) / 2 ))
vlog "page_size=$PAGE_SIZE maxlevel=$MAXLEVEL target leaf=$LEAF, 0xFF bytes 38..$MID"

# run_check_tables <dir> <logfile> [extra args...]
# Runs --prepare --check-tables and asserts a graceful corruption report
# (non-zero exit, no abort, no .ibd growth).
run_check_tables() {
  local dir=$1 log=$2; shift 2
  local before after rc
  before=$(stat -c %s "$dir/test/t1.ibd")
  set +e
  timeout 120 $XB_BIN $XB_ARGS --prepare --check-tables "$@" \
    --target-dir="$dir" 2>&1 | tee "$log"
  rc=${PIPESTATUS[0]}
  set -e
  after=$(stat -c %s "$dir/test/t1.ibd")
  vlog "check-tables exit code: $rc"
  [ "$rc" -ne 124 ] || die "partial_page_write: --check-tables HUNG"
  grep -qiE "Assertion failure|got signal|ut_error|ib::fatal triggered" "$log" && \
    die "partial_page_write: --check-tables ABORTED (crash not handled gracefully)"
  [ "$after" = "$before" ] || \
    die "partial_page_write: .ibd grew during --check-tables ($before -> $after)"
  [ "$rc" -ne 0 ] || die "partial_page_write: --check-tables passed a corrupted page"
  grep -qiE "B-tree corruption|out-of-range|invalid record chain|Table check failed" "$log" || \
    die "partial_page_write: corruption not reported"
}

#
# Control: a clean backup must pass --check-tables.
#
vlog "=== Control: clean --check-tables ==="
cp -r $topdir/backup $topdir/backup_ctrl
xtrabackup --prepare --check-tables --target-dir=$topdir/backup_ctrl 2>&1 \
  | tee $topdir/ctrl.log
grep -q "All table checks passed" $topdir/ctrl.log || \
  die "Control: clean backup unexpectedly failed --check-tables"
vlog "Control passed"

#
# Variant A: corrupt + checksum=none.
#
vlog "=== Variant A: partial write, --innodb-checksum-algorithm=none ==="
cp -r $topdir/backup $topdir/backup_a
fill_page_bytes "$topdir/backup_a/test/t1.ibd" "$LEAF" 38 "$MID" 0xFF
run_check_tables "$topdir/backup_a" "$topdir/a.log" \
  --innodb-checksum-algorithm=none
vlog "Variant A passed"

#
# Variant B: corrupt + recompute a VALID checksum, default algorithm.
#
vlog "=== Variant B: partial write + valid CRC32, default checksum algo ==="
cp -r $topdir/backup $topdir/backup_b
CIBD=$topdir/backup_b/test/t1.ibd
fill_page_bytes "$CIBD" "$LEAF" 38 "$MID" 0xFF
NEWCHK=$(update_page_checksum "$CIBD" "$LEAF")
vlog "recomputed page $LEAF checksum -> $NEWCHK"
# No --innodb-checksum-algorithm: the page passes checksum validation and
# still reaches the B-tree validator.
run_check_tables "$topdir/backup_b" "$topdir/b.log"
grep -qi "checksum" $topdir/b.log && \
  vlog "note: a checksum message appeared (inspect $topdir/b.log)" || true
vlog "Variant B passed"

vlog "partial_page_write passed: torn page reported gracefully on both paths"
