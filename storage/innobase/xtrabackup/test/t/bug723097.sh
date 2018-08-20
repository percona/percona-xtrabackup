. inc/common.sh

start_server --innodb_file_per_table

FULL_DIR="$topdir/full"

vlog "Loading data from sql file"
run_cmd ${MYSQL} ${MYSQL_ARGS} test < inc/bug723097.sql

vlog "Saving checksum"
checksum_a=`checksum_table test messages`
vlog "Checksum before is $checksum_a"

vlog "Creating backup directory"
mkdir -p $FULL_DIR
vlog "Starting backup"

xtrabackup --datadir=$mysql_datadir --backup --target-dir=$FULL_DIR
vlog "Backup is done"
xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$FULL_DIR
vlog "Data prepared fo restore"
stop_server

cp -r $FULL_DIR/test/* $mysql_datadir/test
start_server --innodb_file_per_table
checksum_b=`checksum_table test messages`
vlog "Checksum after is $checksum_b"

if [ "$checksum_a" -eq "$checksum_b" ]
then
	vlog "Checksums are Ok"
else
	vlog "Checksums are not equal"
	exit -1
fi
