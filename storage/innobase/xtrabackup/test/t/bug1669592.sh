#
# Bug 1669592: xtrabackup --prepare --incremental-dir=... --target-dir=...
#              creates redo logs in wrong directory
#

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-data-file-path=ibdata1:100M;ibdata2:10M:autoextend
"

start_server

${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t1 (id int primary key)" test
${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (1)" test

xtrabackup --backup --target-dir=$topdir/backup

${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (2)" test

xtrabackup --backup --target-dir=$topdir/backup-inc --incremental-basedir=$topdir/backup

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --incremental-dir=$topdir/backup-inc --target-dir=$topdir/backup

test -f $topdir/backup/ib_logfile0 || die "Log files are not found in full backup directory"

# force MySQL to check system tablespace for consistency
rm -rf $topdir/backup/ib_logfile*

stop_server

rm -rf $mysql_datadir/*

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

${MYSQL} ${MYSQL_ARGS} -e "SELECT * FROM t1" test
