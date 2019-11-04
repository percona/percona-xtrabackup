#
# PXB-1951: PXB crashes when it cannot connect to server
#

start_server --max-connections=2

mysql -e "select sleep(1000);" &

sleep 2

xtrabackup --backup --target-dir=$topdir/backup
