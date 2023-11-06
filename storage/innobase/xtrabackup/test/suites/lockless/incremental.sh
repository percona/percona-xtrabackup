###############################################################################
# PXB-3034: Reduce the time the instance remain under lock
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server


$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.delete_table (id INT PRIMARY KEY AUTO_INCREMENT);" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.original_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.original_table VALUES(1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.op_ddl (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)); INSERT INTO test.op_ddl VALUES(1, 'test')" test

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_base --lock-ddl=REDUCED

$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.original_table VALUES (2);" test
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_inc --incremental-basedir=$topdir/backup_base \
  --debug-sync="xtrabackup_load_tablespaces_pause" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup_with_new_table.log)&

job_pid=$!
pid_file=$topdir/backup_inc/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"


# Generate redo on table than delete it
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.delete_table VALUES (1); DROP TABLE test.delete_table;" test

# Create table and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.new_table VALUES (), ();" test

# Rename table and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.original_table TO test.renamed_table; INSERT INTO test.renamed_table VALUES (3);" test

# Bulk Index Load and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.op_ddl ADD INDEX(name); INSERT INTO test.op_ddl VALUES (2, 'test2');" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid



xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_base
xtrabackup --prepare --target-dir=$topdir/backup_base --incremental-dir=$topdir/backup_inc
record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup_base
start_server
verify_db_state test
