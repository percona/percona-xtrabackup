###############################################################################
# PXB-3034: Reduce the time the instance remain under lock
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.original_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.original_table VALUES(1);" test
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
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.original_table TO test.renamed_table; INSERT INTO test.renamed_table VALUES (2);" test
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.renamed_table TO test.renamed_table2; INSERT INTO test.renamed_table2 VALUES (3);" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid


xtrabackup --prepare --target-dir=$topdir/backup
record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup
start_server
verify_db_state test
