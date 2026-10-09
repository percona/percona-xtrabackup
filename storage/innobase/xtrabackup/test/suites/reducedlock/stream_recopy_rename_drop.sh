###############################################################################
# A recopied, renamed, and then dropped tablespace must emit exactly one .del
# entry. Duplicate paths make an xbstream backup impossible to extract.
###############################################################################

. inc/common.sh

require_debug_pxb_version
start_server

assert_absent_and_reusable() {
  local table_name=$1
  local exists

  exists=`${MYSQL} ${MYSQL_ARGS} -Ns -e \
    "SELECT COUNT(*) FROM information_schema.tables
     WHERE table_schema='test' AND table_name='${table_name}'"`
  if [ "$exists" != "0" ]; then
    die "test.${table_name} should be absent after restore"
  fi

  $MYSQL $MYSQL_ARGS -Ns -e \
    "CREATE TABLE test.${table_name} (id INT PRIMARY KEY, payload VARCHAR(16));
     INSERT INTO test.${table_name} VALUES (1,'fresh');
     DROP TABLE test.${table_name};"
}

# Full streamed backup.
$MYSQL $MYSQL_ARGS -Ns -e \
  "CREATE TABLE test.t_full
     (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50));
   INSERT INTO test.t_full (name) VALUES ('a'),('b'),('c');"
innodb_wait_for_flush_all

xtrabackup --backup --stream=xbstream --target-dir=$topdir/full_meta \
  --lock-ddl=REDUCED --debug-sync=ddl_tracker_before_lock_ddl \
  > $topdir/full.xbs 2> >(tee $topdir/full.log >&2) &

job_pid=$!
pid_file=$topdir/full_meta/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

$MYSQL $MYSQL_ARGS -Ns -e \
  "ALTER TABLE test.t_full ADD INDEX name_idx(name), ALGORITHM=INPLACE;
   RENAME TABLE test.t_full TO test.t_full_renamed;
   DROP TABLE test.t_full_renamed;"

kill -SIGCONT $xb_pid
run_cmd wait $job_pid

mkdir $topdir/full
run_cmd xbstream -x -C $topdir/full < $topdir/full.xbs
xtrabackup --prepare --target-dir=$topdir/full

record_db_state test
stop_server
rm -rf $mysql_datadir
mkdir $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full
start_server
verify_db_state test
assert_absent_and_reusable t_full

# Incremental streamed backup.
$MYSQL $MYSQL_ARGS -Ns -e \
  "CREATE TABLE test.t_inc
     (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(50));
   INSERT INTO test.t_inc (name) VALUES ('a'),('b'),('c');"
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/base --lock-ddl=REDUCED

xtrabackup --backup --stream=xbstream --target-dir=$topdir/inc_meta \
  --incremental-basedir=$topdir/base --lock-ddl=REDUCED \
  --debug-sync=ddl_tracker_before_lock_ddl \
  > $topdir/inc.xbs 2> >(tee $topdir/inc.log >&2) &

job_pid=$!
pid_file=$topdir/inc_meta/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

$MYSQL $MYSQL_ARGS -Ns -e \
  "ALTER TABLE test.t_inc ADD INDEX name_idx(name), ALGORITHM=INPLACE;
   RENAME TABLE test.t_inc TO test.t_inc_renamed;
   DROP TABLE test.t_inc_renamed;"

kill -SIGCONT $xb_pid
run_cmd wait $job_pid

mkdir $topdir/inc
run_cmd xbstream -x -C $topdir/inc < $topdir/inc.xbs
xtrabackup --prepare --apply-log-only --target-dir=$topdir/base
xtrabackup --prepare --target-dir=$topdir/base --incremental-dir=$topdir/inc

record_db_state test
stop_server
rm -rf $mysql_datadir
mkdir $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/base
start_server
verify_db_state test
assert_absent_and_reusable t_inc
