################################################################################
# Bug #1062684: Applying incremental backup using xtrabackup 2.0.3 fails when
#               innodb_data_file_path = ibdata1:2000M;ibdata2:10M:autoextend is
#               set in [mysqld]
################################################################################

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-data-file-path=ibdata1:10M;ibdata2:5M:autoextend
"

start_server
load_dbase_schema incremental_sample

# Adding initial rows
vlog "Adding initial rows to database..."
${MYSQL} ${MYSQL_ARGS} -e "insert into test values (1, 1);" incremental_sample

# Full backup
# backup root directory
mkdir -p $topdir/backup

vlog "Starting backup"
full_backup_dir=$topdir/backup/full
innobackupex --no-timestamp $full_backup_dir
vlog "Full backup done to directory $full_backup_dir"

# Changing data

vlog "Making changes to database"
${MYSQL} ${MYSQL_ARGS} -e "create table t2 (a int(11) default null, number int(11) default null) engine=innodb" incremental_sample
${MYSQL} ${MYSQL_ARGS} -e "insert into test values (10, 1);" incremental_sample
${MYSQL} ${MYSQL_ARGS} -e "insert into t2 values (10, 1);" incremental_sample
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
inc_backup_dir=$topdir/backup/inc
innobackupex --incremental --incremental-basedir=$full_backup_dir \
    --no-timestamp $inc_backup_dir
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

start_server

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
