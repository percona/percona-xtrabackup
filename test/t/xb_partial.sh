. inc/common.sh

init
run_mysqld --innodb_file_per_table
load_incremental_sample

# Adding 10k rows

vlog "Adding initial rows to database..."

numrow=1000
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count, $numrow);" incremental_sample
	let "count=count+1"
done


vlog "Initial rows added"

checksum_a=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" incremental_sample | awk '{print $2}'`
vlog "Table checksum is $checksum_a"

# Partial backup

# Backup folder
mkdir -p $topdir/data/parted

vlog "Starting backup"
xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/parted --tables="^incremental_sample[.]test"
vlog "Partial backup done"

# Prepare backup
xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/data/parted
vlog "Data prepared for restore"

# removing rows
${MYSQL} ${MYSQL_ARGS} -e "delete from test;" incremental_sample
vlog "Table cleared"

# Restore backup

stop_mysqld

vlog "Copying files"

cd $topdir/data/parted/
cp -r * $mysql_datadir
cd $topdir

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
