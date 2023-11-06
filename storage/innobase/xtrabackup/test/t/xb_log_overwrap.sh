. inc/common.sh

if ! $XB_BIN --help 2>&1 | grep -q debug-sync; then
    skip_test "Requires --debug-sync support"
fi

start_server --innodb_log_file_size=4M --innodb_thread_concurrency=1 \
    --innodb_log_buffer_size=1M

load_dbase_schema sakila
load_dbase_data sakila
mkdir $topdir/backup

#avoid log wrap before we do the first scan
innodb_wait_for_flush_all
run_cmd_expect_failure $XB_BIN $XB_ARGS --datadir=$mysql_datadir --backup \
    --lock-ddl=OFF --innodb_log_file_size=4M --target-dir=$topdir/backup \
    --debug-sync="xtrabackup_copy_logfile_pause" &

job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync

wait_for_xb_to_suspend $pid_file

xb_pid=`cat $pid_file`

# Create 8M+ of log data

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp1 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp2 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp3 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp4 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp5 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp6 ENGINE=InnoDB SELECT * FROM payment" sakila

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

# wait's return code will be the code returned by the background process
run_cmd wait $job_pid
