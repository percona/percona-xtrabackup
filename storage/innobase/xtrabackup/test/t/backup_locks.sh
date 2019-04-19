########################################################################
# Basic backup lock tests
########################################################################

require_server_version_higher_than 5.6.0
require_xtradb

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table"

start_server

has_backup_locks || skip_test "Requires backup locks support"

has_backup_safe_binlog_info && lock_binlog_used=0 || lock_binlog_used=1

load_sakila

xtrabackup --backup --target-dir=$topdir/full_backup

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_lock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_unlock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status1

binlog_stmts=$lock_binlog_used

run_cmd diff -u $topdir/status1 - <<EOF
Com_lock_tables	0
Com_lock_tables_for_backup	1
Com_lock_binlog_for_backup	$binlog_stmts
Com_unlock_binlog	$binlog_stmts
Com_unlock_tables	1
Com_flush	1
EOF

xtrabackup --backup \
           --incremental-basedir=$topdir/full_backup \
           --target-dir=$topdir/inc_backup

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_lock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_unlock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status2

((binlog_stmts+=lock_binlog_used)) || true

run_cmd diff -u $topdir/status2 - <<EOF
Com_lock_tables	0
Com_lock_tables_for_backup	2
Com_lock_binlog_for_backup	$binlog_stmts
Com_unlock_binlog	$binlog_stmts
Com_unlock_tables	2
Com_flush	2
EOF

########################################################################
# Bug #1418820: Make backup locks usage optional
########################################################################

# Test that --no-backup-lock forces FTWRL

rm -rf $topdir/full_backup

xtrabackup --backup --no-backup-locks --target-dir=$topdir/full_backup

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_lock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_unlock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status3

run_cmd diff -u $topdir/status3 - <<EOF
Com_lock_tables	0
Com_lock_tables_for_backup	2
Com_lock_binlog_for_backup	$binlog_stmts
Com_unlock_binlog	$binlog_stmts
Com_unlock_tables	3
Com_flush	5
EOF
