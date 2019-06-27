########################################################################
# Bug #711166: Partitioned tables are not correctly handled by the
#              --databases and --tables-file options of innobackupex,
#              and by the --tables option of xtrabackup.
#              Testcase covers using --tables option with InnoDB
#              database and --stream mode
########################################################################

. inc/common.sh
. inc/ib_part.sh

start_server --innodb_file_per_table

require_partitioning

# Create InnoDB partitioned table
ib_part_init $topdir InnoDB

# Saving the checksum of original table
checksum_a=`checksum_table test test`

# Take a backup
mkdir -p $topdir/backup
xtrabackup --backup --stream=xbstream --tables='^(mysql.*|performance_schema.*|test.test)$' --target-dir=$topdir/backup > $topdir/backup/backup.xbs
xbstream -xv < $topdir/backup/backup.xbs -C $topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

vlog "Backup taken"

stop_server

# Restore partial backup
ib_part_restore $topdir $mysql_datadir

start_server

ib_part_assert_checksum $checksum_a
