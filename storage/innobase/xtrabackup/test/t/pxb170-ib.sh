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

# incremental backup
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t values (31), (32), (33)" test
innobackupex --no-timestamp --incremental \
		--incremental-basedir=$topdir/inc2 \
		$topdir/inc3

# prepare
innobackupex --apply-log --redo-only $topdir/backup
grep log-applied $topdir/backup/xtrabackup_checkpoints

innobackupex --apply-log --redo-only \
			--incremental-dir=$topdir/inc1 $topdir/backup
grep log-applied $topdir/backup/xtrabackup_checkpoints

innobackupex --apply-log --incremental-dir=$topdir/inc2 $topdir/backup
grep full-prepared $topdir/backup/xtrabackup_checkpoints

run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --apply-log \
			--incremental-dir=$topdir/inc3 $topdir/backup
