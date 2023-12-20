###############################################################################
# PXB-3034: Reduce the time the instance remain under lock
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="ddl_tracker_before_lock_ddl" --parallel=10 --lock-ddl=REDUCED \
  2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

for i in {1..100} ; do
    $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.tb_${i} (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.tb_${i} VALUES (1)" test
done;


# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

# xtrabackup uses an ever incremental thread ID. Second phase copy will start from 20+
if ! egrep -q '[1-3][0-9] \[Note\] \[MY-[0-9]*\] \[Xtrabackup\] Copying test/tb_[0-9]*\.ibd to .*/test/tb_[0-9]*.ibd.new' $topdir/backup.log ; then
    die "xtrabackup did not copied tables in parallel"
fi

xtrabackup --prepare --target-dir=$topdir/backup
record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup
start_server
verify_db_state test
