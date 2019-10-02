#
# basic rocksdb backup test
#

require_rocksdb

start_server

FULL_BACKUP_CMD="xtrabackup
    --backup
    --target-dir=$topdir/backup
    --parallel=10
    --rocksdb-checkpoint-max-age=1
    --rocksdb-checkpoint-max-count=10"

INC_BACKUP_CMD="xtrabackup
    --backup
    --target-dir=$topdir/inc
    --incremental-basedir=$topdir/backup
    --parallel=10"

FULL_PREPARE_CMD="xtrabackup
    --prepare
    --apply-log-only
    --target-dir=$topdir/backup"

INC_PREPARE_CMD="xtrabackup
    --prepare
    --target-dir=$topdir/backup
    --incremental-dir=$topdir/inc
    --parallel=10"

CLEANUP_CMD="rm -rf $mysql_datadir"

RESTORE_CMD="xtrabackup
    --copy-back
    --target-dir=$topdir/backup
    --parallel=10"

. inc/rocksdb_basic.sh
