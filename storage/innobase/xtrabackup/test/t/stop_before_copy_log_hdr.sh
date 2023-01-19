########################################################################
# check if PXB can set the start LSN
# ########################################################################
. inc/common.sh

require_debug_pxb_version

start_server

function run_inserts()
{
  for i in $(seq 100);
  do
    $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.t1 VALUES (NULL, REPEAT('a', 63 * 1024))";
  done
}

mysql -e "CREATE TABLE t1 (
  id INT PRIMARY KEY AUTO_INCREMENT,
  str BLOB
);" test

xtrabackup --backup --target-dir=$topdir/backup --debug=d,simulate_backup_lower_version \
  --debug-sync="stop_before_copy_log_hdr" \
  2> >( tee $topdir/backup_before_redo_register.log)&
job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

echo "backup pid is $job_pid"

run_inserts
innodb_wait_for_flush_all

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

xtrabackup --prepare --target-dir=$topdir/backup
