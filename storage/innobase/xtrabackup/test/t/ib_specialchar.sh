. inc/common.sh

start_server

load_dbase_schema sakila
load_dbase_data sakila

PASSWD="123&123"
vlog "Password is $PASSWD"

mkdir -p $topdir/backup

run_cmd ${MYSQLADMIN} ${MYSQL_ARGS} password '$PASSWD'
vlog "mysql password has been changed to contain special char"

vlog "Starting xtrabackup"
xtrabackup --backup --password='$PASSWD' --target-dir=$topdir/backup

run_cmd ${MYSQLADMIN} ${MYSQL_ARGS} -p'$PASSWD' password ''

vlog "Stopping database server"
stop_server
# Remove datadir
vlog "Removing data folder"
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
vlog "###########"
vlog "# PREPARE #"
vlog "###########"
xtrabackup --prepare --target-dir=$topdir/backup
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
vlog "Performing copyback"
vlog "###########"
vlog "# PREPARE #"
vlog "###########"
xtrabackup --copy-back --target-dir=$topdir/backup

vlog "Starting database server"
# using --skip-grant-tables to override root password restored from backup
start_server --skip-grant-tables
vlog "Database server started"
# Check sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila 
