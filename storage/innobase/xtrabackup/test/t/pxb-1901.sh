#
# PXB-1901: Compressed lz4 files are also copied in restored data dir
#

require_lz4

start_server

mysql -e "CREATE TABLE t (a INT)" test

xtrabackup --backup --compress=lz4 --compress-threads=4 \
           --target-dir=$topdir/backup

xtrabackup --decompress --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

if [ -f $mysql_datadir/test/t.ibd.lz4 ] ; then
    die 'lz4 file has been copied to the datadir'
fi

