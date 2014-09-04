########################################################################
# Basic backup lock tests
########################################################################

require_server_version_higher_than 5.6.0
require_xtradb

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table"

start_server

has_backup_locks || skip_test "Requires backup locks support"

load_sakila

innobackupex --no-timestamp $topdir/full_backup

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_%lock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status1

diff $topdir/status1 - <<EOF
Com_lock_tables	0
Com_lock_tables_for_backup	1
Com_lock_binlog_for_backup	1
Com_show_slave_status_nolock	0
Com_unlock_binlog	1
Com_unlock_tables	1
Com_flush	1
EOF

innobackupex --no-timestamp --incremental \
             --incremental-basedir=$topdir/full_backup \
             $topdir/inc_backup

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_%lock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status2

diff $topdir/status2 - <<EOF
Com_lock_tables	0
Com_lock_tables_for_backup	2
Com_lock_binlog_for_backup	2
Com_show_slave_status_nolock	0
Com_unlock_binlog	2
Com_unlock_tables	2
Com_flush	2
EOF
