##########################################################################
# pxb-3003-redo-log-purge-race.sh script has tested two possible scenarios 
# where redo log purges can cause backup failures. We also discovered that, 
# once a redo log purge occurs between the completion of the non-InnoDB file 
# backup and the end of the redo log copy, it can result in incomplete redo 
# log copying, while the backup still completes successfully.
#
# This test should make sure that, in this scenario, backup will fail due to
# incomplete redo log copying.
# ########################################################################
 
. inc/common.sh
 
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_capacity=8M
"
 
require_debug_pxb_version
start_server
 
function run_inserts()
{
  for i in $(seq 1000);
  do
    $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.t1 VALUES (NULL, REPEAT('a', 63 * 1024))";
  done
}
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE t1 (
  id INT PRIMARY KEY AUTO_INCREMENT,
  str BLOB
);" test
 
 
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.t1 VALUES (NULL, REPEAT('a', 63 * 1024))"
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.t1 VALUES (NULL, REPEAT('a', 63 * 1024))"
 
mkdir -p $topdir/backup/
 
run_cmd_expect_failure xtrabackup --backup --target-dir=$topdir/backup --debug=d,xtrabackup_pause_before_stop_copy_redo \
  2> >( tee $topdir/xtrabackup_pause_before_stop_copy_redo.log) &
job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
 
vlog "backup pid is $job_pid"
 
INITIAL_MIN_FILEID=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT MIN(file_id) FROM performance_schema.innodb_redo_log_files"`
 
run_inserts &
insert_pid=$!
CURRENT_MIN_FILEID=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT MIN(file_id) FROM performance_schema.innodb_redo_log_files"`
while [ "${INITIAL_MIN_FILEID}" -eq "${CURRENT_MIN_FILEID}" ]
do
  sleep 1;
  vlog "Waiting for redo log files to be created. Current MIN_FILEID: $CURRENT_MIN_FILEID Initial MIN_FILEID: $INITIAL_MIN_FILEID"
  CURRENT_MIN_FILEID=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT MIN(file_id) FROM performance_schema.innodb_redo_log_files"`
done
# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
 
run_cmd wait $insert_pid
 
rm -rf $topdir/backup/*
