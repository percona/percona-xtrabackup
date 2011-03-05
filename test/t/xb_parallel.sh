. inc/common.sh

init
run_mysqld 
load_dbase_schema sakila
load_dbase_data sakila

# Take backup
vlog "Creating the backup directory: $topdir/backup"
mkdir -p $topdir/backup
run_cmd ${IB_BIN} --socket=$mysql_socket $topdir/backup --parallel=8 > $OUTFILE 2>&1 
backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | awk -F\' '{ print $2}'`

#echo "Backup dir in $backup_dir"

stop_mysqld
# Remove datadir
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
echo "###########" >> $OUTFILE
echo "# PREPARE #" >> $OUTFILE
echo "###########" >> $OUTFILE
run_cmd ${IB_BIN} --apply-log $backup_dir >> $OUTFILE 2>&1
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
echo "###########" >> $OUTFILE
echo "# RESTORE #" >> $OUTFILE
echo "###########" >> $OUTFILE
run_cmd ${IB_BIN} --copy-back $backup_dir >> $OUTFILE 2>&1

run_mysqld
# Check sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
stop_mysqld
clean
