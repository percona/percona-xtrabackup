################################################################################
# Test incremental backup with InnoDB compression
################################################################################
# Expects the following variable to be set before including:
#    mysqld_extra_args: an extra arg to be passed for all mysqld invocations.  
#                       Use this to set special options that influence 
#                       incremental backups, e.g. turns on log archiving or 
#                       changed page bitmap output.
#    first_inc_suspend_command: if non-empty string, passes --suspend-at-start 
#                               to incremental XB invocation and executes the 
#                               given command while it's suspended

. inc/common.sh

function add_rows()
{
  local table=$1
  local start=$2
  local limit=$3

  while [ "$limit" -gt "$start" ]; do
    sql="INSERT INTO $table VALUES ($start, $limit)"
    let "start=start+1"
    for ((i=0; $i<99; i++)); do
      sql="${sql},($start, $limit)"
      let "start=start+1"
    done
    ${MYSQL} ${MYSQL_ARGS} -e "$sql" incremental_sample
  done
}

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
  mysqld_additional_args="$mysqld_extra_args --innodb_strict_mode \
      --innodb_file_per_table --innodb_file_format=Barracuda \
      --innodb_max_dirty_pages_pct=0 --innodb_log_file_size=1M"
  
  start_server ${mysqld_additional_args}

  load_dbase_schema incremental_sample

  vlog "Compressing the table"

  run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
      "ALTER TABLE test ENGINE=InnoDB ROW_FORMAT=compressed \
KEY_BLOCK_SIZE=$page_size" incremental_sample

  # Adding 10k rows

  vlog "Adding initial rows to database..."

  add_rows test 0 10000

  rows=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM test" \
incremental_sample`
  if [ "$rows" != "10000" ]; then
    vlog "Failed to add initial rows"
    exit -1
  fi

  vlog "Initial rows added"

  # Full backup

  # Full backup folder
  rm -rf $topdir/data/full
  mkdir -p $topdir/data/full
  # Incremental data
  rm -rf $topdir/data/delta
  mkdir -p $topdir/data/delta

  vlog "Starting backup"

  xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/full ${mysqld_additional_args}

  vlog "Full backup done"

  # Changing data in sakila

  vlog "Making changes to database"

  ${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t2 (a INT(11) DEFAULT NULL, \
number INT(11) DEFAULT NULL) ENGINE=INNODB \
ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=$page_size" incremental_sample

  add_rows test 10001 12500
  add_rows t2 10001 12500

  # Rotate bitmap file here and force checkpoint at the same time
  shutdown_server
  start_server ${mysqld_additional_args}

  add_rows test 12501 15000
  add_rows t2 12501 15000

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
      suspend_arg="--suspend_at_start";
  else
      suspend_arg="";
  fi
 
  # Incremental backup
  xtrabackup --datadir=$mysql_datadir --backup \
      --target-dir=$topdir/data/delta --incremental-basedir=$topdir/data/full \
      ${mysqld_additional_args} $suspend_arg &

  xb_pid=$!

  if [ -n "$first_inc_suspend_command" ]
  then
      wait_for_xb_to_suspend $topdir/data/delta/xtrabackup_suspended_1
      ${MYSQL} ${MYSQL_ARGS} -e "$first_inc_suspend_command"

      resume_suspended_xb $topdir/data/delta/xtrabackup_suspended_1
  fi

  wait $xb_pid

  vlog "Incremental backup done"
  vlog "Preparing backup"

  # Prepare backup
  xtrabackup --datadir=$mysql_datadir --prepare \
      --apply-log-only --target-dir=$topdir/data/full ${mysqld_additional_args}
  vlog "Log applied to backup"
  xtrabackup --datadir=$mysql_datadir --prepare \
      --apply-log-only --target-dir=$topdir/data/full \
      --incremental-dir=$topdir/data/delta ${mysqld_additional_args}
  vlog "Delta applied to backup"
  xtrabackup --datadir=$mysql_datadir --prepare \
      --target-dir=$topdir/data/full ${mysqld_additional_args}
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

  start_server ${mysqld_additional_args}

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
