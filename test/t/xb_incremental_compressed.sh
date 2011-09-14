################################################################################
# Test incremental backup with InnoDB compression
################################################################################

. inc/common.sh

init

if [ -z "$INNODB_VERSION" ]; then
    echo "Requires InnoDB plugin or XtraDB" >$SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

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
  mysqld_additional_args="--innodb_strict_mode --innodb_file_per_table \
      --innodb_file_format=Barracuda"
  
  run_mysqld ${mysqld_additional_args}

  load_dbase_schema incremental_sample

  vlog "Compressing the table"

  run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
      "ALTER TABLE test ENGINE=InnoDB ROW_FORMAT=compressed \
KEY_BLOCK_SIZE=$page_size" incremental_sample

  # Adding 10k rows

  vlog "Adding initial rows to database..."

  numrow=10000
  count=0
  while [ "$numrow" -gt "$count" ]; do
    sql="INSERT INTO test VALUES ($count, $numrow)"
    let "count=count+1"
    for ((i=0; $i<99; i++)); do
      sql="${sql},($count, $numrow)"
      let "count=count+1"
    done
    ${MYSQL} ${MYSQL_ARGS} -e "$sql" incremental_sample
  done

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

  xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/full

  vlog "Full backup done"

  # Changing data in sakila

  vlog "Making changes to database"

  let "count=numrow+1"
  let "numrow=15000"
  while [ "$numrow" -gt "$count" ]; do
    sql="INSERT INTO test VALUES ($count, $numrow)"
    let "count=count+1"
    for ((i=0; $i<99; i++)); do
      sql="${sql},($count, $numrow)"
      let "count=count+1"
    done
    ${MYSQL} ${MYSQL_ARGS} -e "$sql" incremental_sample
  done

  rows=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM test" \
incremental_sample`
  if [ "$rows" != "15000" ]; then
    vlog "Failed to add more rows"
    exit -1
  fi

  vlog "Changes done"

  # Saving the checksum of original table
  checksum_a=`checksum_table incremental_sample test`

  vlog "Table checksum is $checksum_a"

  vlog "Making incremental backup"

  # Incremental backup
  xtrabackup --datadir=$mysql_datadir --backup \
      --target-dir=$topdir/data/delta --incremental-basedir=$topdir/data/full

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
  vlog "Table cleared"
  ${MYSQL} ${MYSQL_ARGS} -e "delete from test;" incremental_sample

  # Restore backup

  stop_mysqld

  vlog "Copying files"

  cd $topdir/data/full/
  cp -r * $mysql_datadir
  cd -

  vlog "Data restored"

  run_mysqld ${mysqld_additional_args}

  vlog "Cheking checksums"
  checksum_b=`checksum_table incremental_sample test`

  if [ $checksum_a -ne $checksum_b  ]
      then 
      vlog "Checksums are not equal"
      exit -1
  fi

  vlog "Checksums are OK"

  drop_dbase incremental_sample
  stop_mysqld
}

for page_size in 1 2 4 8 16; do
  test_incremental_compressed ${page_size}
done
