. inc/common.sh

if ! which rsync > /dev/null 2>&1
then
    skip_test "Requires rsync to be installed"
fi

start_server --innodb_file_per_table
load_sakila

xtrabackup --backup --rsync --no-lock --target-dir=$topdir/backup

stop_server

run_cmd rm -r $mysql_datadir

xtrabackup --prepare --target-dir=$topdir/backup

run_cmd mkdir -p $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT COUNT(*) FROM actor" sakila
