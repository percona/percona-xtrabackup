# Test for fix of https://bugs.launchpad.net/percona-xtrabackup/+bug/691090
. inc/common.sh

init
run_mysqld

# Take backup
chmod -R 500 $mysql_datadir
trap "vlog restoring permissions on datadir $mysql_datadir ; \
chmod -R 700 $mysql_datadir" INT TERM EXIT
mkdir -p $topdir/backup
innobackupex --stream=tar $topdir/backup > $topdir/backup/out.tar
