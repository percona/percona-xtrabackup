########################################################################
# Bug #803636: "moves files" option needed with --copy-back
########################################################################

. inc/common.sh

start_server --innodb_file_per_table

load_sakila

# Backup
xtrabackup --backup --target-dir=$topdir/backup

stop_server

rm -r $mysql_datadir

# Prepare
xtrabackup --prepare --target-dir=$topdir/backup

# Restore
mkdir -p $mysql_datadir
xtrabackup --move-back --target-dir=$topdir/backup

# Check that there are no data files in the backup directory
run_cmd_expect_failure ls -R $topdir/backup/*/*.{ibd,MYD,MYI,frm}

start_server

# Verify data
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
