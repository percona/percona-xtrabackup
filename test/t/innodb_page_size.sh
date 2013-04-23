########################################################################
# Test support for innodb_page_size
########################################################################

. inc/common.sh

if [ ${MYSQL_VERSION:0:3} != "5.6" -a -z "$XTRADB_VERSION" ]
then
    echo "Requires either XtraDB or a 5.6 server" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table=1
innodb_page_size=8192
"
start_server
load_sakila

checksum1=`checksum_table sakila payment`
test -n "$checksum1" || die "Failed to checksum table sakila.payment"

innobackupex --no-timestamp $topdir/backup

stop_server
rm -rf $MYSQLD_DATADIR/*

innobackupex --apply-log $topdir/backup

innobackupex --copy-back $topdir/backup

start_server

checksum2=`checksum_table sakila payment`
test -n "$checksum2" || die "Failed to checksum table sakila.payment"

vlog "Old checksum: $checksum1"
vlog "New checksum: $checksum2"

if [ "$checksum1" != "$checksum2"  ]
then
    die "Checksums do not match"
fi
