. inc/common.sh
require_server_version_higher_than 8.0.29

start_server

load_dbase_schema sakila
load_dbase_data sakila

# Take backup
mkdir -p $topdir/backup
xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup
vlog "Backup taken, trying stats"
xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/backup

# First check that xtrabackup fails with the correct error message
# when trying to get stats before creating the log files

vlog "===> xtrabackup --stats --datadir=$topdir/backup"
run_cmd_expect_failure $XB_BIN $XB_ARGS --stats --datadir=$topdir/backup

if ! grep -q "Cannot find correct redo log files" $OUTFILE
then
    die "Cannot find the expected error message from xtrabackup --stats"
fi

vlog "Now start the server so it creates redo log files"
stop_server
rm -rf $mysql_datadir
xtrabackup --copy-back --datadir=$mysql_datadir --target-dir=$topdir/backup

start_server
mysql -e "set global innodb_fast_shutdown=0"
shutdown_server

vlog " checking Stats on data directory "

xtrabackup --stats --datadir=$mysql_datadir

vlog "stats did not fail"
