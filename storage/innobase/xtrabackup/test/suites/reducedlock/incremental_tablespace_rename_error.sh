###############################################################################
# Exercise both generic incremental-prepare rename branches. InnoDB returns a
# dberr_t here, so every non-DB_SUCCESS result must fail prepare.
###############################################################################

. inc/common.sh
require_debug_pxb_version
start_server

prepare_failure() {
  local base=$1 inc=$2 pattern=$3
  xtrabackup --prepare --apply-log-only --target-dir="$base"
  run_cmd_expect_failure "$XB_BIN" $XB_ARGS --prepare --target-dir="$base" \
    --incremental-dir="$inc" --debug=d,fil_rename_tablespace_failure_2 \
    >"$base.failure.log" 2>&1
  grep -q "$pattern" "$base.failure.log" || {
    cat "$base.failure.log"
    die "incremental rename failure was not propagated: $pattern"
  }
  if grep -q 'completed OK!' "$base.failure.log"; then
    die "incremental prepare reported success after rename failure"
  fi
}

# Same logical name, new space ID: move the old file out of the way.
$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.same_name(id INT PRIMARY KEY);
  INSERT INTO test.same_name VALUES(1)"
innodb_wait_for_flush_all
xtrabackup --backup --target-dir="$topdir/conflict_base" --lock-ddl=ON
$MYSQL $MYSQL_ARGS -e "DROP TABLE test.same_name;
  CREATE TABLE test.same_name(id INT PRIMARY KEY);
  INSERT INTO test.same_name VALUES(2),(3)"
xtrabackup --backup --target-dir="$topdir/conflict_inc" \
  --incremental-basedir="$topdir/conflict_base" --lock-ddl=ON
cp -a "$topdir/conflict_base" "$topdir/conflict_failed_base"
cp -a "$topdir/conflict_inc" "$topdir/conflict_failed_inc"
prepare_failure "$topdir/conflict_failed_base" "$topdir/conflict_failed_inc" \
  'Cannot rename test/same_name to test/xtrabackup_tmp_'

# Same space ID, new logical name: move the old-name base file to the delta's
# final name.
$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.rename_from(id INT PRIMARY KEY);
  INSERT INTO test.rename_from VALUES(4),(5)"
innodb_wait_for_flush_all
xtrabackup --backup --target-dir="$topdir/rename_base" --lock-ddl=ON
$MYSQL $MYSQL_ARGS -e "RENAME TABLE test.rename_from TO test.rename_to;
  INSERT INTO test.rename_to VALUES(6)"
xtrabackup --backup --target-dir="$topdir/rename_inc" \
  --incremental-basedir="$topdir/rename_base" --lock-ddl=ON
cp -a "$topdir/rename_base" "$topdir/rename_failed_base"
cp -a "$topdir/rename_inc" "$topdir/rename_failed_inc"
prepare_failure "$topdir/rename_failed_base" "$topdir/rename_failed_inc" \
  'Cannot rename test/rename_from to test/rename_to'

# Both unmodified copies must still prepare normally.
xtrabackup --prepare --apply-log-only --target-dir="$topdir/conflict_base"
xtrabackup --prepare --target-dir="$topdir/conflict_base" \
  --incremental-dir="$topdir/conflict_inc"
xtrabackup --prepare --apply-log-only --target-dir="$topdir/rename_base"
xtrabackup --prepare --target-dir="$topdir/rename_base" \
  --incremental-dir="$topdir/rename_inc"
stop_server
