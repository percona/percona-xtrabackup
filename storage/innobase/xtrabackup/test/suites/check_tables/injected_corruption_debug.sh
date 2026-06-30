############################################################################
# Test --check-tables corruption detection via DBUG_EXECUTE_IF debug points.
# Requires a debug build of xtrabackup.
#
# Sub-tests:
#   A) check_table_break_sibling_link  -- broken FIL_PAGE_NEXT/PREV chain
#   B) check_table_wrong_index_id      -- page belongs to wrong index
#   C) check_table_set_wrong_min_bit   -- min-record flag on wrong record
#   D) check_table_inject_corruption   -- catch-all validation failure
#   E) simulate_lob_corruption         -- LOB corruption detection
############################################################################

. inc/common.sh

require_debug_pxb_version

start_server --innodb_file_per_table

vlog "Create table with enough rows for a multi-level B-tree"
mysql test <<EOF
CREATE TABLE t1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
CREATE TABLE t_lob (a INT PRIMARY KEY AUTO_INCREMENT, b LONGBLOB);
EOF

for i in $(seq 1 200); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t1 (b) VALUES (REPEAT('x', 200));"
done

for i in $(seq 1 10); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t_lob (b) VALUES (REPEAT('B', 100000));"
done

vlog "Take backup"
xtrabackup --backup --target-dir=$topdir/backup

#
# Sub-test A: Broken sibling link
#
vlog "=== Sub-test A: check_table_break_sibling_link ==="
cp -r $topdir/backup $topdir/backup_a

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --debug=d,check_table_break_sibling_link \
  --target-dir=$topdir/backup_a 2>&1 | tee $topdir/prepare_a.log

grep -q "broken FIL_PAGE_NEXT" $topdir/prepare_a.log || \
  grep -q "FIL_PAGE_PREV" $topdir/prepare_a.log || \
  die "Sub-test A: broken sibling link message not found"
grep -q "is corrupted" $topdir/prepare_a.log || \
  die "Sub-test A: corruption not detected"
grep -q "Table check failed" $topdir/prepare_a.log || \
  die "Sub-test A: Table check failed message not found"
vlog "Sub-test A passed"

#
# Sub-test B: Index ID mismatch
#
vlog "=== Sub-test B: check_table_wrong_index_id ==="
cp -r $topdir/backup $topdir/backup_b

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --debug=d,check_table_wrong_index_id \
  --target-dir=$topdir/backup_b 2>&1 | tee $topdir/prepare_b.log

grep -q "Page index id" $topdir/prepare_b.log || \
  die "Sub-test B: index ID mismatch message not found"
grep -q "is corrupted" $topdir/prepare_b.log || \
  die "Sub-test B: corruption not detected"
grep -q "Table check failed" $topdir/prepare_b.log || \
  die "Sub-test B: Table check failed message not found"
vlog "Sub-test B passed"

#
# Sub-test C: Min-record flag (existing debug point in page_validate)
#
vlog "=== Sub-test C: check_table_set_wrong_min_bit ==="
cp -r $topdir/backup $topdir/backup_c

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --debug=d,check_table_set_wrong_min_bit \
  --target-dir=$topdir/backup_c 2>&1 | tee $topdir/prepare_c.log

grep -q "is corrupted" $topdir/prepare_c.log || \
  die "Sub-test C: corruption not detected"
grep -q "Table check failed" $topdir/prepare_c.log || \
  die "Sub-test C: Table check failed message not found"
vlog "Sub-test C passed"

#
# Sub-test D: Catch-all injection
#
vlog "=== Sub-test D: check_table_inject_corruption ==="
cp -r $topdir/backup $topdir/backup_d

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --debug=d,check_table_inject_corruption \
  --target-dir=$topdir/backup_d 2>&1 | tee $topdir/prepare_d.log

grep -q "is corrupted" $topdir/prepare_d.log || \
  die "Sub-test D: corruption not detected"
grep -q "Table check failed" $topdir/prepare_d.log || \
  die "Sub-test D: Table check failed message not found"
vlog "Sub-test D passed"

#
# Sub-test E: LOB corruption (duplicate external LOB first page)
#
vlog "=== Sub-test E: simulate_lob_corruption ==="
cp -r $topdir/backup $topdir/backup_e

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --debug=d,simulate_lob_corruption \
  --target-dir=$topdir/backup_e 2>&1 | tee $topdir/prepare_e.log

grep -q "External LOB first page cannot be shared" $topdir/prepare_e.log || \
  die "Sub-test E: LOB duplicate message not found"
grep -q "is corrupted" $topdir/prepare_e.log || \
  die "Sub-test E: corruption not detected"
grep -q "Table check failed" $topdir/prepare_e.log || \
  die "Sub-test E: Table check failed message not found"
vlog "Sub-test E passed"

vlog "All debug sub-tests passed"
