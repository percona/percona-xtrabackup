############################################################################
# PXB-3804 : --check-tables must report (not crash, not silently pass) a page
# whose FIL_PAGE_TYPE has been masked to FIL_PAGE_TYPE_ALLOCATED (0) -- at
# EVERY B-tree level: leaf (level 0), middle/non-leaf (level 1), and the
# upper levels (level 2 ...).
#
# Issue 7 (pxb_3804_issue7.sh) only covered the ROOT page.  The remaining
# levels behave differently without a per-page guard:
#   - a corrupt internal (non-leaf) page aborts the father-pointer search in
#     btr_cur_search_to_nth_level()  (ut_ad(fil_page_index_page_check(page))),
#   - a corrupt leaf SILENTLY PASSES (nothing else checks a leaf's page type).
#
# This verifies the per-page fil_page_index_page_check() gate in
# btr_validate_level() handles all levels gracefully.
#
# A wide PRIMARY KEY + wide row force a multi-level (>=3) clustered B-tree so
# that a genuine non-root, non-leaf middle page exists.  A small Python helper
# locates a clustered-index page at each level; the page's FIL_PAGE_TYPE is
# then set to 0 and --check-tables is run (with --innodb-checksum-algorithm=
# none so the page is accepted past the checksum layer).
############################################################################

. inc/common.sh


start_server --innodb_file_per_table

vlog "Create a wide-key/wide-row table to force a >=3-level clustered B-tree"
mysql test <<'EOF'
SET SESSION cte_max_recursion_depth = 100000;
CREATE TABLE t (k VARCHAR(255) NOT NULL PRIMARY KEY, pad VARCHAR(4000) NOT NULL)
  ROW_FORMAT=DYNAMIC;
INSERT INTO t
WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 6000)
SELECT LPAD(n, 250, '0'), REPEAT('x', 4000) FROM seq;
EOF

vlog "Full backup + apply-log-only prepare"
xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup

IBD=$topdir/backup/test/t.ibd

vlog "Locate a clustered-index page at each level (0=leaf, 1=middle, 2=upper)"
read MAXLEVEL L0 L1 L2 <<< "$(find_clustered_pages_by_level "$IBD")"
vlog "B-tree layout: MAXLEVEL=$MAXLEVEL L0=$L0 L1=$L1 L2=$L2"

[ "$MAXLEVEL" -ge 2 ] || \
  die "clustered B-tree is only $((MAXLEVEL + 1)) levels deep ($LEVELS); need >=3 (increase row count)"
for V in "$L0" "$L1" "$L2"; do
  [ "$V" != "NONE" ] || die "could not find a page at some level: $LEVELS"
done
vlog "Using pages: leaf(level0)=$L0  middle(level1)=$L1  upper(level2)=$L2"

corrupt_and_check() {
  local LEVEL=$1 PAGE=$2
  vlog "=== Level $LEVEL: corrupt FIL_PAGE_TYPE -> 0 (ALLOCATED) on page $PAGE ==="
  rm -rf $topdir/b_$LEVEL
  cp -r $topdir/backup $topdir/b_$LEVEL
  # FIL_PAGE_TYPE is 2 bytes at offset 24; set to 0 (FIL_PAGE_TYPE_ALLOCATED)
  mach_write_2 "$topdir/b_$LEVEL/test/t.ibd" "$PAGE" 24 0

  run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
    --innodb-checksum-algorithm=none \
    --target-dir=$topdir/b_$LEVEL 2>&1 | tee $topdir/check_$LEVEL.log

  if grep -qiE "Assertion failure|got signal|intentionally generate a memory trap" \
       $topdir/check_$LEVEL.log; then
    die "Level $LEVEL: xtrabackup CRASHED on a non-index FIL_PAGE_TYPE (PXB-3804)"
  fi
  grep -q "Starting table checks" $topdir/check_$LEVEL.log || \
    die "Level $LEVEL: check-tables did not start"
  grep -qiE "non-index FIL_PAGE_TYPE|is corrupted" $topdir/check_$LEVEL.log || \
    die "Level $LEVEL: corruption not reported (silent pass?)"
  grep -q "Table check failed" $topdir/check_$LEVEL.log || \
    die "Level $LEVEL: 'Table check failed' message not found"
  vlog "Level $LEVEL passed: non-index FIL_PAGE_TYPE reported gracefully, no crash"
}

corrupt_and_check 0 "$L0"   # leaf
corrupt_and_check 1 "$L1"   # middle / non-leaf, non-root
corrupt_and_check 2 "$L2"   # upper level (root if 3-level tree)

vlog "All levels (0=leaf, 1=middle, 2=upper) reported gracefully, no crash"
