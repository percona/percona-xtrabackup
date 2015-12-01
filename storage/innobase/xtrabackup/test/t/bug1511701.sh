###############################################################################
# Bug 1511701: rsync parameter return error with 2.3.2 build
###############################################################################

start_server

# remove tmpdir from my.cnf
cp ${MYSQLD_VARDIR}/my.cnf ${MYSQLD_VARDIR}/my.cnf.orig
sed -ie '/tmpdir/d' ${MYSQLD_VARDIR}/my.cnf

innobackupex --rsync --datadir=$mysql_datadir --no-timestamp $topdir/backup1

xtrabackup --rsync --datadir=$mysql_datadir --backup --target-dir=$topdir/backup2
