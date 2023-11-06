## This test will be changed once https://jira.percona.com/browse/PXB-3197 is implemented
KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh

require_debug_pxb_version
configure_server_with_component

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.enc_table (id INT PRIMARY KEY AUTO_INCREMENT) ENCRYPTION='Y'; INSERT INTO test.enc_table VALUES (), (), (), ();" test
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_alter_MK \
  --debug-sync="xtrabackup_load_tablespaces_pause" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup_alter_MK.log)&

job_pid=$!
pid_file=$topdir/backup_alter_MK/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"


$MYSQL $MYSQL_ARGS -Ns -e "ALTER INSTANCE ROTATE INNODB MASTER KEY;" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.enc_table VALUES ();" test
# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd_expect_failure wait $job_pid

if ! egrep -q 'Space ID [0-9]* is missing encryption information' $topdir/backup_alter_MK.log ; then
    die "xtrabackup did not failed due to missing encryption information"
fi

