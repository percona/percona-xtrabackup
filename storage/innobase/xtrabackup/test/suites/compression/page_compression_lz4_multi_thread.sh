. inc/page_compression_common.sh

prepare_data
vlog "Taking backup with LZ4 compression and multi thread"
take_backup "--parallel=2  --compress=lz4 --compress-threads=2 --read-buffer-size=1M"
decompress "--decompress --parallel=2 --read-buffer-size=1M"

vlog "Testing restore with xbstream"
rm -rf $topdir/backup && mkdir $topdir/backup
xbstream -x -v -C $topdir/backup --decompress --decompress-threads=2 --parallel=2 < $topdir/backup.xbs

rm -rf $topdir/backup1 && mkdir $topdir/backup1
xbstream -x -v -C $topdir/backup1 --decompress --decompress-threads=2 --parallel=2 < $topdir/backup1.xbs

restore_and_verify
