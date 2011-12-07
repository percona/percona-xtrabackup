. inc/common.sh

init
run_mysqld
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
# Cannot use run_cmd here
if $XB_BIN $XB_ARGS --stats --datadir=$topdir/backup
then
    die "xtrabackup --stats was expected to fail, but it did not."
fi
if ! grep "Cannot find log file ib_logfile0" $OUTFILE
then
    die "Cannot find the expected error message from xtrabackup --stats"
fi

# Now create the log files in the backup and try xtrabackup --stats again

xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/backup

xtrabackup --stats --datadir=$topdir/backup

vlog "stats did not fail"
