###############################################################################
# Bug #606981: linux + o_direct + stream backup = broken?
# Bug #925354: tar4ibd does not use O_DIRECT for per-table *.ibd when it should
###############################################################################

. inc/common.sh

if test "`uname -s`" != "Linux"
then
    echo "Requires Linux" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

init
run_mysqld --innodb_file_per_table
load_sakila

# Take backup
echo "innodb_flush_method=O_DIRECT" >> $topdir/my.cnf
mkdir -p $topdir/backup
innobackupex --stream=tar $topdir/backup > $topdir/backup/out.tar
stop_mysqld

# See if tar4ibd was using O_DIRECT for all InnoDB files
cnt=`grep "tar4ibd: using O_DIRECT for the input file" $OUTFILE | wc -l`
if [ "$cnt" -ne 16 ]
then
  vlog "tar4ibd was not using O_DIRECT for all input files."
  exit -1
fi

# Remove datadir
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
backup_dir=$topdir/backup
cd $backup_dir
$TAR -ixvf out.tar
cd - >/dev/null 2>&1 
innobackupex --apply-log --defaults-file=$topdir/my.cnf $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex --copy-back --defaults-file=$topdir/my.cnf $backup_dir

run_mysqld
# Check sakila
${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
