start_server

vlog "Full backup"

xtrabackup --backup \
    --target-dir=$topdir/data/full

vlog "Incremental backup"

xtrabackup --backup \
    --target-dir=$topdir/data/delta \
    --incremental-basedir=$topdir/data/full

vlog "Prepare full"

xtrabackup --prepare --apply-log-only \
    --throttle=40 \
    --target-dir=$topdir/data/full > $topdir/pxb.log 2>&1

vlog "Prepare full second time"
xtrabackup --prepare  \
    --throttle=40 \
    --target-dir=$topdir/data/full >> $topdir/pxb.log 2>&1

run_cmd grep 'InnoDB: xtrabackup: Last MySQL binlog file position' $topdir/pxb.log