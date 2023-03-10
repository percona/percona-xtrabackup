########################################################################
# register pxb as redo log consummer. Backup should complete
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

mysql -e "CREATE TABLE redo_log_consumer (
  id INT PRIMARY KEY AUTO_INCREMENT,
  str BLOB
);" test

vlog "Run without redo log consumer"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --target-dir=$topdir/backup \
  --debug-sync="xtrabackup_pause_after_redo_catchup" \
 2> >( tee $topdir/backup_before_redo_register.log)&
job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

echo "backup pid is $job_pid"

vlog "sufficient insert to remove old redo files "

run_inserts

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

rm -rf $topdir/backup

vlog "backup with redo log consumer"
xtrabackup --backup --target-dir=$topdir/backup --register-redo-log-consumer \
  --debug-sync="xtrabackup_pause_after_redo_catchup" \
  2> >( tee $topdir/backup_before_redo_register.log)&
job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

echo "backup pid is $job_pid"

run_inserts &

# ER_IB_MSG_LOG_WRITER_WAIT_ON_CONSUMER
while ! grep -q "Redo log writer is waiting for" ${MYSQLD_ERRFILE} ; do
	sleep 1
	vlog "waiting for redo log writer message in mysql error file"
done

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

xtrabackup --prepare --target-dir=$topdir/backup
