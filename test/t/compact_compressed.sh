##########################################################################
# Basic tests for --compact on compressed tables                         #
##########################################################################

. inc/common.sh

if [ -z "$INNODB_VERSION" ]; then
    echo "Requires InnoDB plugin or XtraDB" >$SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

# Use innodb_strict_mode so that failure to use compression results in an 
# error rather than a warning
mysqld_additional_args="--innodb_strict_mode --innodb_file_per_table \
--innodb_file_format=Barracuda"

#
# Test compact backup of a compressed tablespace with a specific page size
#
function test_compact_compressed()
{
  local page_size=$1
  local backup_dir
  
  echo "************************************************************************"
  echo "Testing compact backup with compressed page size=${page_size}KB "
  echo "************************************************************************"

  start_server ${mysqld_additional_args}

  load_dbase_schema sakila
  load_dbase_data sakila

  backup_dir="$topdir/backup"
  
  vlog "Compressing tables"

  table_list=`${MYSQL} ${MYSQL_ARGS} -Ns -e \
"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='sakila' \
AND TABLE_TYPE='BASE TABLE' AND ENGINE='InnoDB'"`

  for t in $table_list; do
      run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
	  "ALTER TABLE $t ENGINE=InnoDB ROW_FORMAT=compressed \
           KEY_BLOCK_SIZE=$page_size" sakila
  done

  innobackupex --no-timestamp --compact $backup_dir
  record_db_state sakila

  stop_server

  rm -r $mysql_datadir

  innobackupex --apply-log --rebuild-indexes $backup_dir

  vlog "Restoring MySQL datadir"
  mkdir -p $mysql_datadir
  innobackupex --copy-back $backup_dir

  start_server

  verify_db_state sakila

  stop_server
  clean
}


# Cannot test with lower page sizes, because not all tables can be compressed
# with KEY_BLOCK_SIZE < 8. This can be changed, if we implement 'canned'
# databases in the test suite, which will contain tables compressed with the
# minimum possible page size.
for page_size in 8 16; do
  test_compact_compressed ${page_size}
done

