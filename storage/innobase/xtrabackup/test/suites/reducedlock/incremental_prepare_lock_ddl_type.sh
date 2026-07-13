. inc/common.sh

require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.base_lock_ddl (
  id INT PRIMARY KEY AUTO_INCREMENT,
  name VARCHAR(64),
  payload CHAR(200)
) ENGINE=InnoDB; INSERT INTO test.base_lock_ddl(name, payload)
VALUES ('a', REPEAT('a', 200)), ('b', REPEAT('b', 200));" test

innodb_wait_for_flush_all

# The full backup intentionally uses the normal lock mode.  Incremental prepare
# must still process reduced-lock metadata from the incremental directory.
xtrabackup --backup --target-dir=$topdir/backup_base_lock_ddl --lock-ddl=ON

xtrabackup --backup --target-dir=$topdir/backup_inc_lock_ddl \
  --incremental-basedir=$topdir/backup_base_lock_ddl \
  --debug-sync="ddl_tracker_before_lock_ddl" --lock-ddl=REDUCED \
  2> >( tee $topdir/backup_inc_lock_ddl.log)&

job_pid=$!
pid_file=$topdir/backup_inc_lock_ddl/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

$MYSQL $MYSQL_ARGS -Ns -e "ALTER TABLE test.base_lock_ddl ADD INDEX name_idx(name);
  RENAME TABLE test.base_lock_ddl TO test.inc_lock_ddl;
  INSERT INTO test.inc_lock_ddl(name, payload) VALUES ('c', REPEAT('c', 200));" test

vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

egrep -q "^lock_ddl_type = ON" \
  $topdir/backup_base_lock_ddl/xtrabackup_info || \
  die "base backup did not record lock_ddl_type ON"
egrep -q "^lock_ddl_type = REDUCED" \
  $topdir/backup_inc_lock_ddl/xtrabackup_info || \
  die "incremental backup did not record lock_ddl_type REDUCED"

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_base_lock_ddl
xtrabackup --prepare --target-dir=$topdir/backup_base_lock_ddl \
  --incremental-dir=$topdir/backup_inc_lock_ddl \
  2> >( tee $topdir/prepare_inc_lock_ddl.log)

if ! egrep -q "Using incremental backup lock_ddl_type REDUCED .* target lock_ddl_type is ON" \
    $topdir/prepare_inc_lock_ddl.log ; then
  die "prepare did not switch to the incremental backup lock_ddl_type"
fi

original_count=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.inc_lock_ddl FORCE INDEX(name_idx);" | awk {'print $1'}`

record_db_state test
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup_base_lock_ddl
start_server

verify_db_state test
restored_count=`$MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM test.inc_lock_ddl FORCE INDEX(name_idx);" | awk {'print $1'}`

if [ "$original_count" != "$restored_count" ]; then
  die "rows in inc_lock_ddl via name_idx mismatch: original=$original_count restored=$restored_count"
fi

stop_server
rm -rf $mysql_datadir $topdir/backup_base_lock_ddl \
  $topdir/backup_inc_lock_ddl $topdir/backup_inc_lock_ddl.log \
  $topdir/prepare_inc_lock_ddl.log
