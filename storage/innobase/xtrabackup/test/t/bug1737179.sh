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
    --target-dir=$topdir/data/full | tee $topdir/pxb.log

vlog "Prepare incremental"
xtrabackup --prepare --apply-log-only \
    --throttle=40 \
    --target-dir=$topdir/data/full --incremental-dir=$topdir/data/delta | tee $topdir/pxb.log

run_cmd grep 'InnoDB: xtrabackup: Last MySQL binlog file position' $topdir/pxb.log > $topdir/output