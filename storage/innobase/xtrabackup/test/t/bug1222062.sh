########################################################################
# Bug #1222062: add option --close-files
########################################################################

start_server

mkdir $topdir/backup

xtrabackup --backup --close-files --target-dir=$topdir/backup

grep "xtrabackup: warning: close-files specified" $OUTFILE
