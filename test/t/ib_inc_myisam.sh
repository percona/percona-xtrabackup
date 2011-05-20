#########################
# Test for MyISAM in incremental backups (bug 759701)
#########################
. inc/common.sh

init
run_mysqld --innodb_file_per_table

${MYSQL} ${MYSQL_ARGS} -e "CREATE DATABASE inctest;"
${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t1 (a INT) ENGINE=MyISAM" inctest
${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t2 (b INT) ENGINE=InnoDB" inctest

# Adding initial rows to MyISAM table
vlog "Adding initial rows to database..."
numrow=100
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into t1 values ($count);" inctest
	let "count=count+1"
done
vlog "Initial rows added to MyISAM table"

# Adding initial rows to InnoDB table
vlog "Adding initial rows to database..."
numrow=100
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into t2 values ($count);" inctest
	let "count=count+1"
done
vlog "Initial rows added to InnoDB table"


# Full backup
# backup root directory
mkdir -p $topdir/backup

vlog "Starting backup"
run_cmd ${IB_BIN} --user=root --socket=$mysql_socket $topdir/backup > $OUTFILE 2>&1 
full_backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | awk -F\' '{print $2}'`
vlog "Full backup done to directory $full_backup_dir"

# Changing data of MyISAM table
vlog "Making changes to database"
let "count=101"
let "numrow=500"
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into t1 values ($count);" inctest
	let "count=count+1"
done
vlog "Changes done"

# Changing data of InnoDB table
vlog "Making changes to database"
let "count=101"
let "numrow=500"
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into t2 values ($count);" inctest
	let "count=count+1"
done
vlog "Changes done"


# Saving the checksum of original table
checksum_myisam_a=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table t1;" inctest | awk '{print $2}'`
checksum_innodb_a=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table t2;" inctest | awk '{print $2}'`
vlog "MyISAM table checksum is $checksum_myisam_a"
vlog "InnoDB table checksum is $checksum_innodb_a"

vlog "Making incremental backup"

echo "###############" >> $OUTFILE
echo "# INCREMENTAL #" >> $OUTFILE
echo "###############" >> $OUTFILE

# Incremental backup
run_cmd ${IB_BIN} --socket=$mysql_socket --incremental --incremental-basedir=$full_backup_dir $topdir/backup >> $OUTFILE 2>&1 
inc_backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | tail -n 1 | awk -F\' '{print $2}'`
vlog "Incremental backup done to directory $inc_backup_dir"

vlog "Preparing backup"
# Prepare backup
echo "##############" >> $OUTFILE
echo "# PREPARE #1 #" >> $OUTFILE
echo "##############" >> $OUTFILE
run_cmd ${IB_BIN} --apply-log --redo-only $full_backup_dir >> $OUTFILE 2>&1 
vlog "Log applied to full backup"
echo "##############" >> $OUTFILE
echo "# PREPARE #2 #" >> $OUTFILE
echo "##############" >> $OUTFILE
run_cmd ${IB_BIN} --apply-log --redo-only --incremental-dir=$inc_backup_dir $full_backup_dir >> $OUTFILE 2>&1 
vlog "Delta applied to full backup"
echo "##############" >> $OUTFILE
echo "# PREPARE #3 #" >> $OUTFILE
echo "##############" >> $OUTFILE
run_cmd ${IB_BIN} --apply-log $full_backup_dir >> $OUTFILE 2>&1 
vlog "Data prepared for restore"

# Destroying mysql data
stop_mysqld
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Copying files"
echo "###########" >> $OUTFILE
echo "# RESTORE #" >> $OUTFILE
echo "###########" >> $OUTFILE
run_cmd ${IB_BIN} --copy-back $full_backup_dir >> $OUTFILE 2>&1
vlog "Data restored"

run_mysqld --innodb_file_per_table

vlog "Checking checksums"
checksum_myisam_b=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table t1;" inctest | awk '{print $2}'`
checksum_innodb_b=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table t2;" inctest | awk '{print $2}'`
vlog "MyISAM table checksum after backup is $checksum_myisam_b"
vlog "InnoDB table checksum after backup is $checksum_innodb_b"

if [ $checksum_myisam_a -ne $checksum_myisam_b  ] || [ $checksum_innodb_a -ne $checksum_innodb_b ];
then 
	vlog "Checksums are not equal"
	exit -1
fi

vlog "Checksums are OK"

stop_mysqld
clean
