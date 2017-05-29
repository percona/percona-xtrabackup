########################################################################
# Bug #977101: --safe-slave-backup results in incorrect binlog info
########################################################################

. inc/common.sh

master_id=1
slave_id=2

start_server_with_id $master_id
start_server_with_id $slave_id

setup_slave $slave_id $master_id

# Full backup of the slave server
switch_server $slave_id

# Check that binlog info is correct with --safe-slave-backup
innobackupex --no-timestamp --safe-slave-backup $topdir/backup

cat $topdir/backup/xtrabackup_binlog_info
check_binlog_info $topdir/backup/xtrabackup_binlog_info

# Check that both binlog info and slave info are correct with 
# --safe-slave-backup
rm -rf $topdir/backup
innobackupex --no-timestamp --slave-info --safe-slave-backup $topdir/backup

cat $topdir/backup/xtrabackup_binlog_info
check_binlog_info $topdir/backup/xtrabackup_binlog_info

check_slave_info $topdir/backup/xtrabackup_slave_info
