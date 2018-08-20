########################################################################
# Bug #1177201: Assertion "to_read % cursor->page_size == 0" failed at
#               fil_cur.cc:218
########################################################################

start_server

dd if=/dev/zero of=$topdir/zeroes bs=1 count=4096

cat $topdir/zeroes >>$MYSQLD_DATADIR/ibdata1

xtrabackup --backup --target-dir=$topdir/backup

stop_server

rm -rf $MYSQLD_DATADIR/*

xtrabackup --prepare --target-dir=$topdir/backup

xtrabackup --copy-back --target-dir=$topdir/backup

start_server
