#
# Bug 1513520: xtrabackup doesn't handle --slave-info option well with PXC
#

start_server

xtrabackup --slave-info --backup --target-dir=$topdir/backup2
