. inc/common.sh

init
run_mysqld
mysqld_innodb_file_per_table
load_sakila2

# Adding 10k rows

vlog "Adding initial rows to database..."

sakila2_numrow=1000
sakila2_count=0
while [ "$sakila2_numrow" -gt "$sakila2_count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($sakila2_count, $sakila2_numrow);" sakila2
	let "sakila2_count=sakila2_count+1"
done


vlog "Initial rows added"

checksum_a=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" sakila2 | awk '{print $2}'`
vlog "Table checksum is $checksum_a"

# Partial backup

# Backup folder
mkdir -p $topdir/data/parted

vlog "Starting backup"
xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/parted --tables="^sakila2[.]test"
vlog "Partial backup done"

# Prepare backup
xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/data/parted
vlog "Data prepared for restore"

# removing rows
${MYSQL} ${MYSQL_ARGS} -e "delete from test;" sakila2
vlog "Table cleared"

# Restore backup

stop_mysqld

vlog "Copying files"

cd $topdir/data/parted/
cp -r * --target-directory=$mysql_datadir
cd $mysql_datadir
chown -R mysql:mysql *
cd $topdir

vlog "Data restored"

run_mysqld
mysqld_innodb_file_per_table

vlog "Cheking checksums"
checksum_b=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" sakila2 | awk '{print $2}'`

if [ $checksum_a -ne $checksum_b  ]
then 
	vlog "Checksums are not equal"
	exit -1
fi

vlog "Checksums are OK"

stop_mysqld
clean
