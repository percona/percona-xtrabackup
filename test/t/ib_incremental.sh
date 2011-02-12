. inc/common.sh

OUTFILE=results/ib_incremental_innobackupex.out

init
run_mysqld --innodb_file_per_table
load_dbase_schema incremental_sample

# creating my.cnf for innobackupex
echo "
[mysqld]
datadir=$mysql_datadir" > $topdir/my.cnf

# Adding initial rows
vlog "Adding initial rows to database..."
numrow=100
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count, $numrow);" incremental_sample
	let "count=count+1"
done
vlog "Initial rows added"

# Full backup
# backup root directory
mkdir -p $topdir/backup

vlog "Starting backup"
innobackupex --user=root --socket=$mysql_socket --defaults-file=$topdir/my.cnf $topdir/backup > $OUTFILE 2>&1 || die "innobackupex died with exit code $?"
full_backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | awk -F\' '{print $2}'`
vlog "Full backup done to folder $full_backup_dir"

# Changing data

vlog "Making changes to database"
let "count=numrow+1"
let "numrow=500"
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count, $numrow);" incremental_sample
	let "count=count+1"
done
vlog "Changes done"

# Saving the checksum of original table
checksum_a=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" incremental_sample | awk '{print $2}'`
vlog "Table checksum is $checksum_a"

vlog "Making incremental backup"

echo "###############" >> $OUTFILE
echo "# INCREMENTAL #" >> $OUTFILE
echo "###############" >> $OUTFILE

# Incremental backup
innobackupex --user=root --socket=$mysql_socket --defaults-file=$topdir/my.cnf --incremental --incremental-basedir=$full_backup_dir $topdir/backup >> $OUTFILE 2>&1 || die "innobackupex died with exit code $?"
inc_backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | tail -n 1 | awk -F\' '{print $2}'`
vlog "Incremental backup done to filder $inc_backup_dir"

vlog "Preparing backup"
# Prepare backup
echo "##############" >> $OUTFILE
echo "# PREPARE #1 #" >> $OUTFILE
echo "##############" >> $OUTFILE
innobackupex --defaults-file=$topdir/my.cnf --apply-log --redo-only $full_backup_dir >> $OUTFILE 2>&1 || die "innobackupex died with exit code $?"
vlog "Log applied to full backup"
echo "##############" >> $OUTFILE
echo "# PREPARE #2 #" >> $OUTFILE
echo "##############" >> $OUTFILE
innobackupex --defaults-file=$topdir/my.cnf --apply-log --redo-only --incremental-dir=$inc_backup_dir $full_backup_dir >> $OUTFILE 2>&1 || die "innobackupex died with exit code $?"
vlog "Delta applied to full backup"
echo "##############" >> $OUTFILE
echo "# PREPARE #3 #" >> $OUTFILE
echo "##############" >> $OUTFILE
innobackupex --defaults-file=$topdir/my.cnf --apply-log $full_backup_dir >> $OUTFILE 2>&1 || die "innobackupex died with exit code $?"
vlog "Data prepared for restore"

# Destroying mysql data
stop_mysqld
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Copying files"
run_cmd innobackupex --defaults-file=$topdir/my.cnf --copy-back $full_backup_dir
vlog "Data restored"

run_mysqld --innodb_file_per_table

vlog "Cheking checksums"
checksum_b=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" incremental_sample | awk '{print $2}'`

if [ $checksum_a -ne $checksum_b  ]
then 
	vlog "Checksums are not equal"
	exit -1
fi

vlog "Checksums are OK"

stop_mysqld
clean
