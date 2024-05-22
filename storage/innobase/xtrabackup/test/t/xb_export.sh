. inc/common.sh
table_count=9
NUMBER_OF_CFG_FILES=11
if is_server_version_higher_than 8.0.28
then
  NUMBER_OF_CFG_FILES=15
  table_count=13
fi

vlog "total table to test $table_count "
function init_schema() {
    local database_name=$1
    local table_name=$2
    local columns_count=$3
    local part=$4

    local cmd="create database if not exists $database_name; "
    cmd+="create table $database_name.$table_name ("
    for ((c=1; c<=$columns_count; ++c ))
    do
        cmd+="c$c int,"
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

function create_tables() {
  ### check if we can import all column types in mysql
  mysql -e 'create table test4(
    col1 bool,
    col2 int,
    col3 float,
    col4 double,
    col5 timestamp,
    col6 long,
    col7 date,
    col8 time,
    col9 datetime,
    col10 year,
    col11 varchar(20),
    col12 bit,
    col13 decimal,
    col14 blob,
    col16 json,
    col17 mediumtext,
    col18 enum("01","2"),
    col19 SET("0","1","2"),
    col20 varchar(255) character set latin1
    )' incremental_sample;

  mysql -e 'CREATE TABLE test5(a int, b int, PRIMARY KEY (a), KEY (b DESC))' incremental_sample
  mysql -e 'CREATE TABLE test6(a int)' incremental_sample
  mysql -e 'ALTER TABLE test6 ADD COLUMN v1 VARCHAR(255), ALGORITHM=INSTANT' incremental_sample
  mysql -e 'CREATE TABLE test7(a int) row_format=compressed' incremental_sample
  mysql -e 'CREATE TABLE test8(c1 INT) ENGINE = InnoDB' incremental_sample
  mysql -e 'ALTER TABLE test8 ADD COLUMN c2 INT DEFAULT 500' incremental_sample

  ### If we can insert without duplicate key error after import
  mysql -e 'CREATE TABLE test9(c1 SERIAL) ENGINE = InnoDB' incremental_sample

  if is_server_version_higher_than 8.0.28
  then
    mysql -e 'CREATE TABLE test10(c1 INT, c3  INT DEFAULT 500)  ENGINE = InnoDB' incremental_sample
    mysql -e 'INSERT INTO test10 VALUES (1,1);' incremental_sample
    mysql -e 'ALTER TABLE test10 ADD COLUMN c2 INT DEFAULT 500 AFTER c1, ALGORITHM=INSTANT' incremental_sample
    mysql -e 'CREATE TABLE test11(c1 INT, c3  INT DEFAULT 500)  ENGINE = InnoDB' incremental_sample
    mysql -e 'INSERT INTO test11 VALUES (1,1);' incremental_sample
    mysql -e 'ALTER TABLE test11 ADD COLUMN c2 INT DEFAULT 500 AFTER c1, ADD COLUMN c4 INT,  ALGORITHM=INSTANT' incremental_sample
    mysql -e 'INSERT INTO test11 VALUES (1,1,1,1);' incremental_sample
    mysql -e 'ALTER TABLE test11 DROP COLUMN c3, ALGORITHM=INSTANT' incremental_sample
  fi

  # To verify that import tablespace is successful for tables created using
  # CREATE TABLE LIKE queries
  mysql -e 'CREATE TABLE test12 LIKE test4' incremental_sample

  # To verify that import tablespace is successful for tables with a non
  # default charset
  mysql -e 'CREATE TABLE test13 LIKE test4' incremental_sample
  mysql -e 'ALTER TABLE test13 CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci' incremental_sample
}


mysql_extra_args="--innodb_file_per_table "

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
init_schema incremental_sample test1 $max_indices_count "n"
init_schema incremental_sample test2 $indices_count "n"
init_schema incremental_sample test3 $indices_count "y"

# Adding some data to database
insert_data incremental_sample test1 $max_indices_count $rows_count
insert_data incremental_sample test2 $indices_count $rows_count

create_tables;
mysql -e 'INSERT INTO test9 VALUES(null)' incremental_sample

checksum_1=`checksum_table incremental_sample test1`
rowsnum_1=`${MYSQL} ${MYSQL_ARGS} -Ns -e "select count(*) from test1" incremental_sample`

vlog "rowsnum_1 is $rowsnum_1"
vlog "checksum_1 is $checksum_1"

# Performing table backup
xtrabackup --datadir=$mysql_datadir --backup --target-dir=$backup_dir
vlog "Table was backed up"

vlog "Re-initializing database server"
stop_server
rm -rf ${MYSQLD_DATADIR}
start_server $mysql_extra_args
init_schema incremental_sample test1 $max_indices_count "n"
init_schema incremental_sample test2 $indices_count "n"
init_schema incremental_sample test3 $indices_count "y"

create_tables


vlog "Database was re-initialized"
for ((c=1; c<=$table_count; ++c ))
do
  mysql -e "alter table test$c discard tablespace;" incremental_sample
done

xtrabackup --datadir=$mysql_datadir --prepare --export \
    --target-dir=$backup_dir

ls -tlr $backup_dir/incremental_sample/*cfg
cfg_count1=`find $backup_dir/incremental_sample -name '*.cfg' | wc -l`

# Get the timestamp of the .cfg file created in the first --export
timestamp_1=$(stat -c %Y "$backup_dir/incremental_sample/test1.cfg")

# Run --prepare --export once again to verify that .cfg files are overwritten when it is run for the second time
xtrabackup --datadir=$mysql_datadir --prepare --export \
    --target-dir=$backup_dir

ls -tlr $backup_dir/incremental_sample/*cfg
cfg_count2=`find $backup_dir/incremental_sample -name '*.cfg' | wc -l`

# Get the timestamp of the .cfg file created in the second --export
timestamp_2=$(stat -c %Y "$backup_dir/incremental_sample/test1.cfg")

# Compare the timestamps
if [ "$timestamp_2" -gt "$timestamp_1" ]; then
    vlog "The .cfg generated by PXB in the second attempt has a newer timestamp."
else
    vlog "The .cfg generated by PXB in the second attempt does not have a newer timestamp."
    exit -1
fi

if [ $cfg_count1 != $cfg_count2 ]; then
    vlog "Number of .cfg files created is different in the second attempt"
fi

cfg_count=$cfg_count1

vlog "Verifying .cfg files in backup, expecting ${NUMBER_OF_CFG_FILES}."
if [ $cfg_count -ne ${NUMBER_OF_CFG_FILES} ]
then
   vlog "Expecting ${NUMBER_OF_CFG_FILES} cfg files. Found only $cfg_count"
   exit -1
fi

run_cmd cp $backup_dir/incremental_sample/test* $mysql_datadir/incremental_sample/

for ((c=1; c<=$table_count; ++c ))
do
  mysql -e "alter table test$c import tablespace" incremental_sample
done

vlog "Table has been imported"

mysql -e "INSERT INTO test9 VALUES(null)" incremental_sample

vlog "Cheking checksums"
checksum_2=`checksum_table incremental_sample test1`
vlog "checksum_2 is $checksum_2"
rowsnum_1=`${MYSQL} ${MYSQL_ARGS} -Ns -e "select count(*) from test1" incremental_sample`
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
mysql -e "select count(*) from test1;" incremental_sample
mysql -e "show create table test9;" incremental_sample

# Performing full backup of imported table
mkdir -p $topdir/backup/full

xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup/full

xtrabackup --datadir=$mysql_datadir --prepare --apply-log-only \
    --target-dir=$topdir/backup/full

xtrabackup --datadir=$mysql_datadir --prepare \
    --target-dir=$topdir/backup/full

mysql -e "delete from test1;" incremental_sample

stop_server

rm -rf $mysql_datadir
xtrabackup --datadir=$mysql_datadir --copy-back \
	--target-dir=$topdir/backup/full

start_server $mysql_extra_args

vlog "Cheking checksums"
checksum_3=`checksum_table incremental_sample test1`
vlog "checksum_3 is $checksum_3"

if [ "$checksum_3" != "$checksum_2"  ]
then
    vlog "Checksums are not equal"
    exit -1
fi

vlog "Checksums are OK"

stop_server

rm -rf $backup_dir
