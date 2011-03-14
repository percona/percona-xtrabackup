. inc/common.sh

init
run_mysqld
load_dbase_schema sakila
load_dbase_data sakila

PASSWD="123&123"
vlog "Password is $PASSWD"

mkdir -p $topdir/backup

run_cmd ${MYSQLADMIN} ${MYSQL_ARGS} password '$PASSWD'
vlog "mysql password has been changed to contain special char"

vlog "Starting innobackupex wrapper"
run_cmd ${IB_BIN} --user=root --password='$PASSWD' --socket=$mysql_socket $topdir/backup > $OUTFILE 2>&1 
backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | awk -F\' '{ print $2}'`

run_cmd ${MYSQLADMIN} ${MYSQL_ARGS} -p'$PASSWD' password ''

vlog "Stopping database server"
stop_mysqld 
# Remove datadir
vlog "Removing data folder"
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
echo "###########" >> $OUTFILE
echo "# PREPARE #" >> $OUTFILE
echo "###########" >> $OUTFILE
run_cmd ${IB_BIN} --apply-log $backup_dir >> $OUTFILE 2>&1
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
vlog "Performing copyback"
echo "###########" >> $OUTFILE
echo "# PREPARE #" >> $OUTFILE
echo "###########" >> $OUTFILE
run_cmd ${IB_BIN} --copy-back $backup_dir >> $OUTFILE 2>&1

vlog "Starting database server"
# using --skip-grant-tables to override root password restored from backup
run_mysqld --skip-grant-tables
vlog "Database server started"
# Check sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila 
vlog "Stopping database server"
stop_mysqld 
clean
