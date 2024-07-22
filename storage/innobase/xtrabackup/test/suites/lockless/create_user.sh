require_debug_pxb_version
. inc/common.sh

start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.table (id INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO test.table VALUES (), (), (), ();" test
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_new_user \
  --debug-sync="xtrabackup_load_tablespaces_pause" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup_with_new_table.log)&

job_pid=$!
pid_file=$topdir/backup_new_user/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# CREATE USER
$MYSQL $MYSQL_ARGS -Ns -e "CREATE USER u1@localhost IDENTIFIED BY 'SomeStr0ngPWD!';"

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

xtrabackup --prepare --target-dir=$topdir/backup_new_user
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup_new_user
start_server

ROWS=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM mysql.user WHERE User = 'u1'"`
if [ "$ROWS" != "1" ]; then
  vlog "User u1 not found in mysql.user"
  exit 1
fi

USER=`${MYSQL} ${MYSQL_ARGS} -u u1 -p'SomeStr0ngPWD!' -Ns -e "SELECT CURRENT_USER()"`
if [ "$USER" != "u1@localhost" ]; then
  vlog "not able to login with new user u1"
  exit 1
fi
