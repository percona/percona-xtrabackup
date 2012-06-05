. inc/common.sh

start_server --innodb_log_file_size=1M --innodb_thread_concurrency=1

load_dbase_schema sakila
load_dbase_data sakila
mkdir $topdir/backup

run_cmd_expect_failure $XB_BIN $XB_ARGS --datadir=$mysql_datadir --backup \
    --innodb_log_file_size=1M --target-dir=$topdir/backup --suspend-at-end &

xb_pid=$!

# Wait for xtrabackup to suspend
i=0
while [ ! -r $topdir/backup/xtrabackup_suspended ]
do
    sleep 1
    i=$((i+1))
    echo "Waited $i seconds for xtrabackup_suspended to be created"
done

# Stop the xtrabackup process
kill -SIGSTOP $xb_pid
 
# Create 2M+ of log data

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp1 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp2 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp3 ENGINE=InnoDB SELECT * FROM payment" sakila

# Resume the xtrabackup process
kill -SIGCONT $xb_pid

rm -f $topdir/backup/xtrabackup_suspended

# wait's return code will be the code returned by the background process
wait $xb_pid
