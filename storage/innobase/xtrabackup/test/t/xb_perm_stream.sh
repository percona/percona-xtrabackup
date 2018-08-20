# Test for fix of https://bugs.launchpad.net/percona-xtrabackup/+bug/691090
. inc/common.sh

start_server

# Take backup
chmod -R 500 $mysql_datadir
mkdir -p $topdir/backup
xtrabackup --backup --stream=xbstream --target-dir=$topdir/backup > $topdir/backup/out.xbstream
