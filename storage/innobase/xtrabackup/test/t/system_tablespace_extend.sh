########################################################################
# PXB-3800: xtrabackup crashes with assertion failure in
# fil_tablespace_redo_extend() when the system tablespace is extended
# during backup.
#
# Since MySQL 8.0.37 the server writes MLOG_FILE_EXTEND redo records for
# the system tablespace (space_id 0) too. Backup must be able to parse them.
#
# Variant 1: the system tablespace is extended by loading data into a table
# created in the system tablespace.
########################################################################

. inc/common.sh

require_server_version_higher_than 8.0.36

# Small autoextend increment so that ibdata1 is extended quickly
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_autoextend_increment=4
"

start_server

mysql test <<EOF
CREATE TABLE t1 (id INT PRIMARY KEY AUTO_INCREMENT, c LONGBLOB)
  TABLESPACE innodb_system;
INSERT INTO t1 (c) VALUES (REPEAT('a', 8192));
EOF

ibdata_size_before=$(stat -c %s $mysql_datadir/ibdata1)
vlog "ibdata1 size before backup: $ibdata_size_before"

# Suspend after the redo log copy thread has started, so that the
# MLOG_FILE_EXTEND records for space 0 are generated after the backup
# checkpoint and are parsed by the redo copy thread.
xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="xtrabackup_suspend_at_start" &
job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file

vlog "Extending the system tablespace while redo is being copied"
for i in $(seq 1 6); do
  mysql -e "INSERT INTO t1 (c) SELECT REPEAT('b', 1024 * 1024) FROM
            (SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4) a" test
done

ibdata_size_after=$(stat -c %s $mysql_datadir/ibdata1)
vlog "ibdata1 size after inserts: $ibdata_size_after"

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

verify_db_state test

check_status=$(mysql -Ns -e "CHECK TABLE t1" test | awk '$3 == "status" {print $4}')
if [ "$check_status" != "OK" ]; then
  die "CHECK TABLE t1 returned '$check_status'"
fi
