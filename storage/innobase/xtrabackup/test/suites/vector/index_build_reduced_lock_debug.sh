########################################################################
# --lock-ddl=reduced and a vector index built during the backup
#
# ALTER TABLE ... ADD VECTOR KEY stops after it created the aux table of
# the index and before it fills it without redo, until xtrabackup has
# copied the files. The aux table is copied empty, so xtrabackup must copy
# it again under the lock.
########################################################################

. inc/common.sh
. inc/vector_index_rows.sh

require_debug_pxb_version
require_debug_server

MYSQLD_EXTRA_MY_CNF_OPTS="
debug-sync-timeout=600
"

start_server
require_vector_index

mysql test -e "CREATE TABLE t (id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
                               v VECTOR(4) NOT NULL) ENGINE=InnoDB"
insert_rows 200

$MYSQL $MYSQL_ARGS test -e "SET DEBUG_SYNC =
    'alter_table_inplace_after_lock_downgrade SIGNAL aux_created WAIT_FOR fill';
    ALTER TABLE t ADD VECTOR KEY vk (v) TYPE hnsw" &
alter_pid=$!
mysql -e "SET DEBUG_SYNC = 'now WAIT_FOR aux_created'"

xtrabackup --backup --lock-ddl=reduced --target-dir=$topdir/backup \
           --debug-sync="ddl_tracker_before_lock_ddl" &
job_pid=$!
wait_for_xb_to_suspend $topdir/backup/xtrabackup_debug_sync

mysql -e "SET DEBUG_SYNC = 'now SIGNAL fill'"
run_cmd wait $alter_pid
insert_rows 5
snapshot server

resume_suspended_xb $topdir/backup/xtrabackup_debug_sync
run_cmd wait $job_pid

stop_server
rm -rf $mysql_datadir
xtrabackup --prepare --target-dir=$topdir/backup
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

snapshot restored
for f in rows index autoinc
do
  diff $topdir/server.$f $topdir/restored.$f ||
    die "The restored $f is not what the server had"
done
