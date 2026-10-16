###############################################################################
# Replacing an Undo tablespace between backups exercises the shared incremental
# rename path. A nonzero InnoDB error must not be treated as success.
###############################################################################

. inc/common.sh
require_debug_pxb_version
require_server_version_higher_than 8.0.13
start_server

$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.t1(id INT PRIMARY KEY) ENGINE=InnoDB;
  INSERT INTO test.t1 VALUES(1),(2);
  CREATE UNDO TABLESPACE undo_error_test ADD DATAFILE 'undo_error_test.ibu';"
old_id=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT SPACE FROM information_schema.innodb_tablespaces WHERE NAME='undo_error_test'")
xtrabackup --backup --target-dir="$topdir/base" --lock-ddl=ON

$MYSQL $MYSQL_ARGS -e "ALTER UNDO TABLESPACE undo_error_test SET INACTIVE;
  SET GLOBAL innodb_purge_rseg_truncate_frequency=1;"
state=
for attempt in $(seq 1 120); do
  state=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT STATE FROM information_schema.innodb_tablespaces WHERE NAME='undo_error_test'")
  [ "$state" = empty ] && break
  sleep 1
done
[ "$state" = empty ] || die "Undo tablespace did not become empty"
$MYSQL $MYSQL_ARGS -e "DROP UNDO TABLESPACE undo_error_test;
  CREATE UNDO TABLESPACE undo_error_test ADD DATAFILE 'undo_error_test.ibu';
  INSERT INTO test.t1 VALUES(3);"
new_id=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT SPACE FROM information_schema.innodb_tablespaces WHERE NAME='undo_error_test'")
[ "$old_id" != "$new_id" ] || die "Undo space ID did not change"
xtrabackup --backup --target-dir="$topdir/inc" --incremental-basedir="$topdir/base" --lock-ddl=ON

cp -a "$topdir/base" "$topdir/failed_base"
cp -a "$topdir/inc" "$topdir/failed_inc"
xtrabackup --prepare --apply-log-only --target-dir="$topdir/failed_base"
run_cmd_expect_failure "$XB_BIN" $XB_ARGS --prepare \
  --target-dir="$topdir/failed_base" --incremental-dir="$topdir/failed_inc" \
  --debug=d,fil_rename_tablespace_failure_2 >"$topdir/failed_prepare.log" 2>&1
if ! grep -q 'Cannot rename.*undo_error_test' "$topdir/failed_prepare.log"; then
  cat "$topdir/failed_prepare.log"
  die "incremental Undo rename error was not propagated"
fi
if grep -q 'completed OK!' "$topdir/failed_prepare.log"; then
  die "incremental prepare succeeded after Undo rename failed"
fi

xtrabackup --prepare --apply-log-only --target-dir="$topdir/base"
xtrabackup --prepare --target-dir="$topdir/base" --incremental-dir="$topdir/inc"
record_db_state test
stop_server
rm -rf "$mysql_datadir"
mkdir "$mysql_datadir"
xtrabackup --copy-back --target-dir="$topdir/base"
start_server
verify_db_state test
restored_id=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT SPACE FROM information_schema.innodb_tablespaces WHERE NAME='undo_error_test'")
[ "$new_id" = "$restored_id" ] || die "restored Undo space ID is incorrect"
stop_server
