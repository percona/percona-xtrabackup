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
innodb_data_home_dir=${TEST_VAR_ROOT}/dir/data
innodb_log_group_home_dir=${TEST_VAR_ROOT}/dir/logs
"

start_server
mysql -e "CREATE UNDO TABLESPACE undo_001 ADD DATAFILE 'undo_001.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo_002 ADD DATAFILE 'undo_002.ibu'"

load_sakila

checksum1=`checksum_table sakila payment`
test -n "$checksum1" || die "Failed to checksum table sakila.payment"

xtrabackup --backup --target-dir=$topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR
rm -rf ${TEST_VAR_ROOT}/dir

xtrabackup --prepare --target-dir=$topdir/backup
xtrabackup --copy-back --target-dir=$topdir/backup

start_server

checksum2=`checksum_table sakila payment`
test -n "$checksum2" || die "Failed to checksum table sakila.payment"
