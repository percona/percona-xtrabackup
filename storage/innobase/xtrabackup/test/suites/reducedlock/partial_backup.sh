###############################################################################
# PXB-3034: Reduce the time the instance remain under lock
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="ddl_tracker_before_lock_ddl" --tables=test.to_backup --lock-ddl=REDUCED \
  2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.to_backup (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.to_backup VALUES (1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.to_skip (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.to_skip VALUES (1)" test


# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

if ls $topdir/backup/test/to_skip* 2>/dev/null; then
    die "xtrabackup did not skip table"
fi

if ! ls $topdir/backup/test/to_backup* 2>/dev/null; then
    die "xtrabackup did not backup table"
fi

xtrabackup --prepare --export --target-dir=$topdir/backup

$MYSQL $MYSQL_ARGS -Ns -e "TRUNCATE TABLE test.to_backup" test

ROWS=`${MYSQL} ${MYSQL_ARGS}  -Ns -e "SELECT COUNT(*) FROM test.to_backup" test`
if [ "$ROWS" != "0" ]; then
    die "xtrabackup did not truncate table"
fi

$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.to_backup DISCARD TABLESPACE" test
cp -R $topdir/backup/test/to_backup* $mysql_datadir/test/
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.to_backup IMPORT TABLESPACE" test

ROWS=`${MYSQL} ${MYSQL_ARGS}  -Ns -e "SELECT COUNT(*) FROM test.to_backup" test`
if [ "$ROWS" != "1" ]; then
    die "xtrabackup did not import tablespace"
fi
