########################################################################
# Basic dump innodb buffer test
########################################################################

. inc/common.sh

require_server_version_higher_than 5.7.02

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_buffer_pool_filename=ib_buffer_pool
"
if [[ -f  $mysql_datadir/ib_buffer_pool ]]; then
    rm -f $mysql_datadir/ib_buffer_pool
fi
start_server

load_sakila

mkdir $topdir/backup

xtrabackup --backup --dump-innodb-buffer-pool --dump-innodb-buffer-pool-pct=100 --rsync --target-dir=$topdir/backup

status=$($MYSQL $MYSQL_ARGS -N -e "SHOW STATUS LIKE 'Innodb_buffer_pool_load_status'")

echo $status

if [[ ! $status =~ "completed at" ]] ; then
    die "Not all pages were dumped"
fi

diff -u $mysql_datadir/ib_buffer_pool $topdir/backup/ib_buffer_pool

diff -q $mysql_datadir/ib_buffer_pool $topdir/backup/ib_buffer_pool || \
   die "xtrabackup copied incomplete buffer pool"
