#
# test rocksdb backup with non-default backup directory
#

require_rocksdb

start_server
init_rocksdb
shutdown_server

rocks_wal_dir=${TEST_VAR_ROOT}/rocks/wal

mkdir -p ${TEST_VAR_ROOT}/rocks

MYSQLD_EXTRA_MY_CNF_OPTS="
rocksdb_wal_dir=$rocks_wal_dir
rocksdb_wal_ttl_seconds=1800
"

FULL_BACKUP_CMD="xtrabackup
    --backup
    --target-dir=$topdir/backup
    --parallel=10"

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

CLEANUP_CMD="rm -rf $mysql_datadir ${TEST_VAR_ROOT}/rocks"

RESTORE_CMD="xtrabackup
    --copy-back
    --target-dir=$topdir/backup
    --parallel=10"

start_server

# create some more tables
mysql -e "CREATE TABLE t (a INT PRIMARY KEY AUTO_INCREMENT, b INT, KEY(b), c VARCHAR(200)) ENGINE=ROCKSDB;" test

# insert some data
for i in {1..1000} ; do
  echo "INSERT INTO t (b, c) VALUES (FLOOR(RAND() * 1000000), UUID());"
done | mysql test


while true ; do
    echo "INSERT INTO t (b, c) VALUES (FLOOR(RAND() * 1000000), UUID());"
done | mysql test &

# force archival of wal logs
mysql -e "SET GLOBAL rocksdb_force_flush_memtable_now=1"

eval ${FULL_BACKUP_CMD}

if ! [ -d $topdir/backup/.rocksdb ] ; then
    die "Rocksdb haven't been backed up"
fi

mysql -e "SET GLOBAL rocksdb_force_flush_memtable_now=1"
# insert some data
for i in {1..1000} ; do
  echo "INSERT INTO t (b, c) VALUES (FLOOR(RAND() * 1000000), UUID());"
done | mysql test

eval ${INC_BACKUP_CMD}

eval ${FULL_PREPARE_CMD}
eval ${INC_PREPARE_CMD}

stop_server

eval ${CLEANUP_CMD}

eval ${RESTORE_CMD}

start_server

mysql -e "SELECT COUNT(*) FROM t" test


