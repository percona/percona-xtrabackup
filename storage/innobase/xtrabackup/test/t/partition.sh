if is_server_version_lower_than 5.6.0
then
    skip_test "Requires server version 5.6"
fi
#
# PXB-2239 - Partitioned table is not restored correctly when partitions are changed during backup
#
start_server
run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (
  id int NOT NULL AUTO_INCREMENT,
  k int NOT NULL DEFAULT '0',
  c char(120) NOT NULL DEFAULT '',
  pad char(60) NOT NULL DEFAULT '',
  PRIMARY KEY (id),
  KEY k_1 (k)
) PARTITION BY KEY() PARTITIONS 5;
EOF

function take_mdl()
{
  sleep=$1
  run_cmd $MYSQL $MYSQL_ARGS test -e "
START TRANSACTION;
SELECT 1 FROM t1;
SELECT SLEEP($sleep);
"
}

function rebuild_partition()
{
  sleep=$1
  run_cmd $MYSQL $MYSQL_ARGS test -e "
SELECT SLEEP($sleep);
ALTER TABLE t1 REBUILD PARTITION p0, p1;
"
}

# test 1 - normal backup

# Some syncronization hack:
# 1) take_mdl 12 will take a MDL lock on t1 and wait for 12 seconds before releaseing it
# 2) rebuild_partition 1 will sleep for 1 seconds and try to alter t1.
#it will wait for #1 to be release before it proceeds
# 3) rebuild_partition 10 will sleep for 10 seconds and try to alter t1.
# 4) xtrabackup will start and try to acquire MDL.
#This will wait for #2 to before it can proceed
# 5) Sleep from step #3 will end and it will try to acquire MDL on t1, at this point
#xtrabackup from step #4 will be first in queue for MDL
take_mdl 12 &
rebuild_partition 1 &
sleep 1.5
rebuild_partition 10 &
xtrabackup --backup --lock-ddl-per-table \
--target-dir=$topdir/backup0

if ! grep -q "Copying ddl_log.log" $OUTFILE
then
    die "Cannot find 'Copying ddl_log.log' message on xtrabackup log"
fi
xtrabackup --prepare --target-dir=$topdir/backup0
stop_server
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup0
start_server
rebuild_partition 0


# test 2 - incremental backup
# we should make sure we start mysql with incremental ddl_log.log
take_mdl 12 &
rebuild_partition 1 &
sleep 1.5
rebuild_partition 10 &
job_id_partition_10=$!
xtrabackup --backup --lock-ddl-per-table \
--target-dir=$topdir/full

log_copy_dll_entries=$(grep "Copying ddl_log.log" results/partition | wc -l)
if [[ "$log_copy_dll_entries" -ne "2" ]];
then
  die "required two entries containing the string 'Copying ddl_log.log'.
  Found ${log_copy_dll_entries}"
fi
wait $job_id_partition_10
rebuild_partition 0

take_mdl 12 &
rebuild_partition 1 &
sleep 1.5
rebuild_partition 10 &
xtrabackup --backup --lock-ddl-per-table --incremental-basedir=$topdir/full \
--target-dir=$topdir/inc1

log_copy_dll_entries=$(grep "Copying ddl_log.log" results/partition | wc -l)
if [[ "$log_copy_dll_entries" -ne "3" ]];
then
  die "required tree entries containing the string 'Copying ddl_log.log'.
  Found ${log_copy_dll_entries}"
fi
inc_ddl_log_md5=$(md5sum $topdir/inc1/ddl_log.log | awk '{print $1}')

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
xtrabackup --prepare --incremental-dir=$topdir/inc1 --target-dir=$topdir/full
current_ddl_log_md5=$(md5sum $topdir/full/ddl_log.log | awk '{print $1}')
if [[ $current_ddl_log_md5 !=  $inc_ddl_log_md5 ]];
then
  die "Current ddl_log.log md5 doesn't match incremental ddl_log.log md5."
fi
stop_server
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full
start_server
rebuild_partition 0

#cleanup
stop_server
rm -rf $mysql_datadir
rm -rf $topdir/{full,inc1,backup0}

