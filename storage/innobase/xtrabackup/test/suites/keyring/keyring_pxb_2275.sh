. inc/common.sh
KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-log-file-size=80M
"
configure_server_with_component

if ! $XB_BIN --help 2>&1 | grep -q debug-sync; then
    skip_test "Requires --debug-sync support"
fi

if is_server_version_lower_than 5.7.0
then
    skip_test "Requires server version 5.7"
fi

load_dbase_schema sakila
load_dbase_data sakila
mkdir $topdir/backup

# Test 1 - should fail since we don't have any entry on keyring file yet
vlog "Test 1 - Should fail as keyring file does not have encryption information"
run_cmd_expect_failure $XB_BIN $XB_ARGS --innodb-log-file-size=80M --xtrabackup-plugin-dir=${plugin_dir} --lock-ddl=false --backup \
--target-dir=$topdir/backup --debug-sync="xtrabackup_pause_after_redo_catchup" 2> >(tee $topdir/backup.log)&

job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync

wait_for_xb_to_suspend $pid_file

xb_pid=`cat $pid_file`

# Create some data on Redo
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp1 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp1 SELECT * FROM payment" sakila


# Create encrypted table
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp2 (ID INT PRIMARY KEY) ENCRYPTION='Y'" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp2 VALUES (1)" sakila

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

# wait's return code will be the code returned by the background process
run_cmd wait $job_pid
if ! grep -q "is missing encryption information" $topdir/backup.log ; then
      die "Backup did not fail as expected"
fi

rm -rf $topdir/backup
mkdir $topdir/backup

# Clean-up
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE tmp1, tmp2" sakila


# Test 2 - Should pass as keyring file already have encryption information
vlog "Test 2 - Should pass as keyring file already have encryption information"

# Write data on Redo log so advance checkpoint to after creation of 1st encryption table
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp1 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp1 SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp1 SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp1 SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp1 SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp1 SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp1 SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE tmp1" sakila

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all


run_cmd $XB_BIN $XB_ARGS --innodb-log-file-size=80M --lock-ddl=false --backup \
--target-dir=$topdir/backup --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} --debug-sync="xtrabackup_pause_after_redo_catchup" &

job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync

wait_for_xb_to_suspend $pid_file

xb_pid=`cat $pid_file`

# Create some data on Redo
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp1 ENGINE=InnoDB SELECT * FROM payment" sakila
$MYSQL $MYSQL_ARGS -Ns -e "DELETE FROM tmp1" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp1 SELECT * FROM payment" sakila


# Create encrypted table
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE tmp2 (ID INT PRIMARY KEY) ENCRYPTION='Y'" sakila
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO tmp2 VALUES (1)" sakila

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

# wait's return code will be the code returned by the background process
run_cmd wait $job_pid

# prepare backup
run_cmd $XB_BIN $XB_ARGS  --prepare \
--target-dir=$topdir/backup --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

# restore
stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

start_server --innodb-log-file-size=80M
run_cmd $MYSQL $MYSQL_ARGS -Ns -e "SELECT * FROM tmp2;" sakila
