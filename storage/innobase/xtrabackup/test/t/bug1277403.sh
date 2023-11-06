################################################################################
# Bug 1277403: Use FLUSH TABLES before FTWRL                                   #
################################################################################

# we don't want to run this test on InnoDB 5.1 since it doesn't support
# "FLUSH ENGINE LOGS" and Com_flush will show 2 for it
require_server_version_higher_than 5.5.0

start_server --general-log=1 --log-output=TABLE

has_backup_locks && skip_test "Requires server without backup locks support"

mysql -e 'CREATE TABLE t1 (a INT) engine=MyISAM' test

xtrabackup --backup --lock-ddl=OFF --target-dir=$topdir/full_backup

grep_general_log > $topdir/log1

diff -u $topdir/log1 - <<EOF
FLUSH NO_WRITE_TO_BINLOG TABLES
FLUSH TABLES WITH READ LOCK
FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS
UNLOCK TABLES
EOF
