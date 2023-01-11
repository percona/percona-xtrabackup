. inc/page_compression_common.sh

vlog "Taking backup with ZSTD compression"
take_backup "--compress=zstd --read-buffer-size=1M"
decompress "--decompress --read-buffer-size=1M"
restore_and_verify

vlog "Testing restore with xbstream"
rm -rf $topdir/backup && mkdir $topdir/backup
xbstream -x -v -C $topdir/backup --decompress < $topdir/backup.xbs

rm -rf $topdir/backup1 && mkdir $topdir/backup1
xbstream -x -v -C $topdir/backup1 --decompress < $topdir/backup1.xbs

restore_and_verify
