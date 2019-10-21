########################################################################
# Basic dump innodb buffer test
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_buffer_pool_filename=ib_buffer_pool
"
if [[ -f  $mysql_datadir/ib_buffer_pool ]]; then
    rm -f $mysql_datadir/ib_buffer_pool
fi
start_server

load_sakila

mkdir $topdir/backup

xtrabackup --backup --dump-innodb-buffer-pool --target-dir=$topdir/backup

if [[ ! -f  $mysql_datadir/ib_buffer_pool ]]; then
    die "dump file doesnt exist"
fi

$MYSQL $MYSQL_ARGS -e \
       "SHOW GLOBAL STATUS LIKE 'Innodb_buffer_pool_dump_status';" > $topdir/message

grep -q "dump completed at" $topdir/message \
|| die "Could not find \"dump completed at\" message"

cat $mysql_datadir/ib_buffer_pool | wc -l > $topdir/status1
cat $topdir/backup/ib_buffer_pool | wc -l > $topdir/status2

diff -q $topdir/status1 $topdir/status2 \
|| die "Not all pages were dumped"
