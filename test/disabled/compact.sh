##########################################################################
# Basic tests for --compact                                              #
##########################################################################

. inc/common.sh

init
run_mysqld --innodb_file_per_table
load_dbase_schema sakila
load_dbase_data sakila

backup_dir="$topdir/backup"

innobackupex --no-timestamp --compact $backup_dir
vlog "Backup created in directory $backup_dir"

record_db_state sakila

stop_mysqld

# Remove datadir
rm -r $mysql_datadir

# Restore sakila
vlog "Applying log"
innobackupex --apply-log $backup_dir

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex --copy-back $backup_dir

run_mysqld

verify_db_state sakila

