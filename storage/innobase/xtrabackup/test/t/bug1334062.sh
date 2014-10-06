#############################################################################
# Bug #1334062: Xtrabackup 2.2.3 fails to perform a full backup on PS 5.5 if
#               innodb_log_file_size on the [mysqld] section of my.cnf is not
#               set
#############################################################################

start_server

sed -i -e 's/innodb_log_file_size=.*//' $MYSQLD_VARDIR/my.cnf

grep innodb_log_file_size $MYSQLD_VARDIR/my.cnf &&
  die "innodb_log_file_size is present in my.cnf"

innobackupex --no-timestamp $topdir/backup

innobackupex --apply-log $topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR/*

innobackupex --copy-back $topdir/backup

start_server
