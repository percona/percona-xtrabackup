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
innobackupex --no-timestamp $topdir/backup
innobackupex --incremental --no-timestamp \
    --incremental-basedir=$topdir/backup $topdir/inc1
innobackupex --incremental --no-timestamp \
    --incremental-basedir=$topdir/inc1 $topdir/inc2

# prepare (last one would fail)
innobackupex --apply-log --redo-only $topdir/backup
innobackupex --apply-log --redo-only --incremental-dir=$topdir/inc1 \
    $topdir/backup
innobackupex --apply-log --redo-only --incremental-dir=$topdir/inc2 \
    $topdir/backup
