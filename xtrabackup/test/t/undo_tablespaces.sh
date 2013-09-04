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

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
innodb_undo_directory=$undo_directory
innodb_undo_tablespaces=4
"

start_server
load_sakila

checksum1=`checksum_table sakila payment`
test -n "$checksum1" || die "Failed to checksum table sakila.payment"

# Start a transaction, modify some data and keep it uncommitted for the backup
# stage. InnoDB avoids using the rollback segment in the system tablespace, if
# separate undo tablespaces are used, so the test would fail if we did not
# handle separate undo tablespaces correctly.
start_uncomitted_transaction &
job_master=$!

innobackupex --no-timestamp $topdir/backup

kill -SIGKILL $job_master
stop_server

rm -rf $MYSQLD_DATADIR/*
rm -rf $undo_directory/*

innobackupex --apply-log $topdir/backup

innobackupex --copy-back $topdir/backup



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
