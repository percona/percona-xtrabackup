#
# PXB-1862: Misleading error after preparing an incremental backup for second time
#

start_server

mysql -e "CREATE TABLE t1 (a INT)" test
mysql -e "INSERT INTO t1 (a) VALUES (1), (2), (3)" test

xtrabackup --backup --target-dir=$topdir/backup

mysql -e "INSERT INTO t1 (a) VALUES (10), (20), (30)" test

xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/backup

cp -a $topdir/backup $topdir/backup1

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup1

xtrabackup --prepare --incremental-dir=$topdir/inc --target-dir=$topdir/backup

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --prepare --incremental-dir=$topdir/inc \
		       --target-dir=$topdir/backup1

grep -q "xtrabackup: error: xtrabackup_logfile was already used to '--prepare'" $OUTFILE || die "error message not found!"
