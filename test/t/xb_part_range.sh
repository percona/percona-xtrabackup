. inc/common.sh

init
run_mysqld
load_dbase_schema part_range_sample

# Adding 10k rows

vlog "Adding initial rows to database..."

numrow=500
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count);" part_range_sample
	let "count=count+1"
done


vlog "Initial rows added"

# Full backup

# Full backup folder
mkdir -p $topdir/data/full
# Incremental data
mkdir -p $topdir/data/delta

vlog "Starting backup"

xtrabackup --no-defaults --datadir=$mysql_datadir --backup \
    --target-dir=$topdir/data/full

vlog "Full backup done"

# Changing data in sakila

vlog "Making changes to database"

numrow=500
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count);" part_range_sample
	let "count=count+1"
done

vlog "Changes done"

# Saving the checksum of original table
checksum_a=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" part_range_sample | awk '{print $2}'`

vlog "Table checksum is $checksum_a - before backup"

vlog "Making incremental backup"

# Incremental backup
xtrabackup --no-defaults --datadir=$mysql_datadir --backup \
    --target-dir=$topdir/data/delta --incremental-basedir=$topdir/data/full

vlog "Incremental backup done"
vlog "Preparing backup"

# Prepare backup
xtrabackup --no-defaults --datadir=$mysql_datadir --prepare --apply-log-only \
    --target-dir=$topdir/data/full
vlog "Log applied to backup"
xtrabackup --no-defaults --datadir=$mysql_datadir --prepare --apply-log-only \
    --target-dir=$topdir/data/full --incremental-dir=$topdir/data/delta
vlog "Delta applied to backup"
xtrabackup --no-defaults --datadir=$mysql_datadir --prepare \
    --target-dir=$topdir/data/full
vlog "Data prepared for restore"

# removing rows
vlog "Table cleared"
${MYSQL} ${MYSQL_ARGS} -e "delete from test;" part_range_sample

# Restore backup

stop_mysqld

vlog "Copying files"

cd $topdir/data/full/
cp -r * $mysql_datadir
cd $topdir

vlog "Data restored"

run_mysqld

vlog "Cheking checksums"
checksum_b=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" part_range_sample | awk '{print $2}'`

if [ $checksum_a -ne $checksum_b  ]
then 
	vlog "Checksums are not equal"
	exit -1
fi

vlog "Checksums are OK"

stop_mysqld
clean
