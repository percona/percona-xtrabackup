############################################################################
# PXB-170: xtrabackup should not allow to apply incremental backup on
#          top of the backup prepared without --apply-log-only
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

# full backup
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t (a INT) ENGINE=InnoDB" test
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t values (1), (2), (3)" test
innobackupex --no-timestamp $topdir/backup

# incremental backup
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t values (11), (12), (13)" test
innobackupex --no-timestamp --incremental \
		--incremental-basedir=$topdir/backup \
		$topdir/inc1

# incremental backup
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t values (21), (22), (23)" test
innobackupex --no-timestamp --incremental \
		--incremental-basedir=$topdir/inc1 \
		$topdir/inc2

# prepare
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
grep log-applied $topdir/backup/xtrabackup_checkpoints

xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
		--target-dir=$topdir/backup
grep log-applied $topdir/backup/xtrabackup_checkpoints

xtrabackup --prepare --incremental-dir=$topdir/inc2 --target-dir=$topdir/backup
grep full-prepared $topdir/backup/xtrabackup_checkpoints

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --prepare \
		--incremental-dir=$topdir/inc2 --target-dir=$topdir/backup
