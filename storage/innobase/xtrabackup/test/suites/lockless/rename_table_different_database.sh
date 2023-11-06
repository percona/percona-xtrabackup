###############################################################################
# PXB-3034: Reduce the time the instance remain under lock
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.original_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.original_table VALUES(1); CREATE DATABASE test2;" test
innodb_wait_for_flush_all


xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# Rename table and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.original_table TO test2.renamed_table; INSERT INTO test2.renamed_table VALUES (2);" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

# Ensure we have DDL tracking in the log renaming the table
if ! egrep -q 'DDL tracking : LSN: [0-9]* rename table ID: [0-9]* From: test/original_table.ibd To: test2/renamed_table.ibd' $topdir/backup.log ; then
    die "xtrabackup did not handle rename table DDL"
fi

xtrabackup --prepare --target-dir=$topdir/backup
record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup
start_server
verify_db_state test
