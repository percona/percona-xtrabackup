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
"
start_server

mysql -e "CREATE TABLE t (a INT AUTO_INCREMENT PRIMARY KEY) ENGINE=ROCKSDB" test
mysql -e "INSERT INTO t (a) VALUES (1), (2), (3)" test

while true ; do
    echo "INSERT INTO t () VALUES ();"
done | mysql test &

xtrabackup --backup --target-dir=$topdir/backup

if ! [ -d $topdir/backup/.rocksdb ] ; then
    die "Rocksdb haven't been backed up"
fi

xtrabackup --prepare --target-dir=$topdir/backup

stop_server

rm -rf $mysql_datadir ${TEST_VAR_ROOT}/rocks

xtrabackup --copy-back --target-dir=$topdir/backup

if ! ls $rocks_wal_dir/*.log 1> /dev/null 2>&1; then
    die "Logs haven't been copied to wal dir"
fi

start_server

mysql -e "SELECT COUNT(*) FROM t" test
