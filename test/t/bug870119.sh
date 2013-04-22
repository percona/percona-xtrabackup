########################################################################
# Bug #870119: assertion failure while trying to read InnoDB partition 
########################################################################

. inc/common.sh

# Set the minimum value for innodb_open_files to be used by xtrabackup
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
innodb_open_files=10
"

start_server

load_dbase_schema sakila
load_dbase_data sakila

innobackupex --no-timestamp $topdir/backup --parallel=16

# No need to prepare and verify the backup, as the bug was resulting in a fatal
# error during the backup
