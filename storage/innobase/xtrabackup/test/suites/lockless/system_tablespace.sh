# PXB-3221 : Assertion failure: page0cur.cc:1177:ib::fatal triggered during prepare
. inc/common.sh

require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE t1(a INT, KEY k1(a)) TABLESPACE=innodb_system;" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 VALUES (1),(2),(3),(4)" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test

innodb_wait_for_flush_all

XB_ERROR_LOG=$topdir/backup.log
BACKUP_DIR=$topdir/backup
rm -rf $BACKUP_DIR
xtrabackup --backup --lock-ddl=REDUCED --target-dir=$BACKUP_DIR \
--debug-sync="ddl_tracker_before_lock_ddl" 2> >( tee $XB_ERROR_LOG)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

mysql -e "ALTER TABLE t1 ADD INDEX k2(a)" test
mysql -e "ALTER TABLE t1 DROP INDEX k1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test


# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

xtrabackup --prepare --target-dir=$BACKUP_DIR
stop_server

rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$BACKUP_DIR
start_server
#mysql -e "ALTER TABLE t1 DROP INDEX k1" test
mysql -e "DELETE FROM t1 WHERE a = 2" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 SELECT * FROM t1" test
mysql -e "DELETE FROM t1 WHERE a = 2" test

stop_server

rm -rf $mysql_datadir $BACKUP_DIR
rm $XB_ERROR_LOG
