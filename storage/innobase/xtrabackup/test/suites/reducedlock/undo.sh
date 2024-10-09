########################################################################
# Test support for separate UNDO tablespace
########################################################################

. inc/common.sh

require_debug_pxb_version
vlog "case #1: ensure truncated  undo tablespace is recopied"

start_server
$MYSQL $MYSQL_ARGS -e "CREATE TABLE t1 (a INT) ENGINE=InnoDB" test
$MYSQL $MYSQL_ARGS -e "INSERT INTO t1 VALUES (1)" test
mysql -e "CREATE UNDO TABLESPACE UNDO_1 ADD DATAFILE 'undo_1.ibu'"
mysql -e "CREATE UNDO TABLESPACE UNDO_2 ADD DATAFILE 'undo_2.ibu'"

xtrabackup --backup --lock-ddl=REDUCED --target-dir=$topdir/backup \
--debug-sync="ddl_tracker_before_lock_ddl" 2> >( tee $topdir/backup.log)&
job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

mysql -e "ALTER UNDO TABLESPACE UNDO_1 SET INACTIVE"
mysql -e "ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE"
mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=1"
sleep 3s

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

if ! egrep -q 'Deleted undo file: ./undo_1.ibu : [0-9]*' $topdir/backup.log ; then
    die "xtrabackup did not handle delete table DDL"
fi

if ! egrep -q "Deleted undo file: ./undo_001 : [0-9]*" $topdir/backup.log ; then
    die "xtrabackup did not handle delete table DDL for undo_001"
fi

if ! egrep -q 'New undo file: ./undo_1.ibu : [0-9]*' $topdir/backup.log ; then
    die "xtrabackup did not handle new table DDL for undo_1.ibu"
fi

if ! egrep -q 'New undo file: ./undo_001 : [0-9]*' $topdir/backup.log ; then
    die "xtrabackup did not handle new table DDL for undo_001"
fi


mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=default"
xtrabackup --prepare --target-dir=$topdir/backup
stop_server
rm -rf $MYSQLD_DATADIR/*
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

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
