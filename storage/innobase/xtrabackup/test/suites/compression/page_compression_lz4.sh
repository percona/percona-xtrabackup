. inc/page_compression_common.sh

prepare_data
vlog "Taking backup with no compression"
take_backup "--compress=lz4"
decompress "--decompress"

vlog "Testing restore with xbstream"
rm -rf $topdir/backup && mkdir $topdir/backup
xbstream -x -v -C $topdir/backup --decompress < $topdir/backup.xbs

rm -rf $topdir/backup1 && mkdir $topdir/backup1
xbstream -x -v -C $topdir/backup1 --decompress < $topdir/backup1.xbs

restore_and_verify
