########################################################################
# Basic test for local compressed backups
########################################################################

. inc/common.sh

require_qpress

start_server --innodb_file_per_table
load_sakila

innobackupex --compress --no-timestamp $topdir/backup

stop_server
rm -rf ${MYSQLD_DATADIR}/*

cd $topdir/backup

innobackupex --decompress $topdir/backup

innobackupex --apply-log $topdir/backup

innobackupex --copy-back $topdir/backup

start_server

# TODO: proper validation
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
