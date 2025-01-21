#########################################################################################
# PXB-3227: Rename table and then drop table, make sure that original table was deleted 
#########################################################################################

. inc/common.sh

require_debug_pxb_version
require_pro_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.original_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.original_table VALUES(1);" test
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="xtrabackup_suspend_at_start" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# Rename table and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.original_table TO test.renamed_table; INSERT INTO test.renamed_table VALUES (2);" test
# Drop table
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.renamed_table;" test

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

if [ -f $mysql_datadir/test/original_table.ibd ] ; then
    die 'dropped original_table.ibd file has been copied to the datadir'
fi

stop_server
rm -rf $topdir/backup

#########################################################################################################
# PXB-3229: Make sure backup does not fail with `File exists` error trying to create .del file twice 
#########################################################################################################

start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.t1 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.t1 VALUES(1);" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.t2 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.t2 VALUES(1);" test
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="xtrabackup_suspend_at_start" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# Rename table and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.t1 TO test.t3; INSERT INTO test.t3 VALUES (2);" test
# Drop table
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.t3;" test
# Rename table and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.t2 TO test.t3; INSERT INTO test.t3 VALUES (2);" test
# Drop table
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.t3;" test

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
stop_server
rm -rf $mysql_datadir $topdir/backup





