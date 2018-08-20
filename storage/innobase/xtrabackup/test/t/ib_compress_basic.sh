########################################################################
# Basic test for local compressed backups
########################################################################

. inc/common.sh

require_qpress

start_server --innodb_file_per_table
load_sakila

xtrabackup --backup --compress --target-dir=$topdir/backup

stop_server
rm -rf ${MYSQLD_DATADIR}/*

cd $topdir/backup

xtrabackup --decompress --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

# TODO: proper validation
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
