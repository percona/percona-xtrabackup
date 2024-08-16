########################################################################
# Test support for separate UNDO tablespace
########################################################################

. inc/common.sh

require_debug_pxb_version
################################################################################
# Start an uncommitted transaction pause "indefinitely" to keep the connection
# open
################################################################################
function start_uncomitted_transaction()
{
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
START TRANSACTION;
INSERT INTO t1 VALUES (1);
SELECT SLEEP(10000);
EOF
}


vlog "case #1: block parallel truncation during backup"

start_server
$MYSQL $MYSQL_ARGS -e "CREATE TABLE t1 (a INT) ENGINE=InnoDB" test
$MYSQL $MYSQL_ARGS -e "INSERT INTO t1 VALUES (1)" test
for i in {1..100} ; do
    mysql -e "CREATE UNDO TABLESPACE UNDO_$i ADD DATAFILE 'undo_$i.ibu'"
done;

for i in {1..100} ; do
    mysql -e "ALTER UNDO TABLESPACE UNDO_$i SET INACTIVE"
done &
undo_pid=$!

sleep 1s

start_uncomitted_transaction &
job_master=$!

xtrabackup --backup --lock-ddl=REDUCED --target-dir=$topdir/backup \
--debug-sync="ddl_tracker_before_lock_ddl" 2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

$MYSQL $MYSQL_ARGS -e "INSERT INTO t1 VALUES (1)" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

if ! egrep -q 'Deleted undo file: ./undo_1.ibu : [0-9]*' $topdir/backup.log ; then
    die "xtrabackup did not handle delete table DDL"
fi

if ! egrep -q 'New undo file: ./undo_1.ibu : [0-9]*' $topdir/backup.log ; then
    die "xtrabackup did not handle new table DDL"
fi

kill -SIGKILL $job_master
kill -SIGKILL $undo_pid

xtrabackup --prepare --target-dir=$topdir/backup
stop_server
rm -rf $MYSQLD_DATADIR/*
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

ROWS=`${MYSQL} ${MYSQL_ARGS}  -Ns -e "SELECT COUNT(*) FROM t1" test`
if [ "$ROWS" != "2" ]; then
    die "Unexpected number of rows in table t1: $ROWS"
fi


stop_server
rm -rf $MYSQLD_DATADIR
start_server

echo
echo PXB-3280 : undo log truncation causes assertion failure with reduced lock
echo
mysql -e "CREATE UNDO TABLESPACE UNDO_1 ADD DATAFILE 'undo_1.ibu'"
mysql -e "CREATE UNDO TABLESPACE UNDO_2 ADD DATAFILE 'undo_2.ibu'"

XB_ERROR_LOG=$topdir/backup.log
BACKUP_DIR=$topdir/backup
rm -rf $BACKUP_DIR
xtrabackup_background --backup --target-dir=$BACKUP_DIR --debug="d,undo_create_node" --lock-ddl=REDUCED

XB_PID=$!
pid_file=$BACKUP_DIR/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file

mysql -e "ALTER UNDO TABLESPACE UNDO_1 SET INACTIVE"
mysql -e "ALTER UNDO TABLESPACE UNDO_2 SET INACTIVE"
mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=1"
sleep 1

mysql -e "DROP UNDO TABLESPACE UNDO_1"
mysql -e "DROP UNDO TABLESPACE UNDO_2"

mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=default"

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $XB_PID
run_cmd wait $XB_PID

xtrabackup --prepare --target-dir=$BACKUP_DIR
record_db_state test

stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$BACKUP_DIR
start_server
verify_db_state test

echo
echo PXB-3280 : CASE2: undo log truncation causes assertion failure with reduced lock
echo

mysql -e "CREATE UNDO TABLESPACE UNDO_1 ADD DATAFILE 'undo_1.ibu'"
mysql -e "CREATE UNDO TABLESPACE UNDO_2 ADD DATAFILE 'undo_2.ibu'"

XB_ERROR_LOG=$topdir/backup.log
BACKUP_DIR=$topdir/backup
rm -rf $BACKUP_DIR
xtrabackup_background --backup --target-dir=$BACKUP_DIR --debug="d,undo_space_open" --lock-ddl=REDUCED

XB_PID=$!
pid_file=$BACKUP_DIR/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file

mysql -e "ALTER UNDO TABLESPACE UNDO_1 SET INACTIVE"
mysql -e "ALTER UNDO TABLESPACE UNDO_2 SET INACTIVE"
mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=1"
sleep 1

mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=default"

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $XB_PID
run_cmd wait $XB_PID

xtrabackup --prepare --target-dir=$BACKUP_DIR
record_db_state test

stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$BACKUP_DIR
start_server
verify_db_state test

stop_server
rm -rf $mysql_datadir $BACKUP_DIR
rm $XB_ERROR_LOG
