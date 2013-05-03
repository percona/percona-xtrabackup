##########################################################################
# Basic tests for --compact                                              #
##########################################################################

. inc/common.sh

start_server --innodb_file_per_table
load_dbase_schema sakila
load_dbase_data sakila

backup_dir="$topdir/backup"

innobackupex --no-timestamp --compact $backup_dir
vlog "Backup created in directory $backup_dir"

record_db_state sakila

stop_server

# Remove datadir
rm -r $mysql_datadir

# Restore sakila

innobackupex --apply-log --rebuild-indexes $backup_dir

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex --copy-back $backup_dir

start_server

verify_db_state sakila

##########################################################################
# Test --rebuild-threads
##########################################################################

rm -rf $backup_dir

innobackupex --no-timestamp --compact $backup_dir
vlog "Backup created in directory $backup_dir"

record_db_state sakila

stop_server

# Remove datadir
rm -r $mysql_datadir

# Restore sakila

innobackupex --apply-log --rebuild-indexes --rebuild-threads=16 $backup_dir

grep -q "Starting 16 threads to rebuild indexes" $OUTFILE

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex --copy-back $backup_dir

start_server

verify_db_state sakila
