#
# PXB-3075 - Xtrabackup 8.0.33 cannot prepare backups from 8.0.29 and previous versions
#
. inc/common.sh
require_xtradb
#this is because backup_instant_tar.xz is taken on 8.0.29
require_server_version_higher_than 8.0.28

start_server
stop_server

rm -rf $topdir/backup && mkdir -p $topdir/backup
run_cmd tar -xf inc/backup_instant.tar.xz -C $topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

[[ -d $mysql_datadir ]] && rm -rf $mysql_datadir && mkdir -p $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup

start_server

upd_count=`$MYSQL $MYSQL_ARGS -Ns -e "select count(1) from t where c2='r1c22'" test | awk {'print $1'}`

vlog "updated row count in table t is $upd_count"

if [ "$upd_count" != 3 ]; then
 vlog "updated rows in table t is $upd_count when it should be 3"
 exit -1
fi

rm -rf $topdir/backup
