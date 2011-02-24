. inc/common.sh

OUTFILE=results/bug606981_innobackupex_out

init
run_mysqld
load_sakila

# Take backup
echo "
[mysqld]
datadir=$mysql_datadir
innodb_flush_method=O_DIRECT
" > $topdir/my.cnf
mkdir -p $topdir/backup
innobackupex --user=root --socket=$mysql_socket --defaults-file=$topdir/my.cnf --stream=tar $topdir/backup > $topdir/backup/out.tar 2>$OUTFILE || dir "innobackupex died with exit code $?"
stop_mysqld

# See if tar4ibd was using O_DIRECT
if ! grep "tar4ibd: using O_DIRECT for the input file" $OUTFILE ;
then
  vlog "tar4ibd was not using O_DIRECT for the input file."
  exit -1
fi

# Remove datadir
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
backup_dir=$topdir/backup
cd $backup_dir
tar -ixvf out.tar
cd - >/dev/null 2>&1 
run_cmd innobackupex --apply-log --defaults-file=$topdir/my.cnf $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
run_cmd innobackupex --copy-back --defaults-file=$topdir/my.cnf $backup_dir

run_mysqld
# Check sakila
${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
stop_mysqld
clean
