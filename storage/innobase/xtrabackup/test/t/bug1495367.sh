########################################################################
# Bug 1495367: xtrabackup 2.3 does not set wait_timeout session variable
########################################################################

MYSQLD_EXTRA_MY_CNF_OPTS="
wait_timeout=1
"

start_server

xtrabackup --backup --target-dir=$topdir/backup \
	   --debug-sync="data_copy_thread_func" &

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync

wait_for_xb_to_suspend $pid_file

xb_pid=`cat $pid_file`

sleep 1

vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

wait $job_pid
