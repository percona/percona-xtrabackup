#
# Bug 1508448: --defaults-file requires datadir to be set.
#

start_server

# remove datadir from my.cnf
sed -ie '/datadir/d' ${MYSQLD_VARDIR}/my.cnf

# must succeed, datadir taken from server
innobackupex --no-timestamp $topdir/backup1
xtrabackup --backup --target-dir=$topdir/backup2

# must fail, datadir pints to bogus destination
run_cmd_expect_failure $IB_BIN $IB_ARGS --datadir=/wrong/datadir \
			--no-timestamp $topdir/backup3
run_cmd_expect_failure $XB_BIN $XB_ARGS --datadir=/wrong/datadir \
			--backup --target-dir=$topdir/backup4
