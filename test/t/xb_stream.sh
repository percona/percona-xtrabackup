. inc/common.sh

OUTFILE=results/xb_stream_innobackupex_out

init
run_mysqld
load_sakila

# Take backup
echo "
[mysqld]
datadir=$mysql_datadir" > $topdir/my.cnf
mkdir -p $topdir/backup
innobackupex-1.5.1 --user=root --socket=$mysql_socket --defaults-file=$topdir/my.cnf --stream=tar $topdir/backup > $topdir/backup/out.tar 2> $OUTFILE || die "innobackupex-1.5.1 died with exit code $?"

stop_mysqld
# Remove datadir
rm -r $mysql_datadir
#init_mysql_dir
# Restore sakila
vlog "Applying log"
backup_dir=$topdir/backup
cd $backup_dir
tar -ixvf out.tar
cd - >/dev/null 2>&1 
innobackupex-1.5.1 --apply-log --defaults-file=$topdir/my.cnf $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex-1.5.1 --copy-back --defaults-file=$topdir/my.cnf $backup_dir

run_mysqld
# Check sakila
${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
stop_mysqld
clean
