#
# basic rocksdb backup test with streaming
#

require_rocksdb

start_server

FULL_BACKUP_CMD="xtrabackup
    --backup
    --extra-lsndir=$topdir/backuplsn
    --parallel=10
    --stream=xbstream >$topdir/backup.xbstream"

INC_BACKUP_CMD="xtrabackup
    --backup
    --incremental-basedir=$topdir/backuplsn
    --parallel=10
    --stream=xbstream >$topdir/inc.xbstream"

FULL_PREPARE_CMD="mkdir $topdir/backup &&
  xbstream -C $topdir/backup -x --parallel=10 < $topdir/backup.xbstream &&
  xtrabackup
    --prepare
    --apply-log-only
    --target-dir=$topdir/backup"

INC_PREPARE_CMD="mkdir $topdir/inc &&
  xbstream -C $topdir/inc -x --parallel=10 < $topdir/inc.xbstream &&
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
