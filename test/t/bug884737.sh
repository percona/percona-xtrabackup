########################################################################
# Bug #884737: xtrabackup's --parallel option asserts / crashes with a
#              value of -1
########################################################################

. inc/common.sh

start_server

# Check that --parallel=<negative value> doesn't blow up
vlog "Creating the backup directory: $topdir/backup"
mkdir -p $topdir/backup
xtrabackup --backup --parallel=-1 --target-dir=$topdir/backup
