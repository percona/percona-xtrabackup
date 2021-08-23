#
# test rocksdb backup with non-default backup directory
#

require_rocksdb

start_server
init_rocksdb
shutdown_server

rocks_datadir=${TEST_VAR_ROOT}/rocks/data

mkdir -p ${TEST_VAR_ROOT}/rocks
mv $mysql_datadir/.rocksdb $rocks_datadir

MYSQLD_EXTRA_MY_CNF_OPTS="
rocksdb_datadir=$rocks_datadir
"
start_server

mysql -e "CREATE TABLE t (a INT PRIMARY KEY) ENGINE=ROCKSDB" test
mysql -e "INSERT INTO t (a) VALUES (1), (2), (3)" test

xtrabackup --backup --target-dir=$topdir/backup

if ! [ -d $topdir/backup/.rocksdb ] ; then
    die "Rocksdb haven't been backed up"
fi

mysql -e "INSERT INTO t (a) VALUES (4), (5), (6)" test

old_checksum=$(checksum_table_columns test t a)

xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/backup

if ! [ -d $topdir/inc/.rocksdb ] ; then
    die "Rocksdb haven't been backed up"
fi

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup --incremental-dir=$topdir/inc

rm -rf $mysql_datadir $rocks_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

new_checksum=$(checksum_table_columns test t a)

vlog "Checksums:  $old_checksum $new_checksum"
if ! [ "$old_checksum" == "$new_checksum" ] ; then
    die "Checksums aren't equal"
fi
