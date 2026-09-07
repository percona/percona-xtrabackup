. inc/common.sh

require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.t1 (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50), pad CHAR(200)); INSERT INTO test.t1(name,pad) VALUES ('a',REPEAT('x',200)),('b',REPEAT('y',200));" test
for i in $(seq 1 16); do
  $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.t1(name,pad) SELECT name,pad FROM test.t1;" test
done

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_base --lock-ddl=REDUCED

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup_inc --incremental-basedir=$topdir/backup_base \
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup_inc.log)&

job_pid=$!
pid_file=$topdir/backup_inc/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

# ADD INDEX puts t1 into the recopy set; RENAME makes the recopy target a name
# that does not exist in the base backup.
$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.t1 ADD INDEX name_idx(name); RENAME TABLE test.t1 TO test.t2;" test

vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_base
xtrabackup --prepare --target-dir=$topdir/backup_base --incremental-dir=$topdir/backup_inc

original_count=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.t2 FORCE INDEX(name_idx);"`

record_db_state test
stop_server
rm -rf $mysql_datadir
mkdir $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup_base
start_server


$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.t2; CHECKSUM TABLE test.t2;" test
scan_rc=$?
if ! $MYSQL $MYSQL_ARGS -Ns -e "SELECT 1" >/dev/null 2>&1; then
    die "mysqld crashed while reading restored t2 (lost connection): the restored tablespace is corrupt"
fi
if [ "$scan_rc" != "0" ]; then
    die "full scan of restored t2 failed (rc=$scan_rc): the restored tablespace is corrupt"
fi

verify_db_state test

restored_count=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.t2 FORCE INDEX(name_idx);"`

if [ "$original_count" != "$restored_count" ]; then
    die "rows in t2 via secondary index mismatch: original=$original_count restored=$restored_count"
fi

stop_server
rm -rf $mysql_datadir $topdir/backup_base $topdir/backup_inc $topdir/backup_inc.log