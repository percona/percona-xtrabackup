# Expects the following variable to be set before including:
#    mysqld_extra_args: an extra arg to be passed for all mysqld invocations.  
#                       Use this to set special options that influence 
#                       incremental backups, e.g. turns on log archiving or 
#                       changed page bitmap output.
#    ib_inc_extra_args: extra args to be passed to innobackup incremental 
#                       backup invocations.

. inc/common.sh

mysqld_extra_args="$mysqld_extra_args --innodb_file_per_table"

start_server $mysqld_extra_args
load_dbase_schema incremental_sample

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
innobackupex  $topdir/backup
full_backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | awk -F\' '{print $2}'`
vlog "Full backup done to directory $full_backup_dir"

# Changing data

vlog "Making changes to database"
${MYSQL} ${MYSQL_ARGS} -e "create table t2 (a int(11) default null, number int(11) default null) engine=innodb" incremental_sample
let "count=numrow+1"
let "numrow=1000"
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count, $numrow);" incremental_sample
	${MYSQL} ${MYSQL_ARGS} -e "insert into t2 values ($count, $numrow);" incremental_sample
	let "count=count+1"
done

# Rotate bitmap file here and force checkpoint at the same time
shutdown_server
start_server $mysqld_extra_args

i=1001
while [ "$i" -lt "7500" ]
do
        ${MYSQL} ${MYSQL_ARGS} -e "insert into t2 values ($i, repeat(\"ab\", 32500));" incremental_sample
        let "i=i+1"
done

vlog "Changes done"

# Saving the checksum of original table
checksum_test_a=`checksum_table incremental_sample test`
checksum_t2_a=`checksum_table incremental_sample t2`
vlog "Table 'test' checksum is $checksum_test_a"
vlog "Table 't2' checksum is $checksum_t2_a"

vlog "Making incremental backup"

vlog "###############"
vlog "# INCREMENTAL #"
vlog "###############"

# Incremental backup
innobackupex --incremental --incremental-basedir=$full_backup_dir \
    $topdir/backup $ib_inc_extra_args
inc_backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | tail -n 1 | awk -F\' '{print $2}'`
vlog "Incremental backup done to directory $inc_backup_dir"

vlog "Preparing backup"
# Prepare backup
vlog "##############"
vlog "# PREPARE #1 #"
vlog "##############"
innobackupex --apply-log --redo-only $full_backup_dir
vlog "Log applied to full backup"
vlog "##############"
vlog "# PREPARE #2 #"
vlog "##############"
innobackupex --apply-log --redo-only --incremental-dir=$inc_backup_dir \
    $full_backup_dir
vlog "Delta applied to full backup"
vlog "##############"
vlog "# PREPARE #3 #"
vlog "##############"
innobackupex --apply-log $full_backup_dir
vlog "Data prepared for restore"

# Destroying mysql data
stop_server
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Copying files"
vlog "###########"
vlog "# RESTORE #"
vlog "###########"
innobackupex --copy-back $full_backup_dir
vlog "Data restored"

start_server $mysqld_extra_args

vlog "Checking checksums"
checksum_test_b=`checksum_table incremental_sample test`
checksum_t2_b=`checksum_table incremental_sample t2`

if [ "$checksum_test_a" != "$checksum_test_b"  ]
then 
	vlog "Checksums for table 'test' are not equal"
	exit -1
fi

if [ "$checksum_t2_a" != "$checksum_t2_b"  ]
then 
	vlog "Checksums for table 't2' are not equal"
	exit -1
fi

vlog "Checksums are OK"
