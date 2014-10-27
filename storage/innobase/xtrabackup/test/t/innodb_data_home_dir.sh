##############################################################################
# Test innodb_data_file_dir support
##############################################################################

. inc/common.sh

mkdir -p ${TEST_VAR_ROOT}/dir/data

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
innodb_data_home_dir=${TEST_VAR_ROOT}/dir/data
"

start_server
load_sakila

checksum1=`checksum_table sakila payment`
test -n "$checksum1" || die "Failed to checksum table sakila.payment"

innobackupex --no-timestamp $topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR/*
rm -rf ${TEST_VAR_ROOT}/dir/data/*

innobackupex --apply-log $topdir/backup
innobackupex --copy-back $topdir/backup

start_server

checksum2=`checksum_table sakila payment`
test -n "$checksum2" || die "Failed to checksum table sakila.payment"
