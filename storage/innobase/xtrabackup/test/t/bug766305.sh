########################################################################
# Bug #766305: Use MySQL Code to get stack trace
########################################################################

start_server --innodb_file_per_table

if [ ${ASAN_OPTIONS:-undefined} != "undefined" ]
then
    skip_test "Incompatible with AddressSanitizer"
fi

mkdir $topdir/backup

xtrabackup --backup --target-dir=$topdir/backup --debug-sync="data_copy_thread_func" &

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync

# Wait for xtrabackup to suspend
i=0
while [ ! -r "$pid_file" ]
do
    sleep 1
    i=$((i+1))
    echo "Waited $i seconds for $pid_file to be created"
done

xb_pid=`cat $pid_file`

kill -SIGSEGV $xb_pid
kill -SIGCONT $xb_pid

run_cmd_expect_failure wait $job_pid

grep -q "handle_fatal_signal" $OUTFILE || \
  die "Could not find a resolved stacktrace in the log"
