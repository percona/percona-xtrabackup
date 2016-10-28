########################################################################
# Bug #1227240: backups not working with archive logging enabled
########################################################################

require_xtradb
require_server_version_higher_than 5.6.10
require_server_version_lower_than 5.7.0

init_server_variables 1
switch_server 1
ARCH_LOG_DIR=$MYSQLD_VARDIR/arch_log
BACKUP_DIR=$MYSQLD_VARDIR/backup
reset_server_variables 1

MYSQLD_EXTRA_MY_CNF_OPTS="innodb-log-archive=ON
                          innodb-log-arch-dir=$ARCH_LOG_DIR"
mkdir -p $ARCH_LOG_DIR $BACKUP_DIR

start_server
# If xtrabackup finished whithout error the test passed.
xtrabackup --backup --datadir=$MYSQLD_DATADIR --target-dir=$BACKUP_DIR
