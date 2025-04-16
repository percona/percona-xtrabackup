require_debug_pxb_version
require_pro_pxb_version
require_debug_sync_thread

start_server

  XB_ERROR_LOG=$topdir/backup_with_enc_general_tablespace.log

  $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.t1 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.t1 VALUES (), (), (), ();" test

  innodb_wait_for_flush_all

  xtrabackup_background --backup --target-dir=$topdir/backup_enc_general_tablespace --debug-sync-thread="xtrabackup_suspend_between_file_discovery_and_open" --debug-sync="xtrabackup_pause_after_redo_catchup" --lock-ddl=REDUCED
  job_pid=$XB_PID

  pid_file=$topdir/backup_enc_general_tablespace/xtrabackup_debug_sync
  wait_for_xb_to_suspend $pid_file


  $MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.t1 to test.t2" test

  kill -SIGCONT $job_pid
  sleep 3

 wait_for_debug_sync_thread "xtrabackup_suspend_between_file_discovery_and_open"

 $MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.t2 to test.t3" test

  echo "Now resume file discover"
  resume_debug_sync_thread  "xtrabackup_suspend_between_file_discovery_and_open" $topdir/backup_enc_general_tablespace

   wait $XB_PID
   exit_status=$?

  # Check the exit status and take appropriate action
  if [ $exit_status -eq 0 ]; then
      echo "xtrabackup_background exited successfully."
  else
      echo "xtrabackup_background exited with an error (exit status: $exit_status)."
			exit 1
  fi

  xtrabackup --prepare --target-dir=$topdir/backup_enc_general_tablespace --xtrabackup-plugin-dir=${plugin_dir}

 record_db_state test
 stop_server

 rm -rf $mysql_datadir/*
 xtrabackup --copy-back --target-dir=$topdir/backup_enc_general_tablespace --xtrabackup-plugin-dir=${plugin_dir}

 start_server
 verify_db_state test
 stop_server
 rm -rf $mysql_datadir $topdir/backup_with_enc_general_tablespace.log $topdir/backup_enc_general_tablespace $topdir/backup_inc
