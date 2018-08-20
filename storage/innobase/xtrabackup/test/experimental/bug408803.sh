. inc/common.sh

start_server --interactive_timeout=1 --wait_timeout=1
load_dbase_schema sakila
load_dbase_data sakila

# Take backup
backup_dir=$topdir/backup
mkdir -p $topdir/backup
xtrabackup --backup --target-dir=$backup_dir

stop_server
# Remove datadir
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
echo "###########" >> $OUTFILE
echo "# PREPARE #" >> $OUTFILE
echo "###########" >> $OUTFILE
xtrabackup --prepare --target-dir=$backup_dir

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
echo "###########" >> $OUTFILE
echo "# RESTORE #" >> $OUTFILE
echo "###########" >> $OUTFILE
xtrabackup --copy-back --target-dir=$backup_dir

start_server
# Check sakila
${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
