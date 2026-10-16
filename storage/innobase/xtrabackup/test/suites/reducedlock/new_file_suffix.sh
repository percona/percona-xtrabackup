###############################################################################
# Preserve .new in general tablespace filenames and incremental parent paths.
# Exercise .new, .new.meta and .new.delta using real DDL and restored data.
###############################################################################

. inc/common.sh
require_debug_pxb_version
start_server

$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.seed(id INT PRIMARY KEY) ENGINE=InnoDB;
  INSERT INTO test.seed VALUES(1);"
innodb_wait_for_flush_all

xtrabackup --backup --target-dir="$topdir/full" --lock-ddl=REDUCED \
  --debug-sync=ddl_tracker_before_lock_ddl 2> >(tee "$topdir/full.log" >&2) &
job_pid=$!
wait_for_xb_to_suspend "$topdir/full/xtrabackup_debug_sync"
$MYSQL $MYSQL_ARGS -e "CREATE TABLESPACE suffix_ts
    ADD DATAFILE 'suffix.new.ibd' ENGINE=InnoDB;
  CREATE TABLE test.full_table(id INT PRIMARY KEY) TABLESPACE suffix_ts;
  INSERT INTO test.full_table VALUES(11),(22);"
kill -CONT "$(cat "$topdir/full/xtrabackup_debug_sync")"
run_cmd wait $job_pid
[ -f "$topdir/full/suffix.new.ibd.new" ] || die "full .new was not generated"

mkdir "$topdir/backup.new"
xtrabackup --backup --target-dir="$topdir/backup.new/inc" \
  --incremental-basedir="$topdir/full" --lock-ddl=REDUCED \
  --debug-sync=ddl_tracker_before_lock_ddl 2> >(tee "$topdir/inc.log" >&2) &
job_pid=$!
wait_for_xb_to_suspend "$topdir/backup.new/inc/xtrabackup_debug_sync"
$MYSQL $MYSQL_ARGS -e "CREATE TABLESPACE suffix_inc_ts
    ADD DATAFILE 'suffix_inc.new.ibd' ENGINE=InnoDB;
  CREATE TABLE test.inc_table(id INT PRIMARY KEY) TABLESPACE suffix_inc_ts;
  INSERT INTO test.inc_table VALUES(33),(44),(55);"
kill -CONT "$(cat "$topdir/backup.new/inc/xtrabackup_debug_sync")"
run_cmd wait $job_pid
[ -f "$topdir/backup.new/inc/suffix_inc.new.ibd.new.meta" ] || die "new.meta was not generated"
[ -f "$topdir/backup.new/inc/suffix_inc.new.ibd.new.delta" ] || die "new.delta was not generated"

xtrabackup --prepare --apply-log-only --target-dir="$topdir/full"
[ -f "$topdir/full/suffix.new.ibd" ] || die "full prepare changed datafile name"
[ ! -e "$topdir/full/suffix.ibd.new" ] || die "full prepare stripped wrong .new"
xtrabackup --prepare --target-dir="$topdir/full" \
  --incremental-dir="$topdir/backup.new/inc"
[ -f "$topdir/full/suffix_inc.new.ibd" ] || die "incremental prepare changed datafile name"
[ -f "$topdir/backup.new/inc/suffix_inc.new.ibd.meta" ] || die "meta moved to wrong parent path"
[ -f "$topdir/backup.new/inc/suffix_inc.new.ibd.delta" ] || die "delta moved to wrong parent path"
[ ! -e "$topdir/backup" ] || die "prepare modified the incremental parent path"

record_db_state test
stop_server
rm -rf "$mysql_datadir"
mkdir "$mysql_datadir"
xtrabackup --copy-back --target-dir="$topdir/full"
start_server
verify_db_state test
rows=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*), SUM(id) FROM test.full_table")
[ "$rows" = $'2\t33' ] || die "full general tablespace has wrong rows: $rows"
rows=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*), SUM(id) FROM test.inc_table")
[ "$rows" = $'3\t132' ] || die "incremental general tablespace has wrong rows: $rows"
stop_server
