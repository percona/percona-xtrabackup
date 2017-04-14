#
# Bug 1669592: xtrabackup --prepare --incremental-dir=... --target-dir=...
#              creates redo logs in wrong directory
#

start_server

${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t1 (id int primary key)" test
${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (1)" test

xtrabackup --backup --target-dir=$topdir/backup

${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (2)" test

xtrabackup --backup --target-dir=$topdir/backup-inc --incremental-basedir=$topdir/backup

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --incremental-dir=$topdir/backup-inc --target-dir=$topdir/backup

test -f $topdir/backup/ib_logfile0 || die "Log files are not found in full backup directory"
