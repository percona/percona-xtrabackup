start_server

vlog "Full backup"

mkdir $topdir/backup

xtrabackup --backup \
    --target-dir=$topdir/backup/full

vlog "Incremental backup"

xtrabackup --backup \
    --target-dir=$topdir/backup/delta \
    --incremental-basedir=$topdir/backup/full

vlog "Prepare full"

xtrabackup --prepare --apply-log-only \
    --throttle=40 \
    --target-dir=$topdir/backup/full

vlog "Prepare incremental"
xtrabackup --prepare --apply-log-only \
    --throttle=40 \
    --target-dir=$topdir/backup/full --incremental-dir=$topdir/backup/delta \
    2>&1 | tee $topdir/pxb.log

run_cmd grep 'xtrabackup: warning.*--throttle' $topdir/pxb.log
