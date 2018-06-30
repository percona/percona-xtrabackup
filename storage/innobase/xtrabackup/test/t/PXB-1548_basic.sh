########################################################################
# Basic dump innodb buffer test
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

start_server

load_sakila

mkdir $topdir/backup

xtrabackup --backup --dump-innodb-buffer-pool --target-dir=$topdir/backup

$MYSQL $MYSQL_ARGS -e \
       "SHOW GLOBAL STATUS LIKE 'Innodb_buffer_pool_dump_status';" > $topdir/status1

grep -q "dump completed at" $topdir/status1 \
 || die "Could not find \"dump completed at\" message"
