#
# test cross engine transactions
#

require_rocksdb

start_server

init_rocksdb

mysql -e "CREATE TABLE inno_t (a INT PRIMARY KEY AUTO_INCREMENT, b INT, KEY(b), c VARCHAR(200)) ENGINE=INNODB" test
mysql -e "CREATE TABLE rocks_t (a INT PRIMARY KEY AUTO_INCREMENT, b INT, KEY(b), c VARCHAR(200)) ENGINE=ROCKSDB" test

# start filling the tables with some data in multiple threads
for i in {1..4} ; do
    while true ; do
        echo "START TRANSACTION;"
        echo "SELECT * FROM inno_t LIMIT 1;"
        echo "SELECT * FROM rocks_t LIMIT 1;"
        echo "SET @a = FLOOR(RAND() * 1000000), @b = FLOOR(RAND() * 1000000), @c = UUID();"
        echo "REPLACE INTO inno_t (a, b, c) VALUES (@a, @b, @c);"
        echo "REPLACE INTO rocks_t (a, b, c) VALUES (@a, @b, @c);"
        echo "COMMIT;"
        echo "START TRANSACTION;"
        echo "SELECT * FROM inno_t LIMIT 1;"
        echo "SELECT * FROM rocks_t LIMIT 1;"
        echo "SET @a = FLOOR(RAND() * 1000000), @b = FLOOR(RAND() * 1000000), @c = UUID();"
        echo "REPLACE INTO rocks_t (a, b, c) VALUES (@a, @b, @c);"
        echo "REPLACE INTO inno_t (a, b, c) VALUES (@a, @b, @c);"
        echo "COMMIT;"
    done | mysql test &
done

while true ; do
    echo "START TRANSACTION;"
    echo "SELECT * FROM inno_t LIMIT 1;"
    echo "SELECT * FROM rocks_t LIMIT 1;"
    echo "SELECT a INTO @a FROM inno_t LIMIT 100,1;"
    echo "DELETE FROM inno_t WHERE a = @a;"
    echo "DELETE FROM rocks_t WHERE a = @a;"
    echo "COMMIT;"
    echo "START TRANSACTION;"
    echo "SELECT * FROM inno_t LIMIT 1;"
    echo "SELECT * FROM rocks_t LIMIT 1;"
    echo "SELECT a INTO @a FROM rocks_t LIMIT 100,1;"
    echo "DELETE FROM rocks_t WHERE a = @a;"
    echo "DELETE FROM inno_t WHERE a = @a;"
    echo "COMMIT;"
done | mysql test &

# backup
xtrabackup --parallel=4 --backup --target-dir=$topdir/backup
xtrabackup --parallel=4 --backup --target-dir=$topdir/inc1 \
           --incremental-basedir=$topdir/backup
xtrabackup --parallel=4 --backup --target-dir=$topdir/inc2 \
           --incremental-basedir=$topdir/inc1
xtrabackup --parallel=4 --backup --target-dir=$topdir/inc3 \
           --incremental-basedir=$topdir/inc2

xtrabackup --parallel=4 --backup --target-dir=$topdir/backup22

stop_server

# prepare
xtrabackup --parallel=4 --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --parallel=4 --prepare --apply-log-only --target-dir=$topdir/backup \
           --incremental-dir=$topdir/inc1
xtrabackup --parallel=4 --prepare --apply-log-only --target-dir=$topdir/backup \
           --incremental-dir=$topdir/inc2
xtrabackup --parallel=4 --prepare --apply-log-only --target-dir=$topdir/backup \
           --incremental-dir=$topdir/inc3
xtrabackup --prepare --target-dir=$topdir/backup

# # clenup and restore

rm -rf $mysql_datadir

xtrabackup --copy-back --parallel=4 --target-dir=$topdir/backup

start_server

# do integrity checks

mysql -e 'SELECT * FROM inno_t WHERE a NOT IN (SELECT a FROM rocks_t)' test
mysql -e 'SELECT * FROM rocks_t WHERE a NOT IN (SELECT a FROM inno_t)' test

checksum_inno_t=$(checksum_table_columns test inno_t a b)
vlog "inno_t checksum: $checksum_inno_t"

checksum_rocks_t=$(checksum_table_columns test rocks_t a b)
vlog "rocks_t checksum: $checksum_rocks_t"

# at the end we should have identical tables
if ! [ "$checksum_inno_t" = "$checksum_rocks_t" ] ; then
    die "checksums are not equal"
fi
