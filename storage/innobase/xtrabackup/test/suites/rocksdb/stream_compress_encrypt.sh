#
# basic rocksdb backup test with streaming, compression and encryption
#

require_rocksdb

start_server

pass=005929dbbc5602e8404fe7840d7a70c5

FULL_BACKUP_CMD="xtrabackup
    --backup
    --extra-lsndir=$topdir/backuplsn
    --compress
    --compress-threads=4
    --encrypt=AES256
    --encrypt-key=$pass
    --encrypt-threads=4
    --stream=xbstream >$topdir/backup.xbstream"

INC_BACKUP_CMD="xtrabackup
    --backup
    --incremental-basedir=$topdir/backuplsn
    --parallel=10
    --compress
    --compress-threads=10
    --stream=xbstream >$topdir/inc.xbstream"

FULL_PREPARE_CMD="mkdir $topdir/backup &&
  xbstream -C $topdir/backup -x
     --parallel=10 --decompress --decrypt=AES256 --encrypt-threads=10
     --encrypt-key=$pass < $topdir/backup.xbstream &&
  xtrabackup
    --prepare
    --apply-log-only
    --target-dir=$topdir/backup"

INC_PREPARE_CMD="mkdir $topdir/inc &&
  xbstream -C $topdir/inc -x
     --parallel=10 --decompress --decrypt=AES256 --encrypt-threads=10
     --encrypt-key=$pass < $topdir/inc.xbstream &&
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
