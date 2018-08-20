#
# Bug 1527081: xtrabackup 2.3 does not respect innodb_log_file_size
#              stored in backup-my.cnf
#

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-log-files-in-group=4
innodb-log-file-size=4M
"

start_server

xtrabackup --backup --target-dir=$topdir/backup

${XB_BIN} --prepare --target-dir=$topdir/backup

stop_server

rm -rf ${mysql_datadir}

xtrabackup --move-back --target-dir=$topdir/backup

start_server

if grep -q "Resizing redo log" ${MYSQLD_ERRFILE} ; then
	die "backup-my.cnf is ignored!"
fi
