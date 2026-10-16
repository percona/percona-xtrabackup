###############################################################################
# Incremental --lock-ddl=REDUCED: a tablespace created before the inc backup,
# captured by the inc copy phase, then ADD-INDEX-ed (inplace) and DROP-ed
# inside the same no-lock window must not reappear in the prepared backup.
# handle_ddl_operations() must still emit a .del marker even when the recopy
# loop has just put the sid back into new_tables.
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

xtrabackup --backup --target-dir=$topdir/backup_base --lock-ddl=REDUCED
innodb_wait_for_flush_all

# Create the table between full and inc so the inc copy phase produces
# t1.ibd.delta + t1.ibd.meta.
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.t1 (id INT PRIMARY KEY AUTO_INCREMENT, payload VARCHAR(32)); INSERT INTO test.t1 (payload) VALUES ('a'),('b'),('c');" test
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

# MLOG_INDEX_LOAD followed by DROP inside the same no-lock window.
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.t1 ADD INDEX idx_payload(payload), ALGORITHM=INPLACE;" test
$MYSQL $MYSQL_ARGS -Ns -e "DROP TABLE test.t1;" test

vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_base
xtrabackup --prepare --target-dir=$topdir/backup_base --incremental-dir=$topdir/backup_inc

record_db_state test
stop_server
rm -rf $mysql_datadir
mkdir $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup_base
start_server
verify_db_state test

EXISTS=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='test' AND table_name='t1'"`
if [ "$EXISTS" != "0" ]; then
    die "test.t1 should be absent after restore, information_schema reports $EXISTS"
fi

# Orphan .ibd from apply_delta would yield ER_TABLESPACE_EXISTS.
if ! ${MYSQL} ${MYSQL_ARGS} -Ns -e "CREATE TABLE test.t1 (id INT PRIMARY KEY, note VARCHAR(16)); INSERT INTO test.t1 VALUES (1,'fresh'); DROP TABLE test.t1;" ; then
    die "CREATE TABLE test.t1 after restore failed (orphan .ibd)"
fi

stop_server
rm -rf $mysql_datadir $topdir/backup_base $topdir/backup_inc $topdir/backup_inc.log
