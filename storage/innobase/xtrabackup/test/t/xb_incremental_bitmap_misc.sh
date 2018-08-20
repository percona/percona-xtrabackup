# Test diagnostics for missing bitmap data and --incremental-force-scan option

require_xtradb
is_64bit || skip_test "Disabled on 32-bit hosts due to LP bug #1359182"

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-track-changed-pages=TRUE
"

start_server
${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t1 (a INT) ENGINE=INNODB" test
${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (1)" test

# Force checkpoint
shutdown_server
start_server

# Full backup
# mkdir $topdir/data/full
# xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/inc1 --

${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (2)" test
# Force checkpoint
shutdown_server
start_server
${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (3)" test
# Force checkpoint
shutdown_server
start_server
${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (4)" test
# Force checkpoint
shutdown_server
start_server

ls -l $mysql_datadir/*.xdb

vlog "Test --incremental-force-scan option"
xtrabackup --datadir=$mysql_datadir --backup \
   --target-dir=$topdir/data/inc0 --incremental_lsn=9000 \
   --incremental-force-scan

vlog "Test nonsensical LSN range (start LSN > end LSN)"
xtrabackup --datadir=$mysql_datadir --backup \
   --target-dir=$topdir/data/inc1 --incremental-lsn=500000000000

vlog "Test missing bitmap data diagnostics: middle of range"
run_cmd rm $mysql_datadir/ib_modified_log_2*.xdb
xtrabackup --datadir=$mysql_datadir --backup \
    --target-dir=$topdir/data/inc2 --incremental-lsn=9000

vlog "Test missing bitmap data diagnostics: end of range too"
run_cmd rm $mysql_datadir/ib_modified_log_3*.xdb
xtrabackup --datadir=$mysql_datadir --backup \
    --target-dir=$topdir/data/inc3 --incremental-lsn=9000

vlog "Test missing bitmap data diagnostics: start of range too"
run_cmd rm $mysql_datadir/ib_modified_log_1*.xdb
xtrabackup --datadir=$mysql_datadir --backup \
    --target-dir=$topdir/data/inc4 --incremental-lsn=9000

check_full_scan_inc_backup
