########################################################################
#test redo enable/disable feature with backup and incremental backup	
########################################################################
. inc/common.sh
require_server_version_higher_than 8.0.20
start_server

vlog '### create a table and keep on inserting records'
mysql -e "CREATE TABLE t (a INT)" test
mysql -e "INSERT INTO t VALUES (1), (2), (3), (4)" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test

while true ; do
    mysql -e "INSERT INTO t SELECT * FROM t limit 100" test 2>/dev/null >/dev/null
done &



vlog "### case #1 check when redo log is disabled ###"
mysql -e "alter instance disable innodb redo_log"
run_cmd_expect_failure xtrabackup --backup \
        --target-dir=$topdir/backup  2>&1 \
        | tee $topdir/pxb.log
grep "Redo logging is disabled, cannot take consistent backup" $topdir/pxb.log || die "missing error message"
mysql -e "alter instance enable innodb redo_log"

vlog "### case #2 check redo log is disabled before incremental backup ###"
run_cmd xtrabackup --backup --target-dir=$topdir/full
mysql -e "alter instance disable innodb redo_log"
run_cmd_expect_failure xtrabackup --backup --target-dir=$topdir/inc \
	--incremental-basedir=$topdir/full 2>&1 \
	| tee $topdir/inc.log
grep "Redo logging is disabled, cannot take consistent backup" $topdir/inc.log || die "missing error message"
rm -r $topdir/full
rm  $topdir/inc.log
mysql -e "alter instance enable innodb redo_log"

vlog "### case #3 check when redo log is disabled after backup is started ###"
run_cmd_expect_failure xtrabackup  --backup \
        --target-dir=$topdir/full --lock-ddl=OFF   \
	--debug-sync="data_copy_thread_func" 2>&1 \
        | tee $topdir/full.log &
job_pid=$!
pid_file=$topdir/full/xtrabackup_debug_sync
# Wait for xtrabackup to suspend
i=0
while [ ! -r "$pid_file" ]
do
    sleep 1
    i=$((i+1))
    echo "Waited $i seconds for $pid_file to be created"
done
mysql -e "alter instance disable innodb redo_log"
xb_pid=`cat $pid_file`
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid
grep "Redo logging is disabled, cannot take consistent backup" $topdir/full.log || die "missing error message"
rm -r $topdir/full
rm $topdir/full.log
mysql -e "alter instance enable innodb redo_log"

vlog "### case #4 check redo log is disabled during incremental backup ###"
run_cmd xtrabackup --backup --target-dir=$topdir/full
run_cmd_expect_failure xtrabackup --backup --lock-ddl=OFF --target-dir=$topdir/inc \
	--incremental-basedir=$topdir/full \
	--debug-sync="data_copy_thread_func" 2>&1 \
	| tee $topdir/inc.log &
job_pid=$!
pid_file=$topdir/inc/xtrabackup_debug_sync
# Wait for xtrabackup to suspend
i=0
while [ ! -r "$pid_file" ]
do
    sleep 1
    i=$((i+1))
    echo "Waited $i seconds for $pid_file to be created"
done
mysql -e "alter instance disable innodb redo_log"
xb_pid=`cat $pid_file`
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid
grep "Redo logging is disabled, cannot take consistent backup" $topdir/inc.log || die "missing error message"
rm -r $topdir/inc
rm -r $topdir/full
rm  $topdir/inc.log
mysql -e "alter instance enable innodb redo_log"

vlog "case #5 disable disable/enable before normal backup"
mysql -e "alter instance disable innodb redo_log"
load_sakila
mysql -e "alter instance enable innodb redo_log"
xtrabackup --backup --target-dir=$topdir/full
record_db_state sakila
stop_server
rm -r $mysql_datadir
xtrabackup --prepare --target-dir=$topdir/full
xtrabackup --copy-back --target-dir=$topdir/full
start_server
verify_db_state sakila
stop_server
rm -r $mysql_datadir
rm -r $topdir/full
start_server

vlog "case #6 run normal backup and disable/enable before incremental"
xtrabackup --backup --target-dir=$topdir/full
mysql -e "alter instance disable innodb redo_log"
load_sakila
mysql -e "alter instance enable innodb redo_log"
xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/full
record_db_state sakila
stop_server
rm -r $mysql_datadir
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
xtrabackup --prepare --target-dir=$topdir/full --incremental-dir=$topdir/inc
xtrabackup --copy-back --target-dir=$topdir/full
start_server
verify_db_state sakila
stop_server
rm -r $mysql_datadir
rm -r $topdir/full
rm -r $topdir/inc
