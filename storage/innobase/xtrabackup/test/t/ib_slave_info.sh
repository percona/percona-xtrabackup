. inc/common.sh

master_id=1
slave_id=2

start_server_with_id $master_id
start_server_with_id $slave_id

setup_slave $slave_id $master_id

switch_server $master_id
load_dbase_schema incremental_sample

# Adding initial rows
multi_row_insert incremental_sample.test \({1..100},100\)

# Full backup of the slave server
switch_server $slave_id

vlog "Check that --slave-info with --no-lock and no --safe-slave-backup fails"
run_cmd_expect_failure $IB_BIN $IB_ARGS --no-timestamp --slave-info --no-lock \
  $topdir/backup

innobackupex --no-timestamp --slave-info $topdir/backup
egrep -q '^mysql-bin.[0-9]+[[:space:]]+[0-9]+$' \
    $topdir/backup/xtrabackup_binlog_info
egrep -q '^CHANGE MASTER TO MASTER_LOG_FILE='\''mysql-bin.[0-9]+'\'', MASTER_LOG_POS=[0-9]+$' \
    $topdir/backup/xtrabackup_slave_info
