
. inc/common.sh

require_debug_pxb_version
require_debug_server

function run_test() {
  ALL_TABLES_IN_BACKUP=$1
  vlog "Running test with ALL_TABLES_IN_BACKUP=$ALL_TABLES_IN_BACKUP"

  start_server
  if [ "$ALL_TABLES_IN_BACKUP" = "false" ]; then
    $MYSQL $MYSQL_ARGS -Ns -e "SET GLOBAL innodb_checkpoint_disabled=true;"
  fi
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.secondary_idx_keep (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50), KEY(name)); INSERT INTO test.secondary_idx_keep VALUES(1, 'a'), (2, 'b');" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.secondary_idx_rename (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50), KEY(name)); INSERT INTO test.secondary_idx_rename VALUES(1, 'a'), (2, 'b');" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.secondary_idx_drop (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50), KEY(name)); INSERT INTO test.secondary_idx_drop VALUES(1, 'a'), (2, 'b');" test

  if [ "$ALL_TABLES_IN_BACKUP" = "true" ]; then
    innodb_wait_for_flush_all
  fi


  xtrabackup --backup --target-dir=$topdir/backup_optimized_ddl \
    --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
    2> >( tee $topdir/backup_with_optimized_ddl.log)&

  job_pid=$!
  pid_file=$topdir/backup_optimized_ddl/xtrabackup_debug_sync
  wait_for_xb_to_suspend $pid_file
  xb_pid=`cat $pid_file`
  echo "backup pid is $job_pid"

  # Add secondary index
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.secondary_idx_keep DROP INDEX name;" test

  # Add secondary index and rename table
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.secondary_idx_rename DROP INDEX name; RENAME TABLE test.secondary_idx_rename TO test.secondary_idx_after_rename" test

  # Add secondary index and drop table
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.secondary_idx_drop DROP INDEX name; DROP TABLE test.secondary_idx_drop;" test

  # Create table and generate redo
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.secondary_idx_new (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50), KEY(name)); INSERT INTO test.secondary_idx_new VALUES(1, 'a'), (2, 'b'); ALTER TABLE test.secondary_idx_new DROP INDEX name;" test

  # Resume the xtrabackup process
  vlog "Resuming xtrabackup"
  kill -SIGCONT $xb_pid
  run_cmd wait $job_pid

  original_keep_table_row_count=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.secondary_idx_keep FORCE INDEX(name);" | awk {'print $1'}`
  original_rename_table_row_count=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.secondary_idx_after_rename FORCE INDEX(name);" | awk {'print $1'}`
  xtrabackup --prepare --target-dir=$topdir/backup_optimized_ddl
  record_db_state test
  stop_server
  rm -rf $mysql_datadir/*
  xtrabackup --copy-back --target-dir=$topdir/backup_optimized_ddl
  start_server
  verify_db_state test
  restored_keep_table_row_count=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.secondary_idx_keep FORCE INDEX(name);" | awk {'print $1'}`
  restored_rename_table_row_count=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.secondary_idx_after_rename FORCE INDEX(name);" | awk {'print $1'}`

  if [ "$original_keep_table_row_count" != "$restored_keep_table_row_count" ]; then
    die "rows in table secondary_idx_keep is $restored_keep_table_row_count when it should be $original_keep_table_row_count"
  fi

  if [ "$original_rename_table_row_count" != "$restored_rename_table_row_count" ]; then
    die "rows in table secondary_idx_after_rename is $restored_rename_table_row_count when it should be $original_rename_table_row_count"
  fi
  stop_server
  rm -rf $mysql_datadir $topdir/backup_with_optimized_ddl.log $topdir/backup_optimized_ddl
}


# Run test with all tables in backup
run_test true

# Run test ensure DDL for create tables are part of redo log entries
run_test false
