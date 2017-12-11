start_server

vlog "Full backup"

xtrabackup --backup \
    --target-dir=$topdir/data/full

vlog "Prepare full"

xtrabackup --prepare --apply-log-only \
    --throttle=40 \
    --target-dir=$topdir/data/full

vlog "Prepare incremental"
xtrabackup --prepare --apply-log-only \
    --throttle=40 \
    --target-dir=$topdir/data/full --incremental-dir=$topdir/data/delta \
    2>&1 | tee $topdir/pxb.log

run_cmd grep 'InnoDB: xtrabackup: Last MySQL binlog file position' $topdir/pxb.log | wc -l