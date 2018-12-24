#
# Bug 1523687: xtrabackup_binlog_info is not being
#              updated correctly when applying incrementals
#

start_server

$MYSQL $MYSQL_ARGS -e "CREATE TABLE t (a INT) ENGINE=InnoDB" test
$MYSQL $MYSQL_ARGS -e "INSERT INTO t VALUES (1), (2), (3)" test

xtrabackup --backup --target-dir=$topdir/full

$MYSQL $MYSQL_ARGS -e "INSERT INTO t VALUES (4), (5), (6)" test

xtrabackup --backup --incremental-basedir=$topdir/full --target-dir=$topdir/inc

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full \
				      --incremental-dir=$topdir/inc

xtrabackup --prepare --target-dir=$topdir/full

# verify that following files overwritten in full backup directory with the
# corresponding files from incremental directory
for i in xtrabackup_binlog_info xtrabackup_galera_info \
	 xtrabackup_slave_info xtrabackup_info ;
do
	if [ -f $topdir/inc/$i ] ; then
		diff -u $topdir/full/$i $topdir/inc/$i
	fi
done

#
# PXB-1711: Incremental backups do not create/update xtrabackup_binlog_info
#

if has_backup_safe_binlog_info ; then
	diff -u $topdir/full/xtrabackup_binlog_info $topdir/full/xtrabackup_binlog_pos_innodb
fi
