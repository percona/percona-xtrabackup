# Test diagnostics for missing bitmap data and --incremental-force-scan option

if [ -z "$XTRADB_VERSION" ]; then
    echo "Requires XtraDB" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

# The test is disabled for Percona XtraDB Cluster until a version
# with bitmap user requests is released (containing PS 5.5.29-30.0 
# or later)
set +e
${MYSQLD} --basedir=$MYSQL_BASEDIR --user=$USER --help --verbose --wsrep-sst-method=rsync| grep -q wsrep
probe_result=$?
if [[ "$probe_result" == "0" ]]
    then
        vlog "Incompatible test" > $SKIPPED_REASON
        exit $SKIPPED_EXIT_CODE
fi
set -e

. inc/common.sh

mysqld_extra_args=--innodb-track-changed-pages=TRUE

start_server $mysqld_extra_args
${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t1 (a INT) ENGINE=INNODB" test
${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (1)" test

# Force checkpoint
shutdown_server
start_server $mysqld_extra_args

# Full backup
# mkdir $topdir/data/full
# xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/inc1 --

${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (2)" test
# Force checkpoint
shutdown_server
start_server $mysqld_extra_args
${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (3)" test
# Force checkpoint
shutdown_server
start_server $mysqld_extra_args
${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t1 VALUES (4)" test
# Force checkpoint
shutdown_server
start_server $mysqld_extra_args

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
