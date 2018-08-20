############################################################################
# Bug #976945: innodb_log_block_size=4096 is not supported
############################################################################
. inc/common.sh

require_xtradb
require_server_version_lower_than 5.7.0

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_log_block_size=4096
"
start_server
load_sakila

# Full backup
vlog "Starting backup"

full_backup_dir=${MYSQLD_VARDIR}/full_backup
xtrabackup --backup --target-dir=$full_backup_dir

vlog "Preparing backup"
xtrabackup --prepare --apply-log-only --target-dir=$full_backup_dir
vlog "Log applied to full backup"

# Destroying mysql data
stop_server
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Copying files to their original locations"
xtrabackup --copy-back --target-dir=$full_backup_dir
vlog "Data restored"

start_server --innodb_log_block_size=4096
