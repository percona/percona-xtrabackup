#
# Bug 1652044: Add -u and -p shortcuts for --user and --password
#

start_server

mysql -e "CREATE USER 'backup'@'localhost' IDENTIFIED BY 'secret'"
mysql -e "GRANT ALL PRIVILEGES ON * . * TO 'backup'@'localhost'"

run_cmd ${XB_BIN} --no-defaults --backup -ubackup -S $MYSQLD_SOCKET \
		  -P $MYSQLD_PORT -psecret --target-dir=$topdir/backup

rm -rf $topdir/backup

run_cmd ${IB_BIN} --no-defaults -ubackup -psecret -S $MYSQLD_SOCKET \
		  -P $MYSQLD_PORT $topdir/backup
