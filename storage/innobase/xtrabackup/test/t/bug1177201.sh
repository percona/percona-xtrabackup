########################################################################
# Bug #1177201: Assertion "to_read % cursor->page_size == 0" failed at
#               fil_cur.cc:218
########################################################################

start_server

dd if=/dev/zero of=$topdir/zeroes bs=1 count=4096

cat $topdir/zeroes >>$MYSQLD_DATADIR/ibdata1

innobackupex --no-timestamp $topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR/*

innobackupex --apply-log $topdir/backup

innobackupex --copy-back $topdir/backup

start_server
