#
# PXB-1903: PXB is stuck with redo log corruption when master key is rotated
#

require_server_version_higher_than 5.7.10

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

mysql -e "CREATE TABLE t (a INT) ENCRYPTION='y'" test

for i in {1..100} ; do
    mysql -e "INSERT INTO t VALUES ($i)" test
    sleep 0.2s
done 1>/dev/null 2>/dev/null &

for i in {1..100} ; do
    mysql -e "ALTER INSTANCE ROTATE INNODB MASTER KEY" test
    sleep 0.2s
done 1>/dev/null 2>/dev/null &

xtrabackup --backup --transition-key=123 --target-dir=$topdir/backup
xtrabackup --prepare --transition-key=123 --target-dir=$topdir/backup
