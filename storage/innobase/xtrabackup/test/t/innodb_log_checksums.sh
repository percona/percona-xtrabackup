########################################################################
# Test for innodb_log_checksums feature in MySQL 5.7
########################################################################

require_server_version_higher_than 5.7.9

function test_with_checksums()
{
    vlog "################################################"
    vlog "Testing with innodb_log_checksums=$1"
    vlog "################################################"

    MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_log_checksums=$1
"
    start_server

    # Workaround for LP bug #1247586
    $MYSQL $MYSQL_ARGS -e "SET GLOBAL innodb_log_checksums=$1"

    load_sakila

    xtrabackup --backup --target-dir=$topdir/backup

    egrep "^innodb_log_checksums=$2" $topdir/backup/backup-my.cnf
    cat $topdir/backup/backup-my.cnf

    record_db_state sakila

    stop_server

    rm -rf $MYSQLD_DATADIR/*

    run_cmd ${XB_BIN} \
        ${XB_ARGS/\/--defaults-file=*my.cnf/$topdir\/backup\/backup-my.cnf} \
        --prepare --target-dir=$topdir/backup

    xtrabackup --copy-back --target-dir=$topdir/backup

    start_server

    verify_db_state sakila

    stop_server

    rm -rf $topdir/backup
    rm -rf $MYSQLD_DATADIR
}

test_with_checksums ON ON
test_with_checksums OFF OFF
