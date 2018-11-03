########################################################################
# Bug #870119: assertion failure while trying to read InnoDB partition 
########################################################################

. inc/common.sh

# Set the minimum value for innodb_open_files to be used by xtrabackup
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
"

start_server

load_dbase_schema sakila
load_dbase_data sakila

xtrabackup --backup --target-dir=$topdir/backup --parallel=16 --innodb_open_files=10

# No need to prepare and verify the backup, as the bug was resulting in a fatal
# error during the backup
