########################################################################
# Bug #1600656: Include xtrabackup_info to extra-lsn-dir
########################################################################

. inc/common.sh

start_server

load_dbase_schema incremental_sample
multi_row_insert incremental_sample.test \({1..100},100\)

mkdir $topdir/backup

vlog "#########################################################################"
vlog "Taking a backup and stream stuff, saving extra stuff into lsndir"

xtrabackup --backup \
    --stream=xbstream \
    --extra-lsndir=$topdir/lsndir \
    > $topdir/backup/stream.xbs

xbstream -xv -C $topdir/backup < $topdir/backup/stream.xbs

vlog "#########################################################################"
vlog "Verifying that streamed and 'extra copy' of xtrabackup_info do not differ"

diff -q $topdir/backup/xtrabackup_info $topdir/lsndir/xtrabackup_info
