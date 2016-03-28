#
# Simple test case for throttle
#

start_server

xtrabackup --backup --throttle=20 --target-dir=$topdir/backup

