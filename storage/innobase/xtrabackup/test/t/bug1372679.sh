########################################################################
# Bug #1372679: innobackupex --slave-info doesn't handle
#               slave_parallel_workers>0
########################################################################

require_server_version_higher_than 5.6.0

# Test that --slave-info with MTS enabled + GTID disabled fails

MYSQLD_EXTRA_MY_CNF_OPTS="
slave_parallel_workers=2
"
master_id=1
slave_id=2

start_server_with_id $master_id

start_server_with_id $slave_id

setup_slave $slave_id $master_id

switch_server $slave_id

innobackupex --no-timestamp --slave-info $topdir/backup 2>&1 |
    grep 'The --slave-info option requires GTID enabled for a multi-threaded slave' ||
    die "could not find the error message"

if [[ ${PIPESTATUS[0]} == 0 ]]
then
    die "innobackupex did not fail as expected"
fi

stop_server_with_id $master_id
stop_server_with_id $slave_id

remove_var_dirs

# Test that --slave-info with MTS enabled + GTID enabled works

MYSQLD_EXTRA_MY_CNF_OPTS="
gtid_mode=on
log_slave_updates=on
enforce_gtid_consistency=on
slave_parallel_workers=2
"

start_server_with_id $master_id
start_server_with_id $slave_id

setup_slave $slave_id $master_id

switch_server $slave_id

innobackupex --no-timestamp --slave-info $topdir/backup
