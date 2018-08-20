#
# Bug 1512616: XtraBackup 2.3 ib_logfile --move-back problem
#

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_log_group_home_dir=${TEST_VAR_ROOT}/dir/logs
innodb_data_home_dir=${TEST_VAR_ROOT}/dir/data
"

mkdir -p ${TEST_VAR_ROOT}/dir/logs
mkdir -p ${TEST_VAR_ROOT}/dir/data

start_server

xtrabackup --backup --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

stop_server

rm -rf $mysql_datadir/*
rm -rf ${TEST_VAR_ROOT}/dir

xtrabackup --move-back --target-dir=$topdir/backup

if [ ! -f ${TEST_VAR_ROOT}/dir/logs/ib_logfile0 ] ; then
	die "Log files were not moved to correct place!"
fi

if [ ! -f ${TEST_VAR_ROOT}/dir/data/ibdata1 ] ; then
	die "Data files were not moved to correct place!"
fi

start_server
