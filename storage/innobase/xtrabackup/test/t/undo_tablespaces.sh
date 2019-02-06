########################################################################
# Test support for separate UNDO tablespace
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

################################################################################
# Start an uncommitted transaction pause "indefinitely" to keep the connection
# open
################################################################################
function start_uncomitted_transaction()
{
    run_cmd $MYSQL $MYSQL_ARGS sakila <<EOF
START TRANSACTION;
DELETE FROM payment;
SELECT SLEEP(10000);
EOF
}

undo_directory=$TEST_VAR_ROOT/var1/undo_dir
undo_directory_ext=$TEST_VAR_ROOT/var1/undo_dir_ext

mkdir -p $undo_directory
mkdir -p $undo_directory_ext

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
innodb_undo_directory=$undo_directory
innodb_undo_tablespaces=4
innodb_directories=$undo_directory_ext
"

start_server

# create some undo tablespaces
mysql -e "CREATE UNDO TABLESPACE undo1 ADD DATAFILE '$undo_directory_ext/undo1.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo2 ADD DATAFILE '$undo_directory_ext/undo2.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo3 ADD DATAFILE 'undo1.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo4 ADD DATAFILE 'undo2.ibu'"

mysql -e "ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE"
mysql -e "ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE"

load_sakila

ls -al $undo_directory_ext

checksum1=`checksum_table sakila payment`
test -n "$checksum1" || die "Failed to checksum table sakila.payment"

# Start a transaction, modify some data and keep it uncommitted for the backup
# stage. InnoDB avoids using the rollback segment in the system tablespace, if
# separate undo tablespaces are used, so the test would fail if we did not
# handle separate undo tablespaces correctly.
start_uncomitted_transaction &
job_master=$!

xtrabackup --backup --target-dir=$topdir/backup

mysql -e "CREATE UNDO TABLESPACE undo5 ADD DATAFILE '$undo_directory_ext/undo3.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo6 ADD DATAFILE '$undo_directory_ext/undo4.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo7 ADD DATAFILE 'undo3.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo8 ADD DATAFILE 'undo4.ibu'"

xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/backup

kill -SIGKILL $job_master
stop_server

rm -rf $MYSQLD_DATADIR/*
rm -rf $undo_directory/*
rm -rf $undo_directory_ext/*

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup --incremental-dir=$topdir/inc

xtrabackup --copy-back --target-dir=$topdir/backup


for file in $undo_directory_ext/undo1.ibu $undo_directory_ext/undo2.ibu \
        $undo_directory/undo1.ibu $undo_directory/undo2.ibu \
        $undo_directory_ext/undo3.ibu $undo_directory_ext/undo4.ibu \
        $undo_directory/undo3.ibu $undo_directory/undo4.ibu ; do
    if [ ! -f $file ] ; then
        vlog "Tablepace $file is missing!"
        exit -1
    fi
done

start_server

# Check that the uncommitted transaction has been rolled back

checksum2=`checksum_table sakila payment`
test -n "$checksum2" || die "Failed to checksum table sakila.payment"

vlog "Old checksum: $checksum1"
vlog "New checksum: $checksum2"

if [ "$checksum1" != "$checksum2"  ]
then
    vlog "Checksums do not match"
    exit -1
fi
