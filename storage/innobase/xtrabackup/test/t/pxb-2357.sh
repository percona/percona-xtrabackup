# PXB-2357: hang in backup with redo log archive#

skip_test "This test is not testing what it is intended to. Check PXB-3110"

require_debug_pxb_version

require_server_version_higher_than 8.0.16

mkdir $TEST_VAR_ROOT/b

start_server --innodb-redo-log-archive-dirs=":$TEST_VAR_ROOT/b"

mkdir $topdir/backup

xtrabackup --backup --lock-ddl=OFF --target-dir=$topdir/backup \
           --debug-sync="stop_before_redo_archive" \
           2> >(tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file

xb_pid=`cat $pid_file`

mysql -e "create table t(i int)" test 2>/dev/null >/dev/null

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

while true ; do
	mysql -e "create table t2(i int)" test 2>/dev/null >/dev/null
	mysql -e "insert into t2 select * from t" test 2>/dev/null >/dev/null
	mysql -e "drop table t2" test 2>/dev/null >/dev/null
done  &

run_cmd wait $job_pid
