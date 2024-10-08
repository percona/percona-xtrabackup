########################################################################
# Test support for external tablespaces
########################################################################

. inc/common.sh

require_debug_pxb_version
vlog "case #1: ensure external undo tablespaces are copied"

undo_directory_ext=$TEST_VAR_ROOT/var1/undo_dir_ext
mkdir -p $undo_directory_ext

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_undo_directory=$undo_directory_ext
"

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
mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=1"
sleep 3s

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

if ! egrep -q "Deleted undo file: $undo_directory_ext/undo_1.ibu : [0-9]*" $topdir/backup.log ; then
    die "xtrabackup did not handle delete table DDL"
fi

if ! egrep -q "Done: Writing file $topdir/backup/[0-9]*.del" $topdir/backup.log ; then
    die "xtrabackup did not create .del file"
fi

if ! egrep -q "New undo file: $undo_directory_ext/undo_1.ibu : [0-9]*" $topdir/backup.log ; then
    die "xtrabackup did not handle new table DDL"
fi

if ! egrep -q "Done: Copying $undo_directory_ext/undo_1.ibu to $topdir/backup/undo_1.ibu.new" $topdir/backup.log ; then
    die "xtrabackup did not create undo_1.ibu.new file"
fi


mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=default"
xtrabackup --prepare --target-dir=$topdir/backup
stop_server
rm -rf $MYSQLD_DATADIR/*
rm -rf $undo_directory_ext/*
xtrabackup --copy-back --target-dir=$topdir/backup --innodb_undo_directory=$undo_directory_ext
start_server

stop_server
rm -rf $MYSQLD_DATADIR
rm -rf $undo_directory_ext



vlog "case #2: ensure regular external tablespaces are handled"

data_directory_ext=$TEST_VAR_ROOT/var1/data_dir_ext
mkdir -p $data_directory_ext

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_directories=$data_directory_ext
"
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.DELETE_TABLE (id INT PRIMARY KEY AUTO_INCREMENT) DATA DIRECTORY = '$data_directory_ext';" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.ORIGINAL_TABLE (id INT PRIMARY KEY AUTO_INCREMENT) DATA DIRECTORY = '$data_directory_ext'; INSERT INTO test.ORIGINAL_TABLE VALUES(1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.OP_DDL_TABLE (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50)) DATA DIRECTORY = '$data_directory_ext'; INSERT INTO test.OP_DDL_TABLE VALUES(1, 'test')" test

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_new_table \
--debug-sync="xtrabackup_load_tablespaces_pause" --lock-ddl=REDUCED \
2> >( tee $topdir/backup_with_new_table.log)&

job_pid=$!
pid_file=$topdir/backup_new_table/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# Generate redo on table than delete it
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.DELETE_TABLE VALUES (1); DROP TABLE test.DELETE_TABLE;" test

# Create table and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.NEW_TABLE (id INT PRIMARY KEY AUTO_INCREMENT) DATA DIRECTORY = '$data_directory_ext'; INSERT INTO test.NEW_TABLE VALUES (), ();" test

# Bulk Index Load and generate redo
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.OP_DDL_TABLE ADD INDEX(name); INSERT INTO test.OP_DDL_TABLE VALUES (2, 'test2');" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

if ! egrep -q "DDL tracking : LSN: [0-9]* delete space ID: [0-9]* Name: $data_directory_ext/test/DELETE_TABLE.ibd" $topdir/backup_with_new_table.log ; then
    die "xtrabackup did not handle delete table DDL"
fi

if ! egrep -q "Done: Writing file $topdir/backup_new_table/test/[0-9]*.del" $topdir/backup_with_new_table.log ; then
    die "xtrabackup did not create .del file"
fi

if ! egrep -q "DDL tracking : LSN: [0-9]* create space ID: [0-9]* Name: $data_directory_ext/test/NEW_TABLE.ibd" $topdir/backup_with_new_table.log ; then
    die "xtrabackup did not handle new table DDL"
fi

if ! egrep -q "Done: Copying $data_directory_ext/test/OP_DDL_TABLE.ibd to $topdir/backup_new_table/test/OP_DDL_TABLE.ibd.new" $topdir/backup_with_new_table.log ; then
    die "xtrabackup did not create OP_DDL_TABLE.ibd.new file"
fi

if ! egrep -q "DDL tracking : LSN: [0-9]* add index on space ID: [0-9]*" $topdir/backup_with_new_table.log ; then
    die "xtrabackup did not handle Bulk Index Load DDL"
fi

xtrabackup --prepare --target-dir=$topdir/backup_new_table
record_db_state test
stop_server
rm -rf $mysql_datadir/*
rm -rf $data_directory_ext/*
xtrabackup --copy-back --target-dir=$topdir/backup_new_table
start_server
verify_db_state test
stop_server
rm -rf $mysql_datadir $topdir/backup_new_table $data_directory_ext

