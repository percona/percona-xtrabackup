########################################################################
# Test resurrect table locks
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

################################################################################
# Start an uncommitted transaction pause "indefinitely" to keep the connection
# open
################################################################################
function start_uncomitted_transaction()
{
    run_cmd $MYSQL $MYSQL_ARGS sakila <<EOF
START TRANSACTION;
DELETE FROM payment;
SELECT SLEEP(10000);
EOF
}

start_server
load_sakila

start_uncomitted_transaction &
job_master=$!

sleep 5;

innobackupex --no-timestamp --include="sakila.actor" $topdir/backup

kill -SIGKILL $job_master
stop_server

innobackupex --apply-log $topdir/backup
