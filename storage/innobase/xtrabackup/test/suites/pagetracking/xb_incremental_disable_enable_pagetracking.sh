################################################################################
# Test incremental backup with page tracking disable in between full and incremental
################################################################################

. inc/common.sh
  echo "************************************************************************"
  echo "Testing incremental backup with page tracking disabled in between"
  echo "************************************************************************"

MYSQLD_EXTRA_MY_CNF_OPT="innodb_max_dirty_pages_pct=0"
  
start_server
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "INSTALL COMPONENT \"file://component_mysqlbackup\""

load_dbase_schema incremental_sample

multi_row_insert incremental_sample.test \({1..10000},10000\)

# Full backup

# Full backup folder
rm -rf $topdir/full
mkdir -p $topdir/full
# Incremental data
rm -rf $topdir/delta
mkdir -p $topdir/delta

vlog "Starting backup"

xtrabackup --datadir=$mysql_datadir --page-tracking --backup --target-dir=$topdir/full

vlog "Full backup done"

# Changing data in sakila

vlog "Making changes to database"

${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t2 (a INT(11) DEFAULT NULL, \
number INT(11) DEFAULT NULL) ENGINE=INNODB \
ROW_FORMAT=COMPRESSED " incremental_sample

multi_row_insert incremental_sample.test \({10001..12500},12500\)
multi_row_insert incremental_sample.t2 \({10001..12500},12500\)

shutdown_server
start_server

multi_row_insert incremental_sample.test \({12501..15000},15000\)
# Disable/enable page tracking

${MYSQL} ${MYSQL_ARGS} -Ns -e "select mysqlbackup_page_track_set(0)";
${MYSQL} ${MYSQL_ARGS} -Ns -e "create table t10(i int)" test;
${MYSQL} ${MYSQL_ARGS} -Ns -e "select mysqlbackup_page_track_set(1)";

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

# Incremental backup
xtrabackup --datadir=$mysql_datadir --page-tracking --backup \
--target-dir=$topdir/delta --incremental-basedir=$topdir/full \

vlog "Incremental backup done"
vlog "Preparing backup"

# Prepare backup
xtrabackup --datadir=$mysql_datadir --prepare \
--apply-log-only --target-dir=$topdir/full
vlog "Log applied to backup"
xtrabackup --datadir=$mysql_datadir --prepare \
--apply-log-only --target-dir=$topdir/full \
--incremental-dir=$topdir/delta
vlog "Delta applied to backup"
xtrabackup --datadir=$mysql_datadir --prepare \
--target-dir=$topdir/full
vlog "Data prepared for restore"

# removing rows
${MYSQL} ${MYSQL_ARGS} -e "delete from test;" incremental_sample
${MYSQL} ${MYSQL_ARGS} -e "delete from t2;" incremental_sample
vlog "Tables cleared"

# Restore backup

shutdown_server
rm -f $mysql_datadir/ib_modified_log*

vlog "Copying files"

cd $topdir/full/
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

check_full_scan_inc_backup
