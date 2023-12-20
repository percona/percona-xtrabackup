
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
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.keep (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.keep VALUES(1, 'a'), (2, 'b');" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.rename (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.rename VALUES(1, 'a'), (2, 'b');" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.drop (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.drop VALUES(1, 'a'), (2, 'b');" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.default_val (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.default_val VALUES(1, 'a'), (2, 'b');" test


  if [ "$ALL_TABLES_IN_BACKUP" = "true" ]; then
    innodb_wait_for_flush_all
  fi


  xtrabackup --backup --target-dir=$topdir/backup_add_column \
    --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
    2> >( tee $topdir/backup_with_add_column.log)&

  job_pid=$!
  pid_file=$topdir/backup_add_column/xtrabackup_debug_sync
  wait_for_xb_to_suspend $pid_file
  xb_pid=`cat $pid_file`
  echo "backup pid is $job_pid"

  # change default value and generate redo
  QUERY="ALTER TABLE test.default_val ALTER COLUMN name SET DEFAULT 'no-name', ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
  else
    QUERY+="INPLACE"
  fi

  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.default_val VALUES (3, 'c'), (4, 'd');" test

  # Add new column to table and generate redo
  QUERY="ALTER TABLE test.keep ADD COLUMN new_col VARCHAR(50) AFTER id, ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
  else
    QUERY+="INPLACE"
  fi

  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.keep VALUES (3, 'col', 'c'), (4, 'col', 'd');" test

  # Add new column to table,  generate redo, rename the table and generate redo
  QUERY="ALTER TABLE test.rename ADD COLUMN new_col VARCHAR(50) AFTER id, ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
  else
    QUERY+="INPLACE"
  fi

  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.rename VALUES (3, 'col', 'c'), (4, 'col', 'd');" test
  $MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.rename TO test.after_rename; INSERT INTO test.after_rename VALUES (5, 'col', 'e'), (6, 'col', 'f');" test

  # Add new column to table,  generate redo, drop the table
  QUERY="ALTER TABLE test.drop ADD COLUMN new_col VARCHAR(50) AFTER id, ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
    else
    QUERY+="INPLACE"
  fi
  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.drop VALUES (3, 'col', 'c'), (4, 'col', 'd'); DROP TABLE test.drop;" test

  # Create table, generate redo and add column
  QUERY="CREATE TABLE test.new (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.new VALUES(1, 'a'), (2, 'b'); ALTER TABLE test.new ADD COLUMN new_col VARCHAR(50) AFTER id, ALGORITHM="
  if [ "$INSTANT" = "true" ]; then
    QUERY+="INSTANT"
  else
    QUERY+="INPLACE"
  fi
  $MYSQL $MYSQL_ARGS -Ns -e "${QUERY} ; INSERT INTO test.new VALUES (3, 'col', 'c'), (4, 'col', 'd');" test

  # Resume the xtrabackup process
  vlog "Resuming xtrabackup"
  kill -SIGCONT $xb_pid
  run_cmd wait $job_pid

  xtrabackup --prepare --target-dir=$topdir/backup_add_column
  record_db_state test
  stop_server
  rm -rf $mysql_datadir/*
  xtrabackup --copy-back --target-dir=$topdir/backup_add_column
  start_server
  verify_db_state test

  stop_server
  rm -rf $mysql_datadir $topdir/backup_with_add_column.log $topdir/backup_add_column
}


# Run test with all tables in backup and algortihm inplace
run_test true false

# Run test with all tables in backup and algortihm instant
run_test true true

# Run test with all tables in redo and algortihm inplace
run_test false false

# Run test with all tables in redo and algortihm instant
run_test false true

