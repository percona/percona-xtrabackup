#
# PXB-1942: Backup fails when database is on a fast storage with redo archive logs enabled
#
. inc/common.sh
. inc/keyring_file.sh

require_server_version_higher_than 8.0.16

mkdir $TEST_VAR_ROOT/b

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_capacity=1G
"

function run_inserts()
{
  for i in $(seq 1000);
  do
    $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.redo_log_consumer VALUES (NULL, REPEAT('a', 63 * 1024))";
  done
}

function run_test() {

  start_server --innodb-redo-log-archive-dirs=":$TEST_VAR_ROOT/b"

  mysql -e "CREATE TABLE redo_log_consumer (
  id INT PRIMARY KEY AUTO_INCREMENT,
  str BLOB
  );" test

  if [ "with_encryption" = $1 ]; then
    mysql -e "set global innodb_redo_log_encrypt=on"
  fi

  mkdir $topdir/backup

  xtrabackup --backup --target-dir=$topdir/backup \
	     --debug-sync="xtrabackup_pause_after_redo_catchup" \
	     2> >(tee $topdir/backup.log)&

  job_pid=$!
  pid_file=$topdir/backup/xtrabackup_debug_sync
  wait_for_xb_to_suspend $pid_file

  xb_pid=`cat $pid_file`

  run_inserts
  innodb_wait_for_flush_all

  # Resume the xtrabackup process
  vlog "Resuming xtrabackup"
  kill -SIGCONT $xb_pid

  run_cmd wait $job_pid

  if ! grep -q "Switched to archived redo log starting with LSN" $topdir/backup.log ; then
      die "Archived logs were not used"
  fi

  mysql -e "DROP TABLE redo_log_consumer;" test
  stop_server
  rm -rf $topdir/backup $topdir/backkup.log

}


vlog "running test without encryption"
run_test without_encryption
vlog "running test with encryption"
run_test with_encryption
