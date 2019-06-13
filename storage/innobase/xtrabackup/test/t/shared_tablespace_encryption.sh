#
# Basic test for InnoDB shared tablespace encryption (ibdata1)
#

require_xtradb
require_server_version_higher_than 8.0.13

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-sys-tablespace-encrypt
innodb-encrypt-tables=FORCE
"

. inc/keyring_file.sh

start_server

mysql -e "CREATE TABLE t (a INT PRIMARY KEY, b TEXT)" test
mysql -e "INSERT INTO t (a, b) VALUES (1, 'a')" test

# use transition-key

xtrabackup --backup --transition-key=1234 --target-dir=$topdir/backup
xtrabackup --prepare --transition-key=1234 --target-dir=$topdir/backup

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --transition-key=1234 --generate-new-master-key --target-dir=$topdir/backup

start_server

mysql -e "SELECT * FROM t" test

rm -rf $topdir/backup

# don't use transition-key

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

mysql -e "SELECT * FROM t" test
