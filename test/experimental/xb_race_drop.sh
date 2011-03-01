. inc/common.sh

init
run_mysqld --innodb_file_per_table

run_cmd race_create_drop &

sleep 3
# Full backup
mkdir -p $topdir/data/full
vlog "Starting backup"
xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/full
vlog "Full backup done"
stop_mysqld
clean 
