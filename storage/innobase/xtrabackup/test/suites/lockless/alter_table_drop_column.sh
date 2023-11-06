
. inc/common.sh

require_debug_pxb_version
require_debug_server

function run_test() {
  ALL_TABLES_IN_BACKUP=$1
  INSTANT=$2

  vlog "Running test with ALL_TABLES_IN_BACKUP=$ALL_TABLES_IN_BACKUP and INSTANT=$INSTANT"

  start_server
  if [ "$ALL_TABLES_IN_BACKUP" = "false" ]; then
    $MYSQL $MYSQL_ARGS -Ns -e "SET GLOBAL innodb_checkpoint_disabled=true;"
  fi
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.keep (id INT PRIMARY KEY AUTO_INCREMENT, new_col VARCHAR(50), name VARCHAR(50)); INSERT INTO test.keep VALUES(1, 'a', 'a'), (2, 'b', 'b');" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.rename (id INT PRIMARY KEY AUTO_INCREMENT, new_col VARCHAR(50), name VARCHAR(50)); INSERT INTO test.rename VALUES(1, 'a', 'a'), (2, 'b', 'b');" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.drop (id INT PRIMARY KEY AUTO_INCREMENT, new_col VARCHAR(50), name VARCHAR(50)); INSERT INTO test.drop VALUES(1, 'a', 'a'), (2, 'b', 'b');" test


  if [ "$ALL_TABLES_IN_BACKUP" = "true" ]; then
    innodb_wait_for_flush_all
  fi


  xtrabackup --backup --target-dir=$topdir/backup_drop_column \
    --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
    2> >( tee $topdir/backup_with_drop_column.log)&

  job_pid=$!
  pid_file=$topdir/backup_drop_column/xtrabackup_debug_sync
  wait_for_xb_to_suspend $pid_file
  xb_pid=`cat $pid_file`
  echo "backup pid is $job_pid"

  # Drop column from the table and generate redo
  QUERY="ALTER TABLE test.keep DROP COLUMN new_col, ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
  else
    QUERY+="INPLACE"
  fi

  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.keep VALUES (3, 'c'), (4, 'd');" test

  # Drop column from the table, generate redo, rename the table and generate redo
  QUERY="ALTER TABLE test.rename DROP COLUMN new_col, ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
  else
    QUERY+="INPLACE"
  fi

  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.rename VALUES (3, 'c'), (4, 'd');" test
  $MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.rename TO test.after_rename; INSERT INTO test.after_rename VALUES (5, 'e'), (6, 'f');" test

  # Drop column from the table, generate redo and drop the table
  QUERY="ALTER TABLE test.drop DROP COLUMN new_col, ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
    else
    QUERY+="INPLACE"
  fi
  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.drop VALUES (3, 'c'), (4, 'd'); DROP TABLE test.drop;" test

  # Create table, generate redo and drop column
  QUERY="CREATE TABLE test.new (id INT PRIMARY KEY AUTO_INCREMENT, new_col VARCHAR(50), name VARCHAR(50)); INSERT INTO test.new VALUES(1, 'a', 'a'), (2, 'b', 'b'); ALTER TABLE test.new DROP COLUMN new_col, ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
  else
    QUERY+="INPLACE"
  fi
  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.new VALUES (3, 'c'), (4, 'd');" test

  # Resume the xtrabackup process
  vlog "Resuming xtrabackup"
  kill -SIGCONT $xb_pid
  run_cmd wait $job_pid

  xtrabackup --prepare --target-dir=$topdir/backup_drop_column
  record_db_state test
  stop_server
  rm -rf $mysql_datadir/*
  xtrabackup --copy-back --target-dir=$topdir/backup_drop_column
  start_server
  verify_db_state test

  stop_server
  rm -rf $mysql_datadir $topdir/backup_with_drop_column.log $topdir/backup_drop_column
}


# Run test with all tables in backup and algortihm inplace
run_test true false

# Run test with all tables in backup and algortihm instant
run_test true true

# Run test with all tables in redo and algortihm inplace
run_test false false

# Run test with all tables in redo and algortihm instant
run_test false true

