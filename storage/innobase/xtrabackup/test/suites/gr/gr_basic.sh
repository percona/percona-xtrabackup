. inc/common.sh

start_group_replication_cluster 3

mysql -e "SELECT * FROM performance_schema.replication_group_members;"

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup
