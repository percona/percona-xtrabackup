########################################################################
# Failing assertion: UT_LIST_GET_LEN(trx->lock.trx_locks) == 0 while
# preparing full backup
########################################################################

. inc/common.sh

if [ ${ASAN_OPTIONS:-undefined} != "undefined" ]
then
    skip_test "Incompatible with AddressSanitizer"
fi

require_server_version_higher_than 5.6.0

################################################################################
# Start an uncommitted transaction pause "indefinitely" to keep the connection
# open
################################################################################
function start_uncomitted_transaction()
{
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (a INT);
START TRANSACTION;
INSERT INTO t1 VALUES (1), (2), (3);
SELECT SLEEP(5000);
EOF
}

start_server

start_uncomitted_transaction &
job_master=$!

sleep 5

innobackupex --no-timestamp $topdir/backup

kill -SIGKILL $job_master
stop_server

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
