# Test for fix of https://bugs.launchpad.net/percona-xtrabackup/+bug/691090
. inc/common.sh

start_server

chmod -R 500 $mysql_datadir
innobackupex  --no-timestamp $topdir/backup
