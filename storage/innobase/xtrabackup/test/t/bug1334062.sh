#############################################################################
# Bug #1334062: Xtrabackup 2.2.3 fails to perform a full backup on PS 5.5 if
#               innodb_redo_log_capacity on the [mysqld] section of my.cnf is not
#               set
#############################################################################

start_server

sed -i -e 's/innodb_redo_log_capacity=.*//' $MYSQLD_VARDIR/my.cnf

grep innodb_redo_log_capacity $MYSQLD_VARDIR/my.cnf &&
  die "innodb_redo_log_capacity is present in my.cnf"

xtrabackup --backup --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR/*

xtrabackup --copy-back --target-dir=$topdir/backup

start_server
