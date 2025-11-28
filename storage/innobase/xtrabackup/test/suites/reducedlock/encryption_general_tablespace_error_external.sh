KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh

require_debug_pxb_version
require_debug_sync_thread

function run_test() {
  ALL_TABLES_IN_BACKUP=$1

  vlog "Running test with ALL_TABLES_IN_BACKUP=$ALL_TABLES_IN_BACKUP"

  configure_server_with_component
  mkdir $topdir/external

  stop_server
  start_server --innodb_directories=$topdir/external/

  XB_ERROR_LOG=$topdir/backup_with_enc_general_tablespace_external.log

  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLESPACE ts1 ADD DATAFILE '$topdir/external/ts1.ibd' Engine=InnoDB ENCRYPTION = 'Y';CREATE TABLE test.enc_table (id INT PRIMARY KEY AUTO_INCREMENT) TABLESPACE ts1 ENCRYPTION='Y'; INSERT INTO test.enc_table VALUES (), (), (), ();" test
  innodb_wait_for_flush_all

	xtrabackup_background --backup --target-dir=$topdir/backup_enc_general_tablespace_external --debug-sync-thread="before_file_copy" --lock-ddl=REDUCED

  job_pid=$XB_PID

	wait_for_debug_sync_thread "before_file_copy"

	echo "Now pause redo thread"
	echo "xtrabackup_copy_logfile_pause" > $topdir/backup_enc_general_tablespace_external/xb_debug_sync_thread
	kill -SIGUSR1 $job_pid

	wait_for_debug_sync_thread "xtrabackup_copy_logfile_pause"

  echo "Now re-encrypt the tablespace Y->N-Y"
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLESPACE ts1 ENCRYPTION='N';ALTER TABLESPACE ts1 ENCRYPTION='Y';" test

  echo "Now resume copying thread"
	resume_debug_sync_thread  "before_file_copy" $topdir/backup_enc_general_tablespace_external

  echo "Now resume redo copy thread"
	resume_debug_sync_thread "xtrabackup_copy_logfile_pause" $topdir/backup_enc_general_tablespace_external

	wait $XB_PID
	exit_status=$?

  # Check the exit status and take appropriate action
  if [ $exit_status -eq 0 ]; then
      echo "xtrabackup_background exited successfully."
  else
      echo "xtrabackup_background exited with an error (exit status: $exit_status)."
			exit 1
  fi

  $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.enc_table VALUES (), (), (), ();" test
  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE t2(a INT); INSERT INTO t2 VALUES (1),(2),(3),(4),(5);" test
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLESPACE mysql ENCRYPTION='Y';" test

  XB_ERROR_LOG=$topdir/backup_inc.log
	BACKUP_DIR=$topdir/backup_inc_external
	xtrabackup_background --backup --target-dir=$topdir/backup_inc_external --incremental-basedir=$topdir/backup_enc_general_tablespace_external --lock-ddl=REDUCED  --debug-sync-thread="before_file_copy"

	wait_for_debug_sync_thread "before_file_copy"

	echo "Now pause redo thread"
	echo "xtrabackup_copy_logfile_pause" > $BACKUP_DIR/xb_debug_sync_thread
	kill -SIGUSR1 $XB_PID

	wait_for_debug_sync_thread "xtrabackup_copy_logfile_pause"

  echo "Now re-encrypt the tablespace Y->N-Y"
  $MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLESPACE mysql ENCRYPTION='N';ALTER TABLESPACE mysql ENCRYPTION='Y';" test

  echo "Now resume copying thread"
	resume_debug_sync_thread  "before_file_copy" $BACKUP_DIR

  echo "Now resume redo copy thread"
	resume_debug_sync_thread "xtrabackup_copy_logfile_pause" $BACKUP_DIR

	echo "################reached waitpid#######################"
	wait $XB_PID
	exit_status=$?

  # Check the exit status and take appropriate action
  if [ $exit_status -eq 0 ]; then
      echo "xtrabackup_background exited successfully."
  else
      echo "xtrabackup_background exited with an error (exit status: $exit_status)."
			exit 1
  fi

  record_db_state test
  stop_server

  xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_enc_general_tablespace_external --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
  xtrabackup --prepare --target-dir=$topdir/backup_enc_general_tablespace_external --incremental-dir=$topdir/backup_inc_external --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

  rm -rf $mysql_datadir/*
  rm -rf $topdir/external/*
  xtrabackup --copy-back --target-dir=$topdir/backup_enc_general_tablespace_external --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
  cp ${instance_local_manifest}  $mysql_datadir
  cp ${keyring_component_cnf} $mysql_datadir

  start_server
  verify_db_state test
  stop_server
  rm -rf $mysql_datadir $topdir/backup_with_enc_general_tablespace_external.log $topdir/backup_enc_general_tablespace_external $topdir/external $topdir/backup_inc_external
}

run_test true
