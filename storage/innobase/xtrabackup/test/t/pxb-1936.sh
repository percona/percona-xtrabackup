#
# PXB-1936: --prepare cannot decrypt table but "completed ok"
#

require_server_version_higher_than 5.7.10

. inc/keyring_file.sh

start_server

mysql -e "CREATE TABLE t (a INT) ENCRYPTION='y'" test

mysql -e "INSERT INTO t VALUES (1), (2), (3), (4)" test

( while true ; do
    mysql -e "INSERT INTO t SELECT a + 10*$i FROM t ORDER BY a LIMIT 4" test
    mysql -e "ALTER INSTANCE ROTATE INNODB MASTER KEY"
    sleep 1
done ) 2>/dev/null &

xtrabackup --backup --target-dir=$topdir/backup

stop_server

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --target-dir=$topdir/backup

rm -rf $mysql_datadir
rm -rf $topdir/backup

start_server

mysql -e "CREATE TABLE t (a INT) ENCRYPTION='y'" test

mysql -e "INSERT INTO t VALUES (1), (2), (3), (4)" test

innodb_wait_for_flush_all

( while true ; do
    mysql -e "INSERT INTO t SELECT a + 10*$i FROM t ORDER BY a LIMIT 4" test
    mysql -e "ALTER INSTANCE ROTATE INNODB MASTER KEY"
    sleep 1
done ) 2>/dev/null &

xtrabackup --backup --target-dir=$topdir/backup

stop_server

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --target-dir=$topdir/backup
