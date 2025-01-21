KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh

require_debug_pxb_version
require_pro_pxb_version

function run_test() {
  ALL_TABLES_IN_BACKUP=$1

  vlog "Running test with ALL_TABLES_IN_BACKUP=$ALL_TABLES_IN_BACKUP"

  configure_server_with_component

  if [ "$ALL_TABLES_IN_BACKUP" = "true" ]; then
    $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.enc_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.enc_table VALUES (), (), (), ();" test
    innodb_wait_for_flush_all
  fi

  xtrabackup --backup --target-dir=$topdir/backup_new_table \
    --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
    2> >( tee $topdir/backup_with_new_table.log)&

  job_pid=$!
  pid_file=$topdir/backup_new_table/xtrabackup_debug_sync
  wait_for_xb_to_suspend $pid_file
  xb_pid=`cat $pid_file`
  echo "backup pid is $job_pid"

  if [ "$ALL_TABLES_IN_BACKUP" = "false" ]; then
    $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.enc_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.enc_table VALUES (), (), (), ();" test
    innodb_wait_for_flush_all
  fi

  # Encrypt table
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.enc_table ENCRYPTION='Y';" test
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.enc_table ENCRYPTION='N';" test
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.enc_table ENCRYPTION='Y';" test

  # Resume the xtrabackup process
  vlog "Resuming xtrabackup"
  kill -SIGCONT $xb_pid
  run_cmd wait $job_pid

  xtrabackup --prepare --target-dir=$topdir/backup_new_table --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
  record_db_state test
  stop_server
  rm -rf $mysql_datadir/*
  xtrabackup --copy-back --target-dir=$topdir/backup_new_table --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
  cp ${instance_local_manifest}  $mysql_datadir
  cp ${keyring_component_cnf} $mysql_datadir
  start_server
  verify_db_state test
  stop_server
  rm -rf $mysql_datadir $topdir/backup_with_new_table.log $topdir/backup_new_table
}

run_test true

run_test false
