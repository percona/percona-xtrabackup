########################################################################
# Bug #665210: tar4ibd does not support innodb row_format=compressed
# Bug #810269: tar4ibd does not check for doublewrite buffer pages
########################################################################

. inc/common.sh

if [ -z "$INNODB_VERSION" ]; then
    skip_test "Requires InnoDB plugin or XtraDB"
fi

start_server "--innodb_strict_mode --innodb_file_per_table \
--innodb_file_format=Barracuda"

load_dbase_schema incremental_sample

vlog "Compressing the table"

run_cmd $MYSQL $MYSQL_ARGS -e \
    "ALTER TABLE test ENGINE=InnoDB ROW_FORMAT=compressed \
KEY_BLOCK_SIZE=4" incremental_sample

multi_row_insert incremental_sample.test \({1..10000},10000\)

checksum_a=`checksum_table incremental_sample test`

vlog "Starting streaming backup"

mkdir -p $topdir/backup

innobackupex --stream=tar $topdir/backup > $topdir/backup/out.tar

stop_server
rm -rf $mysql_datadir

vlog "Applying log"

cd $topdir/backup
$TAR -ixvf out.tar
cd -
innobackupex --apply-log $topdir/backup

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex --copy-back $topdir/backup

start_server

checksum_b=`checksum_table incremental_sample test`

if [ "$checksum_a" != "$checksum_b" ]; then
    vlog "Checksums do not match"
    exit -1
fi

vlog "Checksums are OK"
