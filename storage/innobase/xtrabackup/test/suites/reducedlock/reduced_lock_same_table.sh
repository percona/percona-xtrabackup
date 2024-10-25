###############################################################################
# PXB-XXXX: Reduce the time the instance remain under lock
# Test PXB-2683
# This tests the same scenario with two variants
# 1. The table is created before the backup starts - copy thread will copy its .ibd
# 2. The table is created after the backup starts - DDL Tracker will copy its .ibd after the backup completes
###############################################################################

. inc/common.sh

require_pro_pxb_version
require_debug_pxb_version
require_debug_server
start_server


$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.original_table_in_backup (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.original_table_in_backup VALUES (),(),(),(),()" test
innodb_wait_for_flush_all

$MYSQL $MYSQL_ARGS -Ns -e "SET GLOBAL innodb_checkpoint_disabled=true;"

xtrabackup --backup --target-dir=$topdir/backup_new_table \
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup_new_table/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE original_table_in_backup TO original_table_in_backup_renamed;" test
$MYSQL $MYSQL_ARGS -Ns -e "SET GLOBAL innodb_page_cleaner_disabled_debug=true;"
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.original_table_in_backup (id INT PRIMARY KEY AUTO_INCREMENT) KEY_BLOCK_SIZE=4;" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.original_table_in_backup VALUES (),();" test
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE original_table_in_backup_renamed;" test
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE original_table_in_backup TO original_table_in_backup_renamed;" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid


xtrabackup --prepare --target-dir=$topdir/backup_new_table
record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup_new_table
start_server
verify_db_state test
