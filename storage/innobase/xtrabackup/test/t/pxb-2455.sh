#
# 1. restore backup of 8.0.22
# 2. check data
# 3. do increment and see if restore works fine

MYSQLD_EXTRA_MY_CNF_OPTS="
lower_case_table_names
"
start_server
stop_server

function backup_inc_local() {
	xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/backup
}

function prepare_local() {
	xtrabackup --prepare --target-dir=$topdir/backup
}

function prepare_inc_local() {

	xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
	xtrabackup --prepare --target-dir=$topdir/backup --incremental-dir=$topdir/inc

}


function restore() {
  [[ -d $mysql_datadir ]] && rm -rf $mysql_datadir && mkdir -p $mysql_datadir
  xtrabackup --copy-back --target-dir=$topdir/backup
}

function do_test() {

  mkdir -p $topdir

  tar -xf inc/pxb-2455-back.tar.gz  -C $topdir

  eval $prepare_cmd

  eval $restore_cmd

  start_server

  $MYSQL $MYSQL_ARGS -Ns -e "select * from t1;" test

  row_count=`$MYSQL $MYSQL_ARGS -Ns -e "select count(1) from t1" test | awk {'print $1'}`

  vlog "row count in table t is $row_count"

  if [ "$row_count" != 0 ]; then
   vlog "rows in table t1 is $row_count when it should be 0"
   exit -1
  fi

  rm -rf $topdir/backup

  tar -xf inc/pxb-2455-back.tar.gz  -C $topdir

  $MYSQL $MYSQL_ARGS -Ns -e "insert into t1 values(100)" test

  eval $backup_inc_cmd
  eval $prepare_inc_cmd

  stop_server

  eval $restore_cmd

  start_server

  $MYSQL $MYSQL_ARGS -Ns -e "select * from t1;" test

  row_count=`$MYSQL $MYSQL_ARGS -Ns -e "select count(1) from t1" test | awk {'print $1'}`

  vlog "row count in table t is $row_count"

  if [ "$row_count" != 1 ]; then
   vlog "rows in table t1 is $row_count when it should be 1"
   exit -1
  fi



}


vlog "##############################"
vlog "# backup restore from 8.0.22 data directory"
vlog "##############################"

export backup_inc_cmd=backup_inc_local
export prepare_cmd=prepare_local
export prepare_inc_cmd=prepare_inc_local
export restore_cmd=restore
export pid_file=$topdir/backup/xtrabackup_debug_sync
do_test
