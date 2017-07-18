##########################################################################
# Bug 1192834: Crash during apply with index compaction enabled          #
##########################################################################

. inc/common.sh

skip_test "Enable when bug #1192834 is fixed in 2.4"

start_server --innodb_file_per_table
load_dbase_schema sakila
load_dbase_data sakila

mysql -e "DELETE FROM payment LIMIT 100" sakila

backup_dir="$topdir/backup"

xtrabackup --backup --compact --target-dir=$backup_dir
vlog "Backup created in directory $backup_dir"

stop_server

# Remove datadir
rm -r $mysql_datadir

# Restore sakila

xtrabackup --prepare --rebuild-indexes --target-dir=$backup_dir

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
xtrabackup --copy-back --target-dir=$backup_dir

start_server
