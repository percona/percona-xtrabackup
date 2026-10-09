########################################################################
# PXB-3800: xtrabackup crashes with assertion failure in
# fil_tablespace_redo_extend() when the system tablespace is extended
# during backup.
#
# Since MySQL 8.0.37 the server writes MLOG_FILE_EXTEND redo records for
# the system tablespace (space_id 0) too. Backup must be able to parse them.
#
# Variant 2: the system tablespace is extended by change buffer growth.
# The change buffer B-tree lives in the system tablespace. Random inserts
# into non-unique secondary indexes whose pages are not in the buffer pool
# are buffered, which grows the change buffer and extends ibdata1. The
# buffered changes are merged by the server after restore.
########################################################################

. inc/common.sh

require_server_version_higher_than 8.0.36

# Buffer pool smaller than the secondary indexes, so that their leaf pages
# are not cached and changes to them go to the change buffer. Change
# buffering is enabled explicitly, as its default differs between versions.
# Small autoextend increment so that ibdata1 is extended quickly.
# A debug server inserts slowly enough for the master thread to merge the
# change buffer as fast as it grows, so its background merge is disabled.
# Release servers do not have the option and ignore it.
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_autoextend_increment=4
innodb_change_buffering=all
innodb_buffer_pool_size=64M
innodb_buffer_pool_load_at_startup=OFF
innodb_buffer_pool_dump_at_shutdown=OFF
innodb_adaptive_hash_index=OFF
loose-innodb_disable_background_merge=ON
"

start_server

function ibuf_size()
{
  $MYSQL $MYSQL_ARGS -Ns -e "SHOW ENGINE INNODB STATUS" | \
    grep -o "Ibuf: size [0-9]*" | awk '{print $3}'
}

vlog "Loading data with change buffering disabled"
mysql test <<EOF
SET GLOBAL innodb_change_buffering = none;
CREATE TABLE t2 (
  id INT PRIMARY KEY AUTO_INCREMENT,
  k1 CHAR(96), k2 CHAR(96), k3 CHAR(96),
  KEY(k1), KEY(k2), KEY(k3)
) ENGINE=InnoDB;
SET SESSION cte_max_recursion_depth = 1000000;
INSERT INTO t2 (k1, k2, k3)
  WITH RECURSIVE seq AS (
    SELECT 1 AS n UNION ALL SELECT n + 1 FROM seq WHERE n < 100000)
  SELECT SHA2(RAND(), 384),
         SHA2(RAND(), 384),
         SHA2(RAND(), 384)
  FROM seq;
EOF

# Restart to empty the buffer pool. innodb_change_buffering is back to all
# after restart.
stop_server
start_server

ibdata_size_before=$(stat -c %s $mysql_datadir/ibdata1)
vlog "ibdata1 size before backup: $ibdata_size_before"
vlog "Change buffer size before backup: $(ibuf_size) pages"

# Suspend after the redo log copy thread has started, so that the
# MLOG_FILE_EXTEND records for space 0 are generated after the backup
# checkpoint and are parsed by the redo copy thread.
xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="xtrabackup_suspend_at_start" &
job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file

vlog "Growing the change buffer while redo is being copied"
for i in $(seq 1 20); do
  mysql test <<EOF
SET SESSION cte_max_recursion_depth = 1000000;
INSERT INTO t2 (k1, k2, k3)
  WITH RECURSIVE seq AS (
    SELECT 1 AS n UNION ALL SELECT n + 1 FROM seq WHERE n < 5000)
  SELECT SHA2(RAND(), 384),
         SHA2(RAND(), 384),
         SHA2(RAND(), 384)
  FROM seq;
EOF
  ibdata_size_after=$(stat -c %s $mysql_datadir/ibdata1)
  if [ "$ibdata_size_after" -gt "$ibdata_size_before" ]; then
    break
  fi
done

vlog "ibdata1 size after inserts: $ibdata_size_after"
vlog "Change buffer size after inserts: $(ibuf_size) pages"

if [ "$ibdata_size_after" -le "$ibdata_size_before" ]; then
  die "ibdata1 was not extended, test is not effective"
fi

# Give the redo copy thread time to parse the MLOG_FILE_EXTEND records
sleep 3

resume_suspended_xb $pid_file

run_cmd wait $job_pid

record_db_state test

xtrabackup --prepare --target-dir=$topdir/backup

stop_server
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

# Reading the secondary indexes merges the buffered changes
verify_db_state test

check_status=$(mysql -Ns -e "CHECK TABLE t2" test | awk '$3 == "status" {print $4}')
if [ "$check_status" != "OK" ]; then
  die "CHECK TABLE t2 returned '$check_status'"
fi
