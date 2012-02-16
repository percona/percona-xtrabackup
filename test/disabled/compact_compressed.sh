##########################################################################
# Basic tests for --compact on compressed tables                         #
##########################################################################

. inc/common.sh

init

if [ -z "$INNODB_VERSION" ]; then
    echo "Requires InnoDB plugin or XtraDB" >$SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

# Use innodb_strict_mode so that failure to use compression results in an 
# error rather than a warning
mysqld_additional_args="--innodb_strict_mode --innodb_file_per_table \
--innodb_file_format=Barracuda"

#
# Test compact  backup of a compressed tablespace with a specific page size
#
function test_compact_compressed()
{
  page_size=$1
  
  echo "************************************************************************"
  echo "Testing compact backup with compressed page size=${page_size}KB "
  echo "************************************************************************"

  run_mysqld ${mysqld_additional_args}

  load_dbase_schema sakila
  load_dbase_data sakila

  vlog "Compressing tables"

  table_list=`${MYSQL} ${MYSQL_ARGS} -Ns -e \
"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='sakila' \
AND TABLE_TYPE='BASE TABLE'"`

  for t in $table_list; do
      run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
	  "ALTER TABLE $t ENGINE=InnoDB ROW_FORMAT=compressed \
           KEY_BLOCK_SIZE=$page_size" sakila
  done
}

for page_size in 1 2 4 8 16; do
  init
  test_compact_compressed ${page_size}
  clean
done

