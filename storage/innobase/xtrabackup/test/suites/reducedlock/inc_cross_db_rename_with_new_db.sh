###############################################################################
# Incremental --lock-ddl=REDUCED: destination DB is created inside the inc
# no-lock window, then a table is cross-db renamed into it. prepare must
# materialize the destination directory and move the base .ibd under the new
# schema; otherwise the restored instance misses the table or leaves orphan
# files behind.
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.cross_db_t (id INT PRIMARY KEY AUTO_INCREMENT, payload VARCHAR(32)); INSERT INTO test.cross_db_t VALUES (1,'a'),(2,'b'),(3,'c');" test
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_base --lock-ddl=REDUCED
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

# Inside the inc no-lock window: create the destination DB and move the table
# across. CREATE DATABASE itself leaves no MLOG_FILE_* signal for ddl_tracker.
$MYSQL $MYSQL_ARGS -Ns -e "CREATE DATABASE test_dst;"
$MYSQL $MYSQL_ARGS -Ns -e "RENAME TABLE test.cross_db_t TO test_dst.cross_db_t;"
$MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test_dst.cross_db_t VALUES (4,'d');" test_dst

vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

if ! egrep -q 'DDL tracking : LSN: [0-9]* rename space ID: [0-9]* From: test/cross_db_t.ibd To: test_dst/cross_db_t.ibd' $topdir/backup_inc.log ; then
    die "xtrabackup did not record the cross-database RENAME TABLE during the incremental backup"
fi

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_base
xtrabackup --prepare --target-dir=$topdir/backup_base --incremental-dir=$topdir/backup_inc

record_db_state test_dst
stop_server
rm -rf $mysql_datadir
mkdir $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup_base
start_server
verify_db_state test_dst

ROWS=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM test_dst.cross_db_t"`
if [ "$ROWS" != "4" ]; then
    die "test_dst.cross_db_t should have 4 rows after restore, got $ROWS"
fi

EXISTS_OLD=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='test' AND table_name='cross_db_t'"`
if [ "$EXISTS_OLD" != "0" ]; then
    die "test.cross_db_t should not exist after incremental restore, but information_schema reports it"
fi

if ! ${MYSQL} ${MYSQL_ARGS} -Ns -e "DROP TABLE test_dst.cross_db_t; CREATE TABLE test_dst.cross_db_t (id INT PRIMARY KEY); INSERT INTO test_dst.cross_db_t VALUES (1); SELECT COUNT(*) FROM test_dst.cross_db_t;" ; then
    die "CREATE TABLE test_dst.cross_db_t after incremental restore failed — likely orphan .ibd left over from cross-db RENAME"
fi

stop_server
rm -rf $mysql_datadir $topdir/backup_base $topdir/backup_inc $topdir/backup_inc.log
