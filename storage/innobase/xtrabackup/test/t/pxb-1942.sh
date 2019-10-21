#
# PXB-1942: Backup fails when database is on a fast storage with redo archive logs enabled
#

require_server_version_higher_than 8.0.16

mkdir $TEST_VAR_ROOT/b

start_server --innodb-redo-log-archive-dirs=":$TEST_VAR_ROOT/b"

mysql -e "CREATE TABLE t (a INT)" test
mysql -e "INSERT INTO t VALUES (1), (2), (3), (4)" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test

mkdir $topdir/backup

xtrabackup --backup --target-dir=$topdir/backup \
           --debug-sync="after_redo_log_manager_init" \
           2> >(tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file

xb_pid=`cat $pid_file`

mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test

while true ; do
    mysql -e "INSERT INTO t SELECT * FROM t" test 2>/dev/null >/dev/null
done &

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

run_cmd wait $job_pid

if ! grep -q "Archived redo log has caught up" $topdir/backup.log ; then
    if ! grep -q "Switched to archived redo log starting with LSN" $topdir/backup.log ; then
        die "Archived logs were not used"
    fi
fi
