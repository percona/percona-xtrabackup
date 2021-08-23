#
# basic rocksdb backup test with compression
#

require_rocksdb
require_qpress

start_server

FULL_BACKUP_CMD="xtrabackup
    --backup
    --target-dir=$topdir/backup
    --parallel=10
    --compress
    --compress-threads=10"

INC_BACKUP_CMD="xtrabackup
    --backup
    --target-dir=$topdir/inc
    --incremental-basedir=$topdir/backup
    --parallel=10
    --compress
    --compress-threads=10"

FULL_PREPARE_CMD="xtrabackup
    --parallel=10
    --decompress
    --target-dir=$topdir/backup &&
  xtrabackup
    --prepare
    --apply-log-only
    --target-dir=$topdir/backup"

INC_PREPARE_CMD="xtrabackup
    --parallel=10
    --decompress
    --target-dir=$topdir/inc &&
   xtrabackup
    --prepare
    --target-dir=$topdir/backup
    --incremental-dir=$topdir/inc
    --parallel=10"

CLEANUP_CMD="rm -rf $mysql_datadir"

RESTORE_CMD="xtrabackup
    --move-back
    --target-dir=$topdir/backup
    --parallel=10"

. inc/rocksdb_basic.sh

if ls $topdir/backup/.rocksdb/*.sst.qp 2>/dev/null ; then
    die "SST files are compressed!"
fi

if ls $mysql_datadir/.rocksdb/*.qp 2>/dev/null ; then
    die "Compressed files are copied back!"
fi
