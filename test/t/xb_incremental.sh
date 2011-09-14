. inc/common.sh

init
run_mysqld --innodb_file_per_table
load_dbase_schema incremental_sample

# Adding 10k rows

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

# Full backup folder
mkdir -p $topdir/data/full
# Incremental data
mkdir -p $topdir/data/delta

vlog "Starting backup"

xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/full

vlog "Full backup done"

# Changing data in sakila

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
checksum_a=`checksum_table incremental_sample test`

vlog "Table checksum is $checksum_a"
vlog "Making incremental backup"

# Incremental backup
xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/delta --incremental-basedir=$topdir/data/full

vlog "Incremental backup done"
vlog "Preparing backup"

# Prepare backup
xtrabackup --datadir=$mysql_datadir --prepare --apply-log-only \
    --target-dir=$topdir/data/full
vlog "Log applied to backup"
xtrabackup --datadir=$mysql_datadir --prepare --apply-log-only \
    --target-dir=$topdir/data/full --incremental-dir=$topdir/data/delta
vlog "Delta applied to backup"
xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/data/full
vlog "Data prepared for restore"

# removing rows
vlog "Table cleared"
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "delete from test;" incremental_sample

# Restore backup
stop_mysqld
vlog "Copying files"
cd $topdir/data/full/
cp -r * $mysql_datadir
cd $topdir

vlog "Data restored"
run_mysqld --innodb_file_per_table

vlog "Checking checksums"
checksum_b=`checksum_table incremental_sample test`

if [ $checksum_a -ne $checksum_b  ]
then 
	vlog "Checksums are not equal"
	exit -1
fi

vlog "Checksums are OK"
