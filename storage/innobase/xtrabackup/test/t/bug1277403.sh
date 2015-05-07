################################################################################
# Bug 1277403: Use FLUSH TABLES before FTWRL                                   #
################################################################################

# we don't want to run this test on InnoDB 5.1 since it doesn't support
# "FLUSH ENGINE LOGS" and Com_flush will show 2 for it
require_server_version_higher_than 5.5.0

start_server

has_backup_locks && skip_test "Requires server without backup locks support"

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_unlock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status1

diff -u $topdir/status1 - <<EOF
Com_unlock_tables	0
Com_flush	0
EOF

innobackupex --no-timestamp $topdir/full_backup

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_unlock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status2

diff -u $topdir/status2 - <<EOF
Com_unlock_tables	1
Com_flush	3
EOF
