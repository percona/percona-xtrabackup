########################################################################
# Bug #1347698: log parameter on mysqld section shouldn't be read by
#                xtrabackup
########################################################################

. inc/common.sh

require_server_version_lower_than 5.6.0

# add --log parameter to [mysqld] section of my.cnf
MYSQLD_EXTRA_MY_CNF_OPTS="
log=$MYSQLD_VARDIR/general.log
"

start_server

vlog "Creating the backup directory: $topdir/backup"
mkdir -p $topdir/backup
xtrabackup --backup --target-dir=$topdir/backup
