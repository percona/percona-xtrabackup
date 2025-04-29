###############################################################################
# PXB-3279: Remove requirement to have open file handles equal to files in datadir
###############################################################################

. inc/common.sh

require_debug_pxb_version
require_pro_pxb_version

open_file_limit=$(ulimit -n)
ulimit -n 1024

start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.drop_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.drop_table VALUES(1);" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.drop_create_same_name_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.drop_create_same_name_table VALUES(1);" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.rename_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.rename_table VALUES(1);" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.alter_rename_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.alter_rename_table VALUES(1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.alter_drop_column_table (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.alter_drop_column_table VALUES(1, 'test')" test

for i in {1..1500} ; do
    $MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.tb_${i} (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.tb_${i} VALUES (1)" test
done;

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
run_cmd $MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.drop_table;" test
run_cmd $MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.drop_create_same_name_table;" test
# Create table and generate redo; Table name is same as the one that was dropped but different space id; This should cause space_id mismatch and added to missing list 
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.drop_create_same_name_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.drop_create_same_name_table VALUES(1);" test
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

if ! egrep -q "DDL tracking : LSN: [0-9]* delete space ID: [0-9]* Name: test/drop_table.ibd" $topdir/backup.log ; then
    die "xtrabackup did not handle delete table DDL"
fi

if ! egrep -q "DDL tracking : LSN: [0-9]* rename space ID: [0-9]* From: test/rename_table.ibd To: test/new_rename_table.ibd" $topdir/backup.log ; then
    die "xtrabackup did not handle rename table DDL"
fi

if ! egrep -q "DDL tracking : LSN: [0-9]* rename space ID: [0-9]* From: test/alter_rename_table.ibd To: test/new_alter_rename_table.ibd" $topdir/backup.log ; then
    die "xtrabackup did not handle alter table rename DDL"
fi

if ! egrep -q "DDL tracking : missing after discovery space ID: [0-9]*" $topdir/backup.log ; then
    die "xtrabackup did not add dropped table to missing list"
fi

xtrabackup --prepare --target-dir=$topdir/backup
record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup
start_server
verify_db_state test

stop_server
ulimit -n $open_file_limit
rm -rf $mysql_datadir $topdir/backup
rm $topdir/backup.log
