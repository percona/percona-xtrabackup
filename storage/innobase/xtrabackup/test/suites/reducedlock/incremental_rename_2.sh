###############################################################################
# PXB-3318 : prepare_handle_ren_files(): failed to handle .ren files
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_base --lock-ddl=REDUCED

innodb_wait_for_flush_all

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE t1(a INT);" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t1 VALUES (1),(2),(3),(4)" test

xtrabackup --backup --target-dir=$topdir/backup_inc --incremental-basedir=$topdir/backup_base \
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup_inc.log)&

job_pid=$!
pid_file=$topdir/backup_inc/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# Generate redo on table t
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE t1 to t2" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_base
xtrabackup --prepare --target-dir=$topdir/backup_base --incremental-dir=$topdir/backup_inc

# Ensure two things. t1.ibd shouldn't be present
FILE=$topdir/backup_base/test/t1.ibd
[ ! -f $FILE ] || die "$FILE exists. It should have been deleted by prepare. Server has renamed the table to t2 during incremental"

# t2.ibd should be present
FILE=$topdir/backup_base/test/t2.ibd
[ -f $FILE ] || die "$FILE doesn't exists. It should have been present after rename from t1 to t2"

record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup_base
start_server
verify_db_state test
rm -rf $topdir/backup_base
rm -rf $topdir/backup_inc
rm $topdir/backup_inc.log
