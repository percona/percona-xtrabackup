########################################################################
# PXB-3003 Race Condition in log_files_find_and_analyze
########################################################################
# Test 1 - ensure that xtrabackup continue when some files are gone,
# but the file containing the required lsn still exists.
#
# Test 2 - ensures xtrabackup fails when all files are gone.
########################################################################
. inc/common.sh

require_server_version_higher_than 8.0.29
require_debug_pxb_version

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_capacity=8M
"
start_server

function run_inserts()
{
  for i in $(seq 1000);
  do
    $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.redo_log_consumer VALUES (NULL, REPEAT('a', 63 * 1024))";
  done
}
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE redo_log_consumer (
  id INT PRIMARY KEY AUTO_INCREMENT,
  str BLOB
);" test

$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.redo_log_consumer VALUES (NULL, REPEAT('a', 63 * 1024))"
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.redo_log_consumer VALUES (NULL, REPEAT('a', 63 * 1024))"

mkdir -p $topdir/backup/

vlog "Test 1 - Some files are gone, but not all (--register-redo-log-consumer ensures that)"

xtrabackup --backup --target-dir=$topdir/backup --register-redo-log-consumer --debug="d,xtrabackup_reopen_files_after_catchup" 2> >( tee $topdir/xtrabackup_reopen_files_after_catchup.log) &
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

run_cmd wait $job_pid

if ! grep "Unable to open './#innodb_redo/#ib_redo" $topdir/xtrabackup_reopen_files_after_catchup.log
then
	die "xtrabackup did not show warning about missing files"
fi
run_cmd wait $insert_pid



vlog "Test 2 - All files are gone"

run_inserts &
insert_pid=$!

run_cmd_expect_failure xtrabackup --backup --target-dir=$topdir/backup2 --debug="d,xtrabackup_reopen_files_after_catchup" &
job_pid=$!
pid_file=$topdir/backup2/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

vlog "backup pid is $job_pid"

INITIAL_MAX_FILEID=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT MAX(file_id) FROM performance_schema.innodb_redo_log_files"`

CURRENT_MIN_FILEID=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT MIN(file_id) FROM performance_schema.innodb_redo_log_files"`
while [ "${CURRENT_MIN_FILEID}" -le "${INITIAL_MAX_FILEID}" ]
do
  sleep 1;
  CURRENT_MIN_FILEID=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT MIN(file_id) FROM performance_schema.innodb_redo_log_files"`
done
# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $insert_pid
rm -rf $topdir/backup/* $topdir/backup2/*

