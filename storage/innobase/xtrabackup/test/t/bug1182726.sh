. inc/common.sh
########################################################################
# Bug #1182726: Reduce Replication Delay when Taking Backup from Slave
# 
# Using --no-lock, --slave-info and --safe-slave-backup options together must
# not stop replication while copying non-InnoDB data.
########################################################################


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

xtrabackup --backup --slave-info --safe-slave-backup --no-lock --target-dir=$topdir/backup
egrep -q '^CHANGE REPLICATION SOURCE TO SOURCE_LOG_FILE='\''mysql-bin.[0-9]+'\'', SOURCE_LOG_POS=[0-9]+;$' \
    $topdir/backup/xtrabackup_slave_info
