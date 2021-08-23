#
# basic rocksdb backup test with compression and encryption
#

require_rocksdb
require_qpress

start_server

pass=005929dbbc5602e8404fe7840d7a70c5

FULL_BACKUP_CMD="xtrabackup
    --backup
    --target-dir=$topdir/backup
    --parallel=4
    --compress
    --compress-threads=4
    --encrypt=AES256
    --encrypt-key=$pass
    --encrypt-threads=4"

INC_BACKUP_CMD="xtrabackup
    --backup
    --target-dir=$topdir/inc
    --incremental-basedir=$topdir/backup
    --parallel=4
    --compress
    --compress-threads=4
    --encrypt=AES256
    --encrypt-key=$pass
    --encrypt-threads=4"

FULL_PREPARE_CMD="xtrabackup
    --parallel=4
    --decompress
    --decrypt=AES256
    --encrypt-key=$pass
    --target-dir=$topdir/backup &&
  xtrabackup
    --prepare
    --apply-log-only
    --target-dir=$topdir/backup"

INC_PREPARE_CMD="xtrabackup
    --parallel=4
    --decompress
    --decrypt=AES256
    --encrypt-key=$pass
    --target-dir=$topdir/inc &&
  xtrabackup
    --prepare
    --target-dir=$topdir/backup
    --incremental-dir=$topdir/inc
    --parallel=4"

CLEANUP_CMD="rm -rf $mysql_datadir"

RESTORE_CMD="xtrabackup
    --move-back
    --target-dir=$topdir/backup
    --parallel=4"

. inc/rocksdb_basic.sh

if ls $topdir/backup/.rocksdb/*.sst.qp.xbcrypt 2>/dev/null ; then
    die "SST files are compressed!"
fi

if ls $mysql_datadir/.rocksdb/*.xbcrypt 2>/dev/null ; then
    die "Encrypted files are copied back!"
fi
