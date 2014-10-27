###########################################################################
# Bug1382347: innobackupex should create server directories when copy back
#             is performed
###########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

mkdir -p ${TEST_VAR_ROOT}/dir/{undo,data,logs}

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
innodb_undo_directory=${TEST_VAR_ROOT}/dir/undo
innodb_undo_tablespaces=4
innodb_data_home_dir=${TEST_VAR_ROOT}/dir/data
innodb_log_group_home_dir=${TEST_VAR_ROOT}/dir/logs
"

start_server
load_sakila

checksum1=`checksum_table sakila payment`
test -n "$checksum1" || die "Failed to checksum table sakila.payment"

innobackupex --no-timestamp $topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR
rm -rf ${TEST_VAR_ROOT}/dir

innobackupex --apply-log $topdir/backup
innobackupex --copy-back $topdir/backup

start_server

checksum2=`checksum_table sakila payment`
test -n "$checksum2" || die "Failed to checksum table sakila.payment"
