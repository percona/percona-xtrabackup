###############################################################################
# A concurrent RENAME may cross a --tables filter boundary. The backup must
# reflect the final, locked name rather than retaining or omitting the space.
###############################################################################

. inc/common.sh
require_debug_pxb_version
start_server

# Included -> excluded: remove the already-copied old-name tablespace.
$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.included_before(id INT PRIMARY KEY);
  INSERT INTO test.included_before VALUES(1),(2)"
innodb_wait_for_flush_all
xtrabackup --backup --target-dir="$topdir/outgoing" \
  --tables='test.included_before' --lock-ddl=REDUCED \
  --debug-sync=ddl_tracker_before_lock_ddl &
job_pid=$!
wait_for_xb_to_suspend "$topdir/outgoing/xtrabackup_debug_sync"
$MYSQL $MYSQL_ARGS -e \
  "RENAME TABLE test.included_before TO test.excluded_after"
kill -CONT "$(cat "$topdir/outgoing/xtrabackup_debug_sync")"
run_cmd wait "$job_pid"
find "$topdir/outgoing" -name '*.del' | grep -q . || \
  die "included-to-excluded rename did not emit a delete marker"
xtrabackup --prepare --export --target-dir="$topdir/outgoing"
[ ! -e "$topdir/outgoing/test/included_before.ibd" ] || \
  die "partial backup retained the excluded old-name tablespace"

# Excluded -> included: copy the final-name tablespace under the backup lock.
$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.excluded_before(id INT PRIMARY KEY);
  INSERT INTO test.excluded_before VALUES(3),(4),(5)"
innodb_wait_for_flush_all
xtrabackup --backup --target-dir="$topdir/incoming" \
  --tables='test.included_after' --lock-ddl=REDUCED \
  --debug-sync=ddl_tracker_before_lock_ddl &
job_pid=$!
wait_for_xb_to_suspend "$topdir/incoming/xtrabackup_debug_sync"
$MYSQL $MYSQL_ARGS -e \
  "RENAME TABLE test.excluded_before TO test.included_after"
kill -CONT "$(cat "$topdir/incoming/xtrabackup_debug_sync")"
run_cmd wait "$job_pid"
[ -f "$topdir/incoming/test/included_after.ibd.new" ] || \
  die "excluded-to-included rename did not copy the final tablespace"
xtrabackup --prepare --export --target-dir="$topdir/incoming"
[ -f "$topdir/incoming/test/included_after.ibd" ] || \
  die "partial backup is missing the included final-name tablespace"

stop_server
