########################################################################
# Bug #1049291: innobackupex --copy-back fails with an empty
#               innodb-data-home-dir
########################################################################

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_data_home_dir=
"
start_server

xtrabackup --backup --target-dir=$topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR/*

xtrabackup --prepare --target-dir=$topdir/backup

xtrabackup --copy-back --target-dir=$topdir/backup

test -f $MYSQLD_DATADIR/ibdata1

rm -rf $MYSQLD_DATADIR
