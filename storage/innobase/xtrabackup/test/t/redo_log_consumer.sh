########################################################################
# register pxb as redo log consummer. Backup should complete
########################################################################
. inc/common.sh

require_server_version_higher_than 8.0.29
require_debug_pxb_version

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_capacity=8M
"

start_server

function run_inserts()
{
  for i in $(seq 1000);
  do
    $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.redo_log_consumer VALUES (NULL, REPEAT('a', 63 * 1024))";
  done
}

mysql -e "CREATE TABLE redo_log_consumer (
  id INT PRIMARY KEY AUTO_INCREMENT,
  str BLOB
);" test

vlog "Run without redo log consumer"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --target-dir=$topdir/backup \
  --debug-sync="xtrabackup_pause_after_redo_catchup" \
 2> >( tee $topdir/backup_before_redo_register.log)&
job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

echo "backup pid is $job_pid"

vlog "sufficient insert to remove old redo files "

run_inserts

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

rm -rf $topdir/backup

vlog "backup with redo log consumer"
xtrabackup --backup --target-dir=$topdir/backup --register-redo-log-consumer \
  --debug-sync="xtrabackup_pause_after_redo_catchup" \
  2> >( tee $topdir/backup_before_redo_register.log)&
job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

echo "backup pid is $job_pid"

run_inserts &
inserts_pid=$!

# Wait for backpressure: with 8M capacity and the registered PXB/MEB
# consumer pinned at the catch-up LSN, the server cannot reclaim redo
# files. Exit on the first of: a consumer-lagging warning anchored on
# the consumer name (avoids false matches from the prior backup's
# log_checkpointer warnings), or PFS reporting >= 3 redo files (proof
# we're past 8M with the consumer holding it back). Bound the wait so
# MTR fails with diagnostics instead of Jenkins killing the job.
max_wait_s=60
for ((i=1; i<=max_wait_s; i++)) ; do
	if grep -qE "(Redo log writer is waiting for (PXB|MEB) redo log consumer|'(PXB|MEB)' consumer still lagging behind)" \
			${MYSQLD_ERRFILE} ; then
		vlog "consumer-lagging warning observed in mysql error file"
		break
	fi

	n_files=$($MYSQL $MYSQL_ARGS -Ns -e \
		"SELECT COUNT(*) FROM performance_schema.innodb_redo_log_files" \
		2>/dev/null || echo 0)
	if [ "${n_files:-0}" -ge 3 ] ; then
		vlog "redo log accumulated ${n_files} files;" \
			"consumer is holding the server back as expected"
		break
	fi

	vlog "waiting for redo log backpressure" \
		"(#files=${n_files:-?}, ${i}/${max_wait_s}s)"
	sleep 1
done

if [ ${i} -gt ${max_wait_s} ] ; then
	vlog "ERROR: timed out waiting for redo log backpressure"
	vlog "performance_schema.innodb_redo_log_files at timeout:"
	$MYSQL $MYSQL_ARGS -t -e \
		"SELECT file_id, start_lsn, end_lsn, is_full FROM performance_schema.innodb_redo_log_files ORDER BY file_id" \
		>&2 || true
	vlog "tail of mysqld error file ${MYSQLD_ERRFILE}:"
	tail -100 ${MYSQLD_ERRFILE} >&2 || true
	kill ${inserts_pid} 2>/dev/null || true
	kill -SIGCONT ${xb_pid} 2>/dev/null || true
	die "redo_log_consumer: timeout waiting for backpressure"
fi

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

# Stop background inserts before stop_server to avoid ERROR 2002 spam.
kill ${inserts_pid} 2>/dev/null || true
wait ${inserts_pid} 2>/dev/null || true

xtrabackup --prepare --target-dir=$topdir/backup

# PXB-3147 - register redo log consumer fails with ANSI_QUOTES
stop_server

MYSQLD_EXTRA_MY_CNF_OPTS="
sql_mode = 'ANSI_QUOTES'
"

start_server
xtrabackup --backup --stream --register-redo-log-consumer > /dev/null
