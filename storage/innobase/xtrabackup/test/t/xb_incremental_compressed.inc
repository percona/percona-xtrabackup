################################################################################
# Test incremental backup with InnoDB compression
################################################################################
# Expects the following variable to be set before including:
#    MYSQLD_EXTRA_MY_CNF_OPTS: an extra arg to be passed for all mysqld invocations.  
#                       Use this to set special options that influence 
#                       incremental backups, e.g. turns on log archiving or 
#                       changed page bitmap output.
#    first_inc_suspend_command: if non-empty string, suspends incremental XB
#                               invocation at start and executes the 
#                               given command while it's suspended

. inc/common.sh

#
# Test incremental backup of a compressed tablespace with a specific page size
#
function test_incremental_compressed()
{
  page_size=$1
  
  echo "************************************************************************"
  echo "Testing incremental backup with compressed page size=${page_size}KB "
  echo "************************************************************************"

  # Use innodb_strict_mode so that failure to use compression results in an 
  # error rather than a warning
  MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS:-""}
innodb_strict_mode
innodb_file_per_table
innodb_file_format=Barracuda
innodb_max_dirty_pages_pct=0
innodb_log_file_size=4M"
  
  start_server

  load_dbase_schema incremental_sample

  vlog "Compressing the table"

  run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
      "ALTER TABLE test ENGINE=InnoDB ROW_FORMAT=compressed \
KEY_BLOCK_SIZE=$page_size" incremental_sample

  # Adding 10k rows

  multi_row_insert incremental_sample.test \({1..10000},10000\)

  # Full backup

  # Full backup folder
  rm -rf $topdir/data/full
  mkdir -p $topdir/data/full
  # Incremental data
  rm -rf $topdir/data/delta
  mkdir -p $topdir/data/delta

  vlog "Starting backup"

  xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/full

  vlog "Full backup done"

  # Changing data in sakila

  vlog "Making changes to database"

  ${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t2 (a INT(11) DEFAULT NULL, \
number INT(11) DEFAULT NULL) ENGINE=INNODB \
ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=$page_size" incremental_sample

  multi_row_insert incremental_sample.test \({10001..12500},12500\)
  multi_row_insert incremental_sample.t2 \({10001..12500},12500\)

  # Rotate bitmap file here and force checkpoint at the same time
  shutdown_server
  start_server

  multi_row_insert incremental_sample.test \({12501..15000},15000\)
  multi_row_insert incremental_sample.t2 \({12501..15000},15000\)

  rows=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM test" \
incremental_sample`
  if [ "$rows" != "15000" ]; then
    vlog "Failed to add more rows to 'test'"
    exit -1
  fi

  rows=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM t2" \
incremental_sample`
  if [ "$rows" != "5000" ]; then
    vlog "Failed to add more rows to 't2'"
    exit -1
  fi
  
  vlog "Changes done"

  # Saving the checksum of original table
  checksum_test_a=`checksum_table incremental_sample test`
  checksum_t2_a=`checksum_table incremental_sample t2`

  vlog "Table 'test' checksum is $checksum_test_a"
  vlog "Table 't2' checksum is $checksum_t2_a"

  vlog "Making incremental backup"

  if [ -n "$first_inc_suspend_command" ]
  then
      suspend_arg="--debug-sync=xtrabackup_suspend_at_start";
  else
      suspend_arg="";
  fi
 
  # Incremental backup
  xtrabackup --datadir=$mysql_datadir --backup \
      --target-dir=$topdir/data/delta --incremental-basedir=$topdir/data/full \
      $suspend_arg &

  xb_pid=$!

  if [ -n "$first_inc_suspend_command" ]
  then
      wait_for_xb_to_suspend $topdir/data/delta/xtrabackup_debug_sync
      ${MYSQL} ${MYSQL_ARGS} -e "$first_inc_suspend_command"

      resume_suspended_xb $topdir/data/delta/xtrabackup_debug_sync
  fi

  wait $xb_pid

  vlog "Incremental backup done"
  vlog "Preparing backup"

  # Prepare backup
  xtrabackup --datadir=$mysql_datadir --prepare \
      --apply-log-only --target-dir=$topdir/data/full
  vlog "Log applied to backup"
  xtrabackup --datadir=$mysql_datadir --prepare \
      --apply-log-only --target-dir=$topdir/data/full \
      --incremental-dir=$topdir/data/delta
  vlog "Delta applied to backup"
  xtrabackup --datadir=$mysql_datadir --prepare \
      --target-dir=$topdir/data/full
  vlog "Data prepared for restore"

  # removing rows
  ${MYSQL} ${MYSQL_ARGS} -e "delete from test;" incremental_sample
  ${MYSQL} ${MYSQL_ARGS} -e "delete from t2;" incremental_sample
  vlog "Tables cleared"

  # Restore backup

  shutdown_server
  rm -f $mysql_datadir/ib_modified_log*

  vlog "Copying files"

  cd $topdir/data/full/
  cp -r * $mysql_datadir
  cd -

  vlog "Data restored"

  start_server

  vlog "Checking checksums"
  checksum_test_b=`checksum_table incremental_sample test`
  checksum_t2_b=`checksum_table incremental_sample t2`

  if [ "$checksum_test_a" != "$checksum_test_b"  ]
      then 
      vlog "Checksums of table 'test' are not equal"
      exit -1
  fi

  if [ "$checksum_t2_a" != "$checksum_t2_b"  ]
      then 
      vlog "Checksums of table 't2' are not equal"
      exit -1
  fi

  vlog "Checksums are OK"

  stop_server
}
