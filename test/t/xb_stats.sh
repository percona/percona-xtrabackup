. inc/common.sh

init
run_mysqld
load_dbase_schema sakila
load_dbase_data sakila

# Take backup
mkdir -p $topdir/backup
run_cmd xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup
vlog "Backup taken, trying stats"
run_cmd xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/backup

run_cmd xtrabackup --stats --datadir=$topdir/backup

vlog "stats did not fail"

stop_mysqld
clean
