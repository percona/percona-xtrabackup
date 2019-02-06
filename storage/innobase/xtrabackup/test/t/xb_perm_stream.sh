# Test for fix of https://bugs.launchpad.net/percona-xtrabackup/+bug/691090
. inc/common.sh

binlog=$topdir/xb-perm-stream-log-bin

start_server --log-bin=$binlog

# Take backup
chmod -R 500 $mysql_datadir
mkdir -p $topdir/backup
xtrabackup --backup --stream=xbstream --target-dir=$topdir/backup > $topdir/backup/out.xbstream

stop_server

rm $binlog*
