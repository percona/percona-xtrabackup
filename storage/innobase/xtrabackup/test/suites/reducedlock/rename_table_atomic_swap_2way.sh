###############################################################################
# Atomic two-table swap (RENAME TABLE t1 TO __t, t2 TO t1, __t TO t2) inside
# a --lock-ddl=REDUCED backup window. The .ren entries form a closed graph
# and prepare_handle_ren_files() must rename through a unique intermediate
# name to avoid clobbering an already-renamed file.
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

# Distinct row counts so a silent overwrite produces a wrong row count
# instead of an undetectable permutation.
$MYSQL $MYSQL_ARGS -Ns -e "
CREATE TABLE test.t1 (id INT PRIMARY KEY, tag VARCHAR(8)) ENGINE=InnoDB;
CREATE TABLE test.t2 (id INT PRIMARY KEY, tag VARCHAR(8)) ENGINE=InnoDB;
INSERT INTO test.t1 VALUES (1,'a1'),(2,'a2');
INSERT INTO test.t2 VALUES (1,'b1'),(2,'b2'),(3,'b3'),(4,'b4'),(5,'b5');
" test

innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup.log)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

$MYSQL $MYSQL_ARGS -Ns -e "
RENAME TABLE test.t1 TO test.t_tmp,
             test.t2 TO test.t1,
             test.t_tmp TO test.t2;" test

vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

xtrabackup --prepare --target-dir=$topdir/backup
record_db_state test
stop_server
rm -rf $mysql_datadir
mkdir $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup
start_server
verify_db_state test

# After t1 <-> t2 swap : t1=5, t2=2.
T1_ROWS=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM test.t1"`
T2_ROWS=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM test.t2"`
if [ "$T1_ROWS" != "5" ] || [ "$T2_ROWS" != "2" ]; then
    die "atomic 2-way swap produced wrong rows: t1=$T1_ROWS (want 5) t2=$T2_ROWS (want 2)"
fi

stop_server
rm -rf $mysql_datadir $topdir/backup $topdir/backup.log
