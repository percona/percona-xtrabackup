########################################################################
# Bug #711166: Partitioned tables are not correctly handled by the
#              --databases and --tables-file options of innobackupex,
#              and by the --tables option of xtrabackup.
#              Testcase covers using --tables-file option with InnoDB
#              database
########################################################################

. inc/common.sh
. inc/ib_part.sh
require_server_version_higher_than 8.0.29

start_server --innodb_file_per_table --innodb_directories=$TEST_VAR_ROOT/remote

require_partitioning

# Create InnoDB partitioned table
ib_part_init  $TEST_VAR_ROOT/remote InnoDB

# Saving the checksum of original table
checksum_a=`checksum_table test test`

# Take a backup
# Only backup of test.test table will be taken
cat >$topdir/tables <<EOF
test.test
EOF
ib_part_add_mandatory_tables $mysql_datadir $topdir/tables
xtrabackup --backup --tables-file=$topdir/tables --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

vlog "Backup taken"

stop_server

# Restore partial backup
ib_part_restore $TEST_VAR_ROOT/remote $mysql_datadir

start_server

ib_part_assert_checksum $checksum_a
