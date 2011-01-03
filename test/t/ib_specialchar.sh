. inc/common.sh

OUTFILE=results/xb_basic_innobackupex_out

init
run_mysqld
load_dbase_schema sakila
load_dbase_data sakila

PASSWD="123&123"
vlog "Password is $PASSWD"

# Take backup
echo "
[mysqld]
datadir=$mysql_datadir" > $topdir/my.cnf
mkdir -p $topdir/backup

${MYSQLADMIN}  ${MYSQL_ARGS} password '$PASSWD'
vlog "mysql password has been changed to contain special char"

vlog "Starting innobackupex wrapper"
innobackupex-1.5.1 --user=root --password='$PASSWD' --socket=$mysql_socket --defaults-file=$topdir/my.cnf $topdir/backup > $OUTFILE 2>&1 || die "innobackupex-1.5.1 died with exit code $?"
backup_dir=`grep "innobackupex-1.5.1: Backup created in directory" $OUTFILE | awk -F\' '{ print $2}'`

${MYSQLADMIN}  ${MYSQL_ARGS} -p'$PASSWD' password ''

vlog "Stopping database server"
stop_mysqld 
# Remove datadir
vlog "Removing data folder"
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
innobackupex-1.5.1 --apply-log --defaults-file=$topdir/my.cnf $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
vlog "Performing copyback"
innobackupex-1.5.1 --copy-back --defaults-file=$topdir/my.cnf $backup_dir

vlog "Starting database server"
# using --skip-grant-tables to override root password restored from backup
run_mysqld --skip-grant-tables
vlog "Database server started"
# Check sakila
${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila 
vlog "Stopping database server"
stop_mysqld 
clean
