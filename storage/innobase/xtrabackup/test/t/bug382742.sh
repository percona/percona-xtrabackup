########################################################################
# Bug #382742: Absolute paths in innodb_data_file_path are not supported
########################################################################

# MySQL 5.7 require write permissions for innodb_data_home_dir
require_server_version_lower_than 5.7.0

# Use this as an absolute path prefix in innodb_data_file_path
innodb_data_home_dir=$TEST_VAR_ROOT/innodb_data_home_dir

mkdir $innodb_data_home_dir

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=0
innodb_data_home_dir=/
innodb_data_file_path=$innodb_data_home_dir/ibdata1:3M;$innodb_data_home_dir/ibdata2:10M:autoextend
"

start_server

test -f $innodb_data_home_dir/ibdata1
test -f $innodb_data_home_dir/ibdata2

$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.t(a INT)"

record_db_state test

innobackupex --no-timestamp $topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR/*
rm -rf $innodb_data_home_dir/*

innobackupex --apply-log $topdir/backup

innobackupex --copy-back $topdir/backup

test -f $innodb_data_home_dir/ibdata1
test -f $innodb_data_home_dir/ibdata2

start_server

verify_db_state test
