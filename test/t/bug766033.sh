##########################################################################
# Bug #766033: xtrabackup doesn't report file name in error message      #
##########################################################################

. inc/common.sh

init
run_mysqld --innodb_file_per_table
load_sakila

stop_mysqld

# Full backup
vlog "Starting backup"

# corrupt database
dd if=/dev/zero of=$mysql_datadir/sakila/rental.ibd seek=1000 count=16384

# we want xtrabackup to be failed on rental.ibd
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --datadir=$mysql_datadir \
    --target-dir=$topdir/backup
grep -q "File ./sakila/rental.ibd seems to be corrupted" $OUTFILE
