###############################################################################
# PXB-3220: Tolerate file deletion/rename between discovery and file open
###############################################################################

. inc/common.sh

require_debug_pxb_version

start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.drop_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.drop_table VALUES(1);" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.rename_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.rename_table VALUES(1);" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.alter_rename_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.alter_rename_table VALUES(1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.alter_drop_column_table (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.alter_drop_column_table VALUES(1, 'test')" test

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="xtrabackup_suspend_between_file_discovery_and_open" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# Delete table
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.drop_table;" test
# Rename table and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.rename_table TO test.new_rename_table; INSERT INTO test.new_rename_table VALUES(2);" test
# Alter table rename and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.alter_rename_table RENAME test.new_alter_rename_table; INSERT INTO test.new_alter_rename_table VALUES(2);" test
# Alter table drop column and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.alter_drop_column_table DROP COLUMN name; INSERT INTO test.alter_drop_column_table VALUES(2);" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

if ! egrep -q "DDL tracking : LSN: [0-9]* delete table ID: [0-9]* Name: test/drop_table.ibd" $topdir/backup.log ; then
    die "xtrabackup did not handle delete table DDL"
fi

if ! egrep -q "DDL tracking : LSN: [0-9]* rename table ID: [0-9]* From: test/rename_table.ibd To: test/new_rename_table.ibd" $topdir/backup.log ; then
    die "xtrabackup did not handle rename table DDL"
fi

if ! egrep -q "DDL tracking : LSN: [0-9]* rename table ID: [0-9]* From: test/alter_rename_table.ibd To: test/new_alter_rename_table.ibd" $topdir/backup.log ; then
    die "xtrabackup did not handle alter table rename DDL"
fi

xtrabackup --prepare --target-dir=$topdir/backup
record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup
start_server
verify_db_state test
stop_server
rm -rf $mysql_datadir $topdir/backup
