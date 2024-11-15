###############################################################################
# PXB-3034: Reduce the time the instance remain under lock
###############################################################################

. inc/common.sh

require_debug_pxb_version
require_pro_pxb_version

function run_test() {
  local TEST_TYPE=$1

  start_server
  vlog "Running test with ${TEST_TYPE}"
  if [[ $TEST_TYPE = "special_char" ]]; then
    DELETE_TABLE="delete_Ѭtable"
    ORIGINAL_TABLE="original_Ѭtable"
    RENAMED_TABLE="renamed_Ѭtable"
    OP_DDL_TABLE="op_Ѭddl"
    NEW_TABLE="new_Ѭtable"
    DELETE_TABLE_IN_DISK="delete_@U2table"
    ORIGINAL_TABLE_IN_DISK="original_@U2table"
    RENAMED_TABLE_IN_DISK="renamed_@U2table"
    NEW_TABLE_IN_DISK="new_@U2table"
  elif [[ $TEST_TYPE = "normal" ]]; then
    DELETE_TABLE="delete_table"
    ORIGINAL_TABLE="original_table"
    RENAMED_TABLE="renamed_table"
    OP_DDL_TABLE="op_ddl"
    NEW_TABLE="new_table"
    DELETE_TABLE_IN_DISK="delete_table"
    ORIGINAL_TABLE_IN_DISK="original_table"
    RENAMED_TABLE_IN_DISK="renamed_table"
    NEW_TABLE_IN_DISK="new_table"
  fi
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.${DELETE_TABLE} (id INT PRIMARY KEY AUTO_INCREMENT);" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.${ORIGINAL_TABLE} (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.${ORIGINAL_TABLE} VALUES(1)" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.${OP_DDL_TABLE} (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.${OP_DDL_TABLE} VALUES(1, 'test')" test

  innodb_wait_for_flush_all

  xtrabackup --backup --target-dir=$topdir/backup_new_table \
    --debug-sync="xtrabackup_load_tablespaces_pause" --lock-ddl=REDUCED \
    2> >( tee $topdir/backup_with_new_table.log)&

  job_pid=$!
  pid_file=$topdir/backup_new_table/xtrabackup_debug_sync
  wait_for_xb_to_suspend $pid_file
  xb_pid=`cat $pid_file`
  echo "backup pid is $job_pid"

  # Generate redo on table than delete it
  $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.${DELETE_TABLE} VALUES (1); DROP TABLE test.${DELETE_TABLE};" test

  # Create table and generate redo
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.${NEW_TABLE} (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.${NEW_TABLE} VALUES (), ();" test

  # Rename table and generate redo
  $MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.${ORIGINAL_TABLE} TO test.${RENAMED_TABLE}; INSERT INTO test.${RENAMED_TABLE} VALUES (2);" test

  # Bulk Index Load and generate redo
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.${OP_DDL_TABLE} ADD INDEX(name); INSERT INTO test.${OP_DDL_TABLE} VALUES (2, 'test2');" test

  # Resume the xtrabackup process
  vlog "Resuming xtrabackup"
  kill -SIGCONT $xb_pid
  run_cmd wait $job_pid

  if ! egrep -q "DDL tracking : LSN: [0-9]* delete space ID: [0-9]* Name: test/${DELETE_TABLE_IN_DISK}.ibd" $topdir/backup_with_new_table.log ; then
     die "xtrabackup did not handle delete table DDL"
  fi

  if ! egrep -q "DDL tracking : LSN: [0-9]* create space ID: [0-9]* Name: test/${NEW_TABLE_IN_DISK}.ibd" $topdir/backup_with_new_table.log ; then
     die "xtrabackup did not handle new table DDL"
  fi

  if ! egrep -q "DDL tracking : LSN: [0-9]* rename space ID: [0-9]* From: test/${ORIGINAL_TABLE_IN_DISK}.ibd To: test/${RENAMED_TABLE_IN_DISK}.ibd" $topdir/backup_with_new_table.log ; then
     die "xtrabackup did not handle rename table DDL"
  fi

  if ! egrep -q "DDL tracking : LSN: [0-9]* add index on space ID: [0-9]*" $topdir/backup_with_new_table.log ; then
     die "xtrabackup did not handle Bulk Index Load DDL"
  fi

  xtrabackup --prepare --target-dir=$topdir/backup_new_table
  record_db_state test
  stop_server
  rm -rf $mysql_datadir/*
  xtrabackup --copy-back --target-dir=$topdir/backup_new_table
  start_server
  verify_db_state test
  stop_server
  rm -rf $mysql_datadir $topdir/backup_new_table
}

run_test normal
run_test special_char
