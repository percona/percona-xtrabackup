#
# PXB-1552: Preparing an incremental backup will crash if compressed InnoDB undo
#           tablespaces are not removed beforehand
#

require_server_version_higher_than 5.6.0
require_qpress

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
innodb_undo_tablespaces=4
"

start_server

load_sakila

xtrabackup --backup --compress --target-dir=$topdir/backup

mysql -e "DELETE FROM payment LIMIT 100" sakila

xtrabackup --backup --compress --target-dir=$topdir/inc --incremental-basedir=$topdir/backup

xtrabackup --decompress --target-dir=$topdir/backup
xtrabackup --decompress --target-dir=$topdir/inc

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup --incremental-dir=$topdir/inc
xtrabackup --prepare --target-dir=$topdir/backup
