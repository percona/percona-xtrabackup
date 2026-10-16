###############################################################################
# Incremental --lock-ddl=REDUCED: a tablespace renamed to an intermediate
# name before the inc copy phase, and renamed back to its original name
# while the inc backup is running, must end up under the original name in
# the prepared backup. prepare_handle_ren_files() must rename the captured
# .delta/.meta to the final name even when source path == dest path.
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.t1 (id INT PRIMARY KEY AUTO_INCREMENT, payload VARCHAR(32)) ENGINE=InnoDB; INSERT INTO test.t1 (payload) VALUES ('a'),('b'),('c');" test
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_base --lock-ddl=REDUCED
innodb_wait_for_flush_all

# Rename to the intermediate name; inc copy will capture the file under
# this name.
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.t1 TO test.t1_mid;" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.t1_mid (payload) VALUES ('d'),('e');" test
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_inc \
  --incremental-basedir=$topdir/backup_base \
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup_inc.log)&

job_pid=$!
pid_file=$topdir/backup_inc/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# Rename back to the original name inside the inc no-lock window.
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.t1_mid TO test.t1;" test
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.t1 (payload) VALUES ('f');" test

vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

if ! egrep -q 'DDL tracking : LSN: [0-9]* rename space ID: [0-9]* From: test/t1_mid.ibd To: test/t1.ibd' $topdir/backup_inc.log ; then
    die "xtrabackup did not record the cycle-back RENAME TABLE"
fi

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_base
xtrabackup --prepare --target-dir=$topdir/backup_base --incremental-dir=$topdir/backup_inc

record_db_state test
stop_server
rm -rf $mysql_datadir
mkdir $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup_base
start_server
verify_db_state test

EXISTS_ORIG=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='test' AND table_name='t1'"`
if [ "$EXISTS_ORIG" != "1" ]; then
    die "test.t1 should exist after restore, information_schema reports $EXISTS_ORIG"
fi
EXISTS_MID=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='test' AND table_name='t1_mid'"`
if [ "$EXISTS_MID" != "0" ]; then
    die "test.t1_mid must not exist after restore, information_schema reports $EXISTS_MID"
fi

# 3 + 2 (between full and inc) + 1 (inside inc window) = 6
ROWS=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM test.t1"`
if [ "$ROWS" != "6" ]; then
    die "test.t1 should have 6 rows after restore, got $ROWS"
fi

# Orphan .ibd under the intermediate name would yield ER_TABLESPACE_EXISTS.
if ! ${MYSQL} ${MYSQL_ARGS} -Ns -e "CREATE TABLE test.t1_mid (id INT PRIMARY KEY); DROP TABLE test.t1_mid;" ; then
    die "CREATE TABLE test.t1_mid after restore failed (orphan .ibd)"
fi

stop_server
rm -rf $mysql_datadir $topdir/backup_base $topdir/backup_inc $topdir/backup_inc.log
