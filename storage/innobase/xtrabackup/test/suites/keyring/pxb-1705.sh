#
# PXB-1705: PXB crashes in prepare of backup for encrypted tablespace and tables
#

. inc/keyring_file.sh

start_server

( while true ; do
  ${MYSQL_BIN} ${MYSQL_ARGS} test -e "CREATE TABLE t1 (id INT AUTO_INCREMENT PRIMARY KEY, i INT, name VARCHAR(255)) ENGINE=InnoDB ENCRYPTION='Y'"
  ${MYSQL_BIN} ${MYSQL_ARGS} test -e "INSERT INTO t1 set i=1, name=NOW()"
  ${MYSQL_BIN} ${MYSQL_ARGS} test -e "DROP TABLE t1"
done ) &

for i in {1..3} ; do
  rm -rf $topdir/backup
  xtrabackup --backup --target-dir=$topdir/backup --transition-key=123
  xtrabackup --prepare --target-dir=$topdir/backup --transition-key=123
done

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup
