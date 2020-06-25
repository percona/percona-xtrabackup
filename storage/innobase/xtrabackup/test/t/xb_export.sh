. inc/common.sh

if ! is_xtradb && ! is_server_version_higher_than 5.6.0
then
    skip_test "Requires XtraDB or MySQL 5.6+"
fi

function init_schema() {
    local database_name=$1
    local table_name=$2
    local columns_count=$3
    local part=$4

    local cmd="create database if not exists $database_name; "
    cmd+="create table $database_name.$table_name ("
    for ((c=1; c<=$columns_count; ++c ))
    do
        cmd+="\`c$c\` int,"
    done
    for ((c=1; c<=$columns_count; ++c ))
    do
        cmd+="index \`c$c\` (\`c$c\`),"
    done
    cmd=${cmd%?}
    cmd+=")"
    cmd+=" engine=InnoDB"
    if [ "$part" == "y" ] ; then
        cmd+=" PARTITION BY RANGE (c1)"
        cmd+=" (PARTITION p0 VALUES LESS THAN (10) ENGINE = InnoDB,"
        cmd+=" PARTITION p1 VALUES LESS THAN (50) ENGINE = InnoDB,"
        cmd+=" PARTITION p2 VALUES LESS THAN MAXVALUE ENGINE = InnoDB)"
    fi

    echo $cmd

    echo "$cmd" | run_cmd $MYSQL $MYSQL_ARGS
}

function insert_data() {
    local database_name=$1
    local table_name=$2
    local columns_count=$3
    local rows_count=$4

    local cmd="INSERT INTO $database_name.$table_name VALUES"
    local val=0
    for ((r=1; r<=$rows_count; ++r ))
    do
        cmd+="("
        for ((c=1; c<=$columns_count; ++c ))
        do
            cmd+="$val,"
            ((++val))
        done
        cmd=${cmd%?}
        cmd+="),"
    done
    cmd=${cmd%?}
    cmd+=";"
    echo "$cmd" | run_cmd $MYSQL $MYSQL_ARGS
}

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

mysql_extra_args="--innodb_file_per_table $import_option"

# Starting database server
start_server $mysql_extra_args

backup_dir=$topdir/xb_export_backup
rm -rf $backup_dir
mkdir $backup_dir
indices_count=10
rows_count=3

# Pre-5.6.0 secnario: not too many indexes, both .exp files generated Ok.
max_indices_count=30
exp_files_count=2


if is_server_version_higher_than 5.6.0
then
    # Post-5.6.0 scenario: many indexes, only one .exp file is generated.
    # Despite that both .cfg are generated and both tables can be imported.
    max_indices_count=64
    exp_files_count=1
fi

# Loading table schema
init_schema incremental_sample test $max_indices_count "n"
init_schema incremental_sample test2 $indices_count "n"
init_schema incremental_sample test3 $indices_count "y"

# Adding some data to database
insert_data incremental_sample test $max_indices_count $rows_count
insert_data incremental_sample test2 $indices_count $rows_count

# check if we can import all the columns type in mysql
mysql -e 'create table test4(col1 bool, col2 int, col3 float, col4 double,
col5 timestamp, col6 long, col7 date, col8 time, col9 datetime, col10 year,
col11 varchar(20), col12 bit ,col13 decimal, col14 blob, col16 json,
col17 mediumtext, col18 enum("01","2"),col19 SET("0","1","2") )' incremental_sample;

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
init_schema incremental_sample test $max_indices_count "n"
init_schema incremental_sample test2 $indices_count "n"
init_schema incremental_sample test3 $indices_count "y"
# check if we can import all the columns type in mysql
mysql -e 'create table test4(col1 bool, col2 int, col3 float, col4 double,
col5 timestamp, col6 long, col7 date, col8 time, col9 datetime, col10 year,
col11 varchar(20), col12 bit ,col13 decimal, col14 blob, col16 json,
col17 mediumtext, col18 enum("01","2"),col19 SET("0","1","2") )' incremental_sample;
vlog "Database was re-initialized"

mysql -e "alter table test discard tablespace;" incremental_sample
mysql -e "alter table test2 discard tablespace;" incremental_sample
mysql -e "alter table test3 discard tablespace;" incremental_sample
mysql -e "alter table test4 discard tablespace;" incremental_sample

xtrabackup --datadir=$mysql_datadir --prepare --export \
    --target-dir=$backup_dir

ls -tlr $backup_dir/incremental_sample/*cfg
cfg_count=`find $backup_dir/incremental_sample -name '*.cfg' | wc -l`
vlog "Verifying .cfg files in backup, expecting 6."
if [ $cfg_count -ne 6 ]
then
   vlog "Expecting 6 cfg files. Found only $cfg_count"
   exit -1
fi

run_cmd cp $backup_dir/incremental_sample/test* $mysql_datadir/incremental_sample/
mysql -e "alter table test import tablespace" incremental_sample
mysql -e "alter table test2 import tablespace" incremental_sample
mysql -e "alter table test3 import tablespace" incremental_sample
mysql -e "alter table test4 import tablespace" incremental_sample
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
mysql -e "select count(*) from test;" incremental_sample
mysql -e "show create table test;" incremental_sample

# Performing full backup of imported table
mkdir -p $topdir/backup/full

xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup/full

xtrabackup --datadir=$mysql_datadir --prepare --apply-log-only \
    --target-dir=$topdir/backup/full

xtrabackup --datadir=$mysql_datadir --prepare \
    --target-dir=$topdir/backup/full

mysql -e "delete from test;" incremental_sample

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
