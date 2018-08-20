########################################################################
# Bug 1363234: Incremental backup fail with innodb_undo_tablespaces > 1
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
innodb_undo_tablespaces=4
"

start_server
load_sakila

# backup
xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --backup \
    --incremental-basedir=$topdir/backup --target-dir=$topdir/inc1
xtrabackup --backup \
    --incremental-basedir=$topdir/inc1 --target-dir=$topdir/inc2

# prepare (last one would fail)
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
    --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc2 \
    --target-dir=$topdir/backup
