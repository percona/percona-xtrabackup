###############################################################################
# Bug #1042887: innobackupex should understand --datadir option
###############################################################################

start_server

# remove datadir from my.cnf
cp ${MYSQLD_VARDIR}/my.cnf ${MYSQLD_VARDIR}/my.cnf.orig
sed -ie '/datadir/d' ${MYSQLD_VARDIR}/my.cnf

xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

stop_server

rm -rf $mysql_datadir

xtrabackup --datadir=$mysql_datadir --copy-back --target-dir=$topdir/backup

# restore original my.cnf to allow mysqld to start
mv ${MYSQLD_VARDIR}/my.cnf.orig ${MYSQLD_VARDIR}/my.cnf

start_server
