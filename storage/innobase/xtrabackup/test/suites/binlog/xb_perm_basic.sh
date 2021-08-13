# Test for fix of https://bugs.launchpad.net/percona-xtrabackup/+bug/691090
. inc/common.sh

binlog=$topdir/xb-perm-basic-log-bin

start_server --log-bin=$binlog
innodb_wait_for_flush_all

chmod -R 500 $mysql_datadir
xtrabackup --backup --target-dir=$topdir/backup

stop_server

rm $binlog*
