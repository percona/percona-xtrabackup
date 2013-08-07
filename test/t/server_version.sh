#########################################################################
# Test server version auto-detection
#########################################################################

. inc/common.sh

start_server

# Remove the explicit --ibbackup option from IB_ARGS
IB_ARGS="--defaults-file=$MYSQLD_VARDIR/my.cnf"

backup_dir=$topdir/backup
innobackupex --no-timestamp $backup_dir
vlog "Backup created in directory $backup_dir"

stop_server
# Remove datadir
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
vlog "###########"
vlog "# PREPARE #"
vlog "###########"
innobackupex --apply-log $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
vlog "###########"
vlog "# RESTORE #"
vlog "###########"
innobackupex --copy-back $backup_dir

start_server
