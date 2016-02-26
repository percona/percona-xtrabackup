##########################################################################
# Bug #766033: xtrabackup doesn't report file name in error message      #
##########################################################################

. inc/common.sh

start_server --innodb_file_per_table
load_sakila

# make sure all pages are flushed
while true
do
	DIRTY=`mysql -Ns -e "SHOW GLOBAL STATUS LIKE 'Innodb_buffer_pool_pages_dirty'" | awk '{print $2;}'`
	if [ "$DIRTY" == "0" ] ; then
		break
	fi
done

# Full backup
vlog "Starting backup"

# corrupt database
printf '\xAA\xAA\xAA\xAA' | dd of=$mysql_datadir/sakila/rental.ibd seek=16384 count=4 bs=1 \
    conv=notrunc

# we want xtrabackup to be failed on rental.ibd
run_cmd_expect_failure $XB_BIN $XB_ARGS  --backup --datadir=$mysql_datadir \
    --target-dir=$topdir/backup 

grep -q "File ./sakila/rental.ibd seems to be corrupted" $OUTFILE
