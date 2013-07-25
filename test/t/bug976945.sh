############################################################################
# Bug #976945: innodb_log_block_size=4096 is not supported
############################################################################
. inc/common.sh

require_xtradb

# Require XtraDB <= 5.5 until bug #1194828 is fixed
is_server_version_lower_than 5.6.0 || \
    skip_test "Doesn't work with XtraDB 5.6, bug #1194828"

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_log_block_size=4096
"
start_server
load_sakila

# Full backup
vlog "Starting backup"

full_backup_dir=${MYSQLD_VARDIR}/full_backup
innobackupex  --no-timestamp $full_backup_dir

vlog "Preparing backup"
innobackupex --apply-log --redo-only $full_backup_dir
vlog "Log applied to full backup"

# Destroying mysql data
stop_server
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Copying files to their original locations"
innobackupex --copy-back $full_backup_dir
vlog "Data restored"

start_server --innodb_log_block_size=4096
