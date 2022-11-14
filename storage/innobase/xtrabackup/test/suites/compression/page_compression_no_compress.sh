. inc/page_compression_common.sh

prepare_data
vlog "Taking backup with no compression"
take_backup ""
restore_and_verify

vlog "Testing restore with xbstream"
rm -rf $topdir/backup && mkdir $topdir/backup
xbstream -x -v -C $topdir/backup < $topdir/backup.xbs

rm -rf $topdir/backup1 && mkdir $topdir/backup1
xbstream -x -v -C $topdir/backup1 < $topdir/backup1.xbs

restore_and_verify
