########################################################################
# Bug #870119: assertion failure while trying to read InnoDB partition 
########################################################################

. inc/common.sh

start_server --innodb_file_per_table

# Set the minimum value for innodb_open_files to be used by xtrabackup
echo "innodb_open_files=10" >>$topdir/my.cnf

load_dbase_schema sakila
load_dbase_data sakila

innobackupex --no-timestamp $topdir/backup --parallel=16

# No need to prepare and verify the backup, as the bug was resulting in a fatal
# error during the backup
