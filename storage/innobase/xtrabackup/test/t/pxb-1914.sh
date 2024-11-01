#
# PXB-1914: Add timeout options for FTWRL, LTFB, LBFB
#

start_server

has_backup_locks || skip_test "Requires backup locks support"

mysql -e "CREATE TABLE t (a INT) ENGINE=MyISAM" test

mysql -e "SET GLOBAL general_log = 'ON'"
mysql -e "SET GLOBAL log_output = 'TABLE'"

xtrabackup --backup --target-dir=$topdir/bak1

rm -rf $topdir/bak1

xtrabackup --backup --lock-ddl=OFF --backup-lock-retry-count=5 --backup-lock-timeout=3 \
	   --target-dir=$topdir/bak1

rm -rf $topdir/bak1

xtrabackup --backup --lock-ddl=OFF --backup-lock-timeout=2 --backup-lock-retry-count=4 \
	   --no-backup-locks --target-dir=$topdir/bak1

rm -rf $topdir/bak1

mysql -e "SELECT argument FROM mysql.general_log WHERE argument LIKE 'SET SESSION lock_wait_timeout%'" > $topdir/log

run_cmd diff -u $topdir/log - <<EOF
argument
SET SESSION lock_wait_timeout=31536000
SET SESSION lock_wait_timeout=3
SET SESSION lock_wait_timeout=31536000
EOF
