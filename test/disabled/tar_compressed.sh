################################################################################
# Test of streaming backups with InnoDB compression
################################################################################

. inc/common.sh

if [ "$MYSQL_VERSION" = "5.0" ]; then
  exit $SKIPPED_EXIT_CODE
fi

function test_incremental_compressed()
{
  page_size=$1
  
  echo "************************************************************************"
  echo "Testing streaming backup with compressed page size=${page_size}KB "
  echo "************************************************************************"

  # Use innodb_strict_mode so that failure to use compression results in an 
  # error rather than a warning
  if [ "$MYSQL_VERSION" = "5.1" ]; then
    mysqld_additional_args="--ignore_builtin_innodb --innodb_strict_mode --plugin-load=innodb=ha_innodb_plugin.so  --innodb_file_per_table --innodb_file_format=Barracuda"
  else
    mysqld_additional_args="--innodb_strict_mode --innodb_file_per_table --innodb_file_format=Barracuda"
  fi
  
  run_mysqld ${mysqld_additional_args}
  load_dbase_schema incremental_sample

  vlog "Compressing the table"

  run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
      "ALTER TABLE test ENGINE=InnoDB ROW_FORMAT=compressed KEY_BLOCK_SIZE=$page_size" incremental_sample

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

  rows=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM test" incremental_sample`
  if [ "$rows" != "10000" ]; then
    vlog "Failed to add initial rows"
    exit -1
  fi
  vlog "Initial rows added"

  # Create directory for backup files
  rm -rf $topdir/data/full
  mkdir -p $topdir/data/full

  # Start streaming backuop
  vlog "Starting backup"
  ${IB_BIN} --socket=$mysql_socket --stream=tar $topdir/data/full > $topdir/data/full/out.tar 2>$OUTFILE

  # Saving the checksum of original table
  checksum_a=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" incremental_sample | awk '{print $2}'`
  vlog "Table checksum is $checksum_a"


  # Prepare backup
  vlog "Preparing backup"
  cd $topdir/data/full
  tar -ixf out.tar
  cd - > /dev/null 2>&1
  run_cmd ${IB_BIN} --apply-log $topdir/data/full >> $OUTFILE 2>&1
  vlog "Data prepared for restore"

  # removing rows
  vlog "Table cleared"
  ${MYSQL} ${MYSQL_ARGS} -e "delete from test;" incremental_sample

  # Restore backup
  stop_mysqld
  vlog "Copying files"
  run_cmd ${IB_BIN} --copy-back $topdir/data/full >> $OUTFILE 2>&1
  vlog "Data restored"

  # Comparing checksums
  run_mysqld ${mysqld_additional_args}
  vlog "Cheking checksums"
  checksum_b=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" incremental_sample | awk '{print $2}'`

  if [ $checksum_a -ne $checksum_b  ]
      then 
      vlog "Checksums are not equal"
      exit -1
  fi

  vlog "Checksums are OK"

  drop_dbase incremental_sample
  stop_mysqld
}

init

for page_size in 1 2 4 8 16; do
  test_incremental_compressed ${page_size}
done

clean
