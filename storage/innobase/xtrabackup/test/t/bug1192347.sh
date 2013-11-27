##############################################################################
# Bug #1192347: innobackupex terminates if --galera-info specified when
#               backing up non-galera server
##############################################################################

. inc/common.sh

is_galera && skip_test "Requires a server without Galera support"

start_server

innobackupex --no-timestamp --galera-info $topdir/backup

# Check that to xtrabackup_binlog_info file is created
if [ -f $topdir/backup/xtrabackup_galera_info ]
then
    vlog "$topdir/backup/xtrabackup_galera_info should not be created!"
    exit -1
fi
