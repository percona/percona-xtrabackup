. inc/xbcloud_common.sh
is_xbcloud_credentials_set
require_debug_pxb_version

start_server

write_credentials

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table1 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.new_table1 VALUES (1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table2 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.new_table2 VALUES (1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table3 (id INT PRIMARY KEY AUTO_INCREMENT, name varchar(10)); INSERT INTO test.new_table3 VALUES (1, 'a')" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table4 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.new_table4 VALUES (1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table5 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.new_table5 VALUES (1)" test
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table6 (id INT PRIMARY KEY AUTO_INCREMENT, name varchar(10)); INSERT INTO test.new_table6 VALUES (1, 'a')" test

innodb_wait_for_flush_all


vlog "take full backup"

(xtrabackup --backup --target-dir=$topdir/full --stream --extra-lsndir=$topdir/full\
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
| run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf put \
	    --parallel=4 \
	    ${full_backup_name}) &

job_pid=$!
pid_file=$topdir/full/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table7 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.new_table7 VALUES (1)" test
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.new_table1 TO renamed_table; INSERT INTO test.renamed_table VALUES (2)" test
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.new_table2" test
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.new_table3 ADD INDEX(name); INSERT INTO test.new_table3 VALUES (2, 'test2');" test


# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

innodb_wait_for_flush_all

vlog "take incremental backup"



(xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/full --stream \
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
| run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf put \
            --parallel=4 \
      ${inc_backup_name}) &
job_pid=$!
pid_file=$topdir/inc/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.new_table8 (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.new_table8 VALUES (1)" test
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.new_table4 TO renamed_table2; INSERT INTO test.renamed_table2 VALUES (2)" test
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.new_table5" test
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.new_table6 ADD INDEX(name); INSERT INTO test.new_table6 VALUES (2, 'test2');" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

innodb_wait_for_flush_all
record_db_state test

vlog "download and prepare"
mkdir $topdir/downloaded_full
mkdir $topdir/downloaded_inc

run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf get \
        --parallel=4 \
	${full_backup_name} | \
    xbstream -xv -C $topdir/downloaded_full --parallel=4

xtrabackup --prepare --apply-log-only --target-dir=$topdir/downloaded_full

run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf get \
        --parallel=4 \
        ${inc_backup_name} | \
    xbstream -xv -C $topdir/downloaded_inc

xtrabackup --prepare \
	   --target-dir=$topdir/downloaded_full \
	   --incremental-dir=$topdir/downloaded_inc

stop_server
rm -rf $MYSQLD_DATADIR/*

xtrabackup --copy-back --target-dir=$topdir/downloaded_full
start_server
verify_db_state test
