#
# Check that xtrabackup don't fail when pages with
# fixed purpose have wrong page type
#

################################################################################
# Start an uncommitted transaction pause "indefinitely" to keep the connection
# open
################################################################################
function start_uncomitted_transaction()
{
    run_cmd $MYSQL $MYSQL_ARGS sakila <<EOF
START TRANSACTION;
DELETE FROM payment;
SELECT SLEEP(10000);
EOF
}

start_server
load_sakila

start_uncomitted_transaction &
transaction=$!

xtrabackup --backup --target-dir=$topdir/backup

kill -SIGKILL $transaction
stop_server

# set page type (FIL_PAGE_TYPE field) of pages 6 and 7 to FIL_PAGE_TYPE_UNKNOWN
for page in 6 7 ; do
	offs=$(( 16384 * page + 24 ))
	printf '\x00\x0d' | dd of=$topdir/backup/ibdata1 bs=1 seek=$offs count=2 conv=notrunc
done

xtrabackup --prepare --target-dir=$topdir/backup --innodb-checksum-algorithm=NONE
