# Expects the following variable to be set before including:
#    MYSQLD_EXTRA_MY_CNF_OPTS: an extra arg to be passed for all mysqld invocations.  
#                       Use this to set special options that influence 
#                       incremental backups, e.g. turns on log archiving or 
#                       changed page bitmap output.
#    ib_inc_extra_args: extra args to be passed to innobackup incremental 
#                       backup invocations.
#    ib_full_backup_extra_args: extra args to be passed to xtrabackup backup invocations.
#    ib_inc_use_lsn:    if 1, use --incremental-lsn instead of
#                       --incremental-basedir

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS:-""}
innodb_file_per_table"

ib_inc_use_lsn=${ib_inc_use_lsn:-""}
ib_full_backup_extra_args=${ib_full_backup_extra_args:-""}

start_server


if [[ $ib_full_backup_extra_args =~ 'page-tracking' ]]; then
  run_cmd ${MYSQL} ${MYSQL_ARGS} -e "INSTALL COMPONENT \"file://component_mysqlbackup\""
fi

load_dbase_schema incremental_sample

# Adding initial rows
multi_row_insert incremental_sample.test \({1..100},100\)

vlog "Starting backup"
full_backup_dir=$topdir/full_backup
xtrabackup --backup $ib_full_backup_extra_args  --target-dir=$full_backup_dir

# Changing data

vlog "Making changes to database"
${MYSQL} ${MYSQL_ARGS} -e "create table t2 (a int(11) default null, number text default null) engine=innodb" incremental_sample

multi_row_insert incremental_sample.test \({101..1000},1000\)
multi_row_insert incremental_sample.t2 \({101..1000},1000\)

# Rotate bitmap file here and force checkpoint at the same time
shutdown_server
start_server

multi_row_insert incremental_sample.t2 \({1001..7500},REPEAT\(\'ab\',32500\)\)

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
inc_backup_dir=$topdir/backup_incremental
if [ "$ib_inc_use_lsn" = "1" ]
then
    inc_lsn=`grep to_lsn $full_backup_dir/xtrabackup_checkpoints | \
             sed 's/to_lsn = //'`

    [ -z "$inc_lsn" ] && die "Couldn't read to_lsn from xtrabackup_checkpoints"

    ib_inc_extra_args="${ib_inc_extra_args:-""} --incremental-lsn=$inc_lsn"
else
    ib_inc_extra_args="${ib_inc_extra_args:-""} --incremental-basedir=$full_backup_dir"
fi

$MYSQL $MYSQL_ARGS \
    -e 'START TRANSACTION; DELETE FROM t2 LIMIT 1000; SELECT SLEEP(200000);' \
    incremental_sample &
job_id=$!

xtrabackup --backup $ib_inc_extra_args --target-dir=$inc_backup_dir

kill -SIGKILL $job_id

vlog "Preparing backup"
# Prepare backup
vlog "##############"
vlog "# PREPARE #1 #"
vlog "##############"
xtrabackup --prepare --apply-log-only --target-dir=$full_backup_dir
vlog "Log applied to full backup"
vlog "##############"
vlog "# PREPARE #2 FINAL PREPARE #"
vlog "##############"
xtrabackup --prepare --incremental-dir=$inc_backup_dir \
    --target-dir=$full_backup_dir

# Destroying mysql data
stop_server
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Copying files"
vlog "###########"
vlog "# RESTORE #"
vlog "###########"
xtrabackup --copy-back --target-dir=$full_backup_dir
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

if egrep -q "We scanned the log up to [0-9]+. A checkpoint was at [0-9]+" $OUTFILE
then
    die "PXB-1699 is present"
fi
