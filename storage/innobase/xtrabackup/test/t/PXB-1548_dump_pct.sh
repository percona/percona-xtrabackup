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

xtrabackup --backup --dump-innodb-buffer-pool --dump-innodb-buffer-pool-pct=100 --target-dir=$topdir/backup

$MYSQL $MYSQL_ARGS -e \
       "SHOW ENGINE INNODB STATUS\G" | grep "Database pages" \
       | head -1 | awk '{print $3}' > $topdir/status1
       
cat $topdir/backup/ib_buffer_pool | wc -l > $topdir/status2

diff -q $topdir/status1 $topdir/status2 \
|| die "Not all pages were dumped"
