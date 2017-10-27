############################################################################
# Bug 1717158: Safe-slave-backups stucks with long query
############################################################################

# Not stopping slave SQL thread until long-running query is over.

master_id=1
slave_id=2

mkdir -p $topdir/backup

function long_running_task()
{
    local name=$1
    local duration=$2
    switch_server $master_id
    mysql <<EOF
CREATE TEMPORARY TABLE $name (a INT) ENGINE=InnoDB;
INSERT INTO $name VALUES(1);
UPDATE $name SET a=a+1 WHERE 0 = sleep($duration);
DROP TABLE $name;
EOF
    vlog "Long running task is DONE"
}

function run_test()
{
    local slave_mode=
    if [ $# -gt 0 -a "${1:-}" = "GTID" ]
    then
        slave_mode="$1"
        shift 1
    fi

    start_server_with_id $master_id "$@"
    start_server_with_id $slave_id "$@"

    setup_slave $slave_mode $slave_id $master_id

    # Long running query completes during wait period.
    switch_server $master_id
    long_running_task test.tmp 5&
    long_running_task_id=$!

    switch_server $slave_id
    xtrabackup --backup \
        --safe-slave-backup \
        --safe-slave-backup-timeout=15 \
        --target-dir=$topdir/backup

    stop_server_with_id $master_id
    stop_server_with_id $slave_id
}

run_test
rm -rf $topdir/backup/*

if is_server_version_higher_than 5.6.0
then
    vlog "Testing GTID mode with auto-position"
    run_test GTID \
        --gtid-mode=ON \
        --enforce-gtid-consistency=ON \
        --log-bin \
        --log-slave-updates
fi
