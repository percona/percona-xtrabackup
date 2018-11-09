#
# PXB-1700: PXB8 crashes with ssl-mode option
#

start_server

xtrabackup --backup --ssl-fips-mode=ON --ssl-mode=PREFERRED --target-dir=$topdir/backup

