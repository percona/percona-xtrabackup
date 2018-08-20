############################################################################
# Bug #1248065: innodb_checksum_algorithm should be stored in backup-my.cnf
############################################################################

require_server_version_higher_than 5.6.0

function test_with_checksum_algo()
{
    vlog "**************************************"
    vlog "Testing with innodb_checksum_algorithm=$1"
    vlog "**************************************"

    MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_checksum_algorithm=$1
"

    start_server
    load_sakila

    record_db_state sakila

    xtrabackup --backup --target-dir=$topdir/backup

    egrep '^innodb_checksum_algorithm='$1'$' $topdir/backup/backup-my.cnf

    stop_server
    rm -rf $MYSQLD_DATADIR/*

    run_cmd ${XB_BIN} \
        ${XB_ARGS/\/--defaults-file=*my.cnf/$topdir\/backup\/backup-my.cnf} \
        --prepare --target-dir=$topdir/backup

    xtrabackup --copy-back --target-dir=$topdir/backup

    rm -rf $topdir/backup

    start_server

    verify_db_state sakila

    stop_server

    rm -rf $MYSQLD_VARDIR
}

# Test with strict_* values to force errors on algorithm mismatch

test_with_checksum_algo strict_none
test_with_checksum_algo strict_crc32
test_with_checksum_algo strict_innodb
