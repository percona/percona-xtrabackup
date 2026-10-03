###############################################################################
# Verifies that when a tablespace becomes corrupted during backup but is NEVER
# covered by a DDL (no drop, no recopy trigger), xtrabackup must fail rather
# than produce a "successful" backup with a phantom .crpt that destroys the
# table at prepare time.
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "
  CREATE TABLE test.t (
    id INT PRIMARY KEY AUTO_INCREMENT,
    payload CHAR(200)) ENGINE=InnoDB;
  INSERT INTO test.t (payload) VALUES
    (REPEAT('x',200)),(REPEAT('y',200)),(REPEAT('z',200));
" test
innodb_wait_for_flush_all

# Pause xtrabackup right after tablespace discovery, before data copy starts.
xtrabackup --backup --target-dir=$topdir/backup_corrupt \
  --debug-sync="xtrabackup_load_tablespaces_pause" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup_corrupt.log)&

job_pid=$!
pid_file=$topdir/backup_corrupt/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

# Inject corruption into page 4 of the source .ibd (16K page size assumed).
# 200 bytes of /dev/urandom is enough to break the checksum reliably.
TABLE_FILE=$mysql_datadir/test/t.ibd
[ -f "$TABLE_FILE" ] || die "expected $TABLE_FILE to exist"
dd if=/dev/urandom of=$TABLE_FILE bs=1 count=200 seek=65536 conv=notrunc \
   status=none

# Resume xtrabackup. NO DDL is issued, so the corrupted sid will NOT enter
# drops nor new_tables.
kill -SIGCONT $xb_pid

# After fix: backup must fail. Use 'wait' and capture exit code.
set +e
wait $job_pid
xb_rc=$?
set -e

if [ $xb_rc -eq 0 ]; then
    die "xtrabackup unexpectedly succeeded with an uncovered corrupted tablespace"
fi

# Sanity check on the error message produced by handle_ddl_operations.
if ! grep -q "corrupted tablespace" $topdir/backup_corrupt.log; then
    die "expected 'corrupted tablespace' error in backup log"
fi

vlog "OK: backup correctly aborted on uncovered corrupted tablespace"