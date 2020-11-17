# PXB-787 - xtrabackup continues after finding corrupted tablespace.
. inc/common.sh
start_server
mkdir $topdir/backup


# Create table and make it corrupted
$MYSQL $MYSQL_ARGS -Ns -e "CREATE DATABASE percona"
$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE percona.t1 (ID INT PRIMARY KEY AUTO_INCREMENT)"
echo "" > $mysql_datadir/percona/t1.ibd


run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --target-dir=$topdir/backup

rm -rf $topdir/backup
$MYSQL $MYSQL_ARGS -Ns -e "DROP DATABASE percona"

