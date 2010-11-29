. inc/common.sh

OUTFILE=results/xb_parallel_innobackupex_out

init
run_mysqld --innodb_file_per_table
load_sakila

# Take backup
echo "
[mysqld]
datadir=$mysql_datadir" > $topdir/my.cnf
vlog "Creating the backup directory: $topdir/backup"
mkdir -p $topdir/backup
vlog "Running innobackupex-1.5.1 as innobackupex-1.5.1 --user=root --socket=$mysql_socket --defaults-file=$topdir/my.cnf $topdir/backup --parallel=8"
innobackupex-1.5.1 --user=root --socket=$mysql_socket --defaults-file=$topdir/my.cnf $topdir/backup --parallel=8 > $OUTFILE 2>&1 || die "innobackupex-1.5.1 died with exit code $?"
backup_dir=`grep "innobackupex-1.5.1: Backup created in directory" $OUTFILE | awk -F\' '{ print $2}'`

#echo "Backup dir in $backup_dir"

stop_mysqld
# Remove datadir
rm -r $mysql_datadir
#init_mysql_dir
# Restore sakila
vlog "Applying log"
innobackupex-1.5.1 --apply-log --defaults-file=$topdir/my.cnf $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex-1.5.1 --copy-back --defaults-file=$topdir/my.cnf $backup_dir

run_mysqld
# Check sakila
${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
stop_mysqld
clean
