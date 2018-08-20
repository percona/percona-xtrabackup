########################################################################
# Bug #1206363: xtrabackup: Unrecognized value O_DIRECT_NO_FSYNC for
#               innodb_flush_method
########################################################################

require_server_version_higher_than 5.6.6

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_flush_method=O_DIRECT_NO_FSYNC
"

start_server

xtrabackup --backup --target-dir=$topdir/backup
