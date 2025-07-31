#
# PXB-3571 : --transition-key does not save keys with --lock-ddl=reduced
#

. inc/common.sh
require_debug_pxb_version
require_pro_pxb_version

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

mysql -e "CREATE TABLE t (a INT) ENCRYPTION='y'" test

shutdown_server
start_server

for i in {1..100} ; do
    mysql -e "INSERT INTO t VALUES ($i)" test
done

BACKUP_DIR=$topdir/backup
xtrabackup --backup --transition-key=123 --target-dir=$BACKUP_DIR --lock-ddl=reduced
record_db_state test
xtrabackup --prepare --transition-key=123 --target-dir=$BACKUP_DIR --lock-ddl=reduced --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$BACKUP_DIR --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir
start_server
verify_db_state test

rm -rf $BACKUP_DIR

mysql -e "CREATE TABLE t2(a INT) ENCRYPTION='y'" test

xtrabackup --backup --transition-key=123 --target-dir=$BACKUP_DIR  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED 2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$BACKUP_DIR/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# Generate redo on table than delete it
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.t2 VALUES (1); DROP TABLE test.t2;" test

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

record_db_state test

xtrabackup --prepare --transition-key=123 --target-dir=$BACKUP_DIR --lock-ddl=reduced --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$BACKUP_DIR --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir
start_server
verify_db_state test

rm -rf $BACKUP_DIR
rm $topdir/backup.log
