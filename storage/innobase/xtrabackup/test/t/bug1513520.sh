#
# Bug 1513520: xtrabackup doesn't handle --slave-info option well with PXC
#

start_server

innobackupex --slave-info --no-timestamp $topdir/backup1

xtrabackup --slave-info --backup --target-dir=$topdir/backup2
