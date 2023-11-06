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

if ! egrep -q 'DDL tracking : LSN: [0-9]* delete table ID: [0-9]* Name: undo_1.ibu' $topdir/backup.log ; then
    die "xtrabackup did not handle delete table DDL"
fi

if ! egrep -q 'DDL tracking : LSN: [0-9]* create table ID: [0-9]* Name: undo_1.ibu' $topdir/backup.log ; then
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
