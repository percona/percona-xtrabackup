. inc/common.sh

init

backup_dir=/tmp/xb_export_backup
rm -rf $backup_dir
mkdir $backup_dir
# Starting database server
run_mysqld --innodb_file_per_table

# Loading table schema
load_dbase_schema incremental_sample

# Adding some data to database
vlog "Adding initial rows to database..."
numrow=100
count=0
while [ "$numrow" -gt "$count" ]
do
	run_cmd ${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count, $numrow);" incremental_sample
	let "count=count+1"
done
vlog "Initial rows added"

checksum_1=`${MYSQL} ${MYSQL_ARGS} -e "checksum table test" incremental_sample | awk '{print $2}'`
vlog "checksum_1 is $checksum_1"

# Creating a directory for tables
mkdir -p $topdir/../backup

# Performing table backup
run_cmd ${XB_BIN} --datadir=$mysql_datadir --backup --tables="^incremental_sample[.]test" --target-dir=$backup_dir
vlog "Table was backed up"

vlog "Re-initializing database server"
stop_mysqld
clean
init
run_mysqld --innodb_file_per_table --innodb_expand_import=1
load_dbase_schema incremental_sample
vlog "Database was re-initialized"

run_cmd ${MYSQL} ${MYSQL_ARGS} -e "alter table test discard tablespace" incremental_sample
run_cmd ${XB_BIN} --datadir=$mysql_datadir --prepare --export --target-dir=$backup_dir
cp $backup_dir/incremental_sample/test* $mysql_datadir/incremental_sample/
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "alter table test import tablespace" incremental_sample
vlog "Table has been imported"

vlog "Cheking checksums"
checksum_2=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" incremental_sample | awk '{print $2}'`

if [ $checksum_1 -ne $checksum_2  ]
then 
	vlog "Checksums are not equal"
	exit -1
fi

vlog "Checksums are OK"

# Some testing queries
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "select count(*) from test;" incremental_sample
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "show create table test;" incremental_sample

# Performing full backup of imported table
mkdir -p $topdir/backup/full

xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup/full
xtrabackup --datadir=$mysql_datadir --prepare --apply-log-only --target-dir=$topdir/backup/full
xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/backup/full

run_cmd ${MYSQL} ${MYSQL_ARGS} -e "delete from test;" incremental_sample

stop_mysqld

cd $topdir/backup/full
cp -r * $mysql_datadir
cd -

run_mysqld --innodb_file_per_table

vlog "Cheking checksums"
checksum_3=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" incremental_sample | awk '{print $2}'`

if [ $checksum_3 -ne $checksum_2  ]
then
        vlog "Checksums are not equal"
        exit -1
fi

vlog "Checksums are OK"

rm -rf $backup_dir
stop_mysqld
clean
