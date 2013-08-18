. inc/common.sh

if ! is_xtradb && ! is_server_version_higher_than 5.6.0
then
    skip_test "Requires XtraDB or MySQL 5.6+"
fi

import_option=""

if is_xtradb
then
    if is_server_version_higher_than 5.6.0
        # No additional options for XtraDB 5.6+
    then
        true
    elif is_server_version_higher_than 5.5.0
    then
        import_option="--innodb_import_table_from_xtrabackup=1"
    else
        import_option="--innodb_expand_import=1"
    fi
fi

mysql_extra_args="--innodb_file_per_table $import_option \
--innodb_file_format=Barracuda"

# Starting database server
start_server $mysql_extra_args

backup_dir=$topdir/xb_export_backup
rm -rf $backup_dir
mkdir $backup_dir

# Loading table schema
load_dbase_schema incremental_sample

# Adding some data to database
multi_row_insert incremental_sample.test \({1..100},100\)

checksum_1=`checksum_table incremental_sample test`
rowsnum_1=`${MYSQL} ${MYSQL_ARGS} -Ns -e "select count(*) from test" incremental_sample`
vlog "rowsnum_1 is $rowsnum_1"
vlog "checksum_1 is $checksum_1"

# Performing table backup
xtrabackup --datadir=$mysql_datadir --backup --target-dir=$backup_dir
vlog "Table was backed up"

vlog "Re-initializing database server"
stop_server
rm -rf ${MYSQLD_DATADIR}
start_server $mysql_extra_args
load_dbase_schema incremental_sample
vlog "Database was re-initialized"

run_cmd ${MYSQL} ${MYSQL_ARGS} -e "alter table test discard tablespace;" incremental_sample

xtrabackup --datadir=$mysql_datadir --prepare --export \
    --target-dir=$backup_dir

run_cmd cp $backup_dir/incremental_sample/test* $mysql_datadir/incremental_sample/
run_cmd ls -lah $mysql_datadir/incremental_sample/
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "alter table test import tablespace" incremental_sample
vlog "Table has been imported"

vlog "Cheking checksums"
checksum_2=`checksum_table incremental_sample test`
vlog "checksum_2 is $checksum_2"
rowsnum_1=`${MYSQL} ${MYSQL_ARGS} -Ns -e "select count(*) from test" incremental_sample`
vlog "rowsnum_1 is $rowsnum_1"

if [ "$checksum_1" != "$checksum_2"  ]
then 
	vlog "Checksums are not equal"
	exit -1
fi

vlog "Checksums are OK"

# Tablespace import is asynchronous, so shutdown the server to have
# consistent backup results. Otherwise we risk ending up with no test.ibd
# in the backup in case importing has not finished before taking backup

shutdown_server
start_server $mysql_extra_args

# Some testing queries
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "select count(*) from test;" incremental_sample
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "show create table test;" incremental_sample

# Performing full backup of imported table
mkdir -p $topdir/backup/full

xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup/full

xtrabackup --datadir=$mysql_datadir --prepare --apply-log-only \
    --target-dir=$topdir/backup/full

xtrabackup --datadir=$mysql_datadir --prepare \
    --target-dir=$topdir/backup/full

run_cmd ${MYSQL} ${MYSQL_ARGS} -e "delete from test;" incremental_sample

stop_server

cd $topdir/backup/full
cp -r * $mysql_datadir
cd -

start_server $mysql_extra_args

vlog "Cheking checksums"
checksum_3=`checksum_table incremental_sample test`
vlog "checksum_3 is $checksum_3"

if [ "$checksum_3" != "$checksum_2"  ]
then
        vlog "Checksums are not equal"
        exit -1
fi

vlog "Checksums are OK"

stop_server

rm -rf $backup_dir
