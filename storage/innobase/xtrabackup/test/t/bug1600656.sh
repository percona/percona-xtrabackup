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

# The (uncompressed_)backup_size lines are sampled at different times
# in the two copies, so they intentionally differ. Strip them before
# diffing and verify everything else matches.
diff <(sed -E '/^(uncompressed_)?backup_size = /d' \
           $topdir/backup/xtrabackup_info) \
     <(sed -E '/^(uncompressed_)?backup_size = /d' \
           $topdir/lsndir/xtrabackup_info)
