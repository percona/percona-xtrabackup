#
# 1. start backup and wait a moment after query p_s.log_status
# 2. insert data and wait for a moment
# 3. continue backup
# 4. restore
# 5. check data
#

if ! $XB_BIN --help 2>&1 | grep -q debug-sync; then
    skip_test "Requires --debug-sync support"
fi

start_server

function backup_local() {
	xtrabackup --backup --debug-sync=xtrabackup_after_query_log_status --target-dir=$topdir/backup
}

function prepare_local() {
	xtrabackup --prepare --target-dir=$topdir/backup
}


function restore() {
  [[ -d $mysql_datadir ]] && rm -rf $mysql_datadir && mkdir -p $mysql_datadir
  xtrabackup --copy-back --target-dir=$topdir/backup
}

function do_test() {

  cat <<EOF | $MYSQL $MYSQL_ARGS
CREATE DATABASE sakila;
use sakila;
CREATE TABLE t (a INT);
INSERT INTO t (a) VALUES (1), (2), (3);
EOF

  innodb_wait_for_flush_all

  (eval $backup_cmd) &
  job_pid=$!

  wait_for_xb_to_suspend $pid_file

  xb_pid=`cat $pid_file`

  checksum1=`checksum_table sakila t`
  $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO t (a) VALUES (10), (20), (30);" sakila
  $MYSQL $MYSQL_ARGS -Ns -e "DROP DATABASE sakila"

  innodb_wait_for_flush_all

  # Resume the xtrabackup process
  vlog "Resuming xtrabackup"
  kill -SIGCONT $xb_pid

  # wait's return code will be the code returned by the background process
  run_cmd wait $job_pid

  stop_server

  eval $prepare_cmd

  eval $restore_cmd

  start_server

  row_count=`$MYSQL $MYSQL_ARGS -Ns -e "select count(1) from t" sakila | awk {'print $1'}`

  vlog "row count in table t is $row_count"

  if [ "$row_count" != 3 ]; then
   vlog "rows in table t is $row_count when it should be 3"
   exit -1
  fi

  checksum2=`checksum_table sakila t`

  $MYSQL $MYSQL_ARGS -Ns -e "select * from t;" sakila

  vlog "Old checksum: $checksum1"
  vlog "New checksum: $checksum2"

  if [ "$checksum1" != "$checksum2" ]; then
  vlog "Checksums do not match"
  exit -1
  fi

  rm -rf $topdir/backup
  rm -rf $pid_file

}


vlog "##############################"
vlog "# Local backup"
vlog "##############################"

export backup_cmd=backup_local
export prepare_cmd=prepare_local
export restore_cmd=restore
export pid_file=$topdir/backup/xtrabackup_debug_sync
do_test
