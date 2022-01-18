#
# PXB-2673 - Rocksdb backup fails if log_status_get is called more than one.
#

require_rocksdb
require_debug_pxb_version

start_server
init_rocksdb

mysql -e "INSTALL COMPONENT \"file://component_mysqlbackup\""
mysql -e "CREATE TABLE t_logstatus (a INT AUTO_INCREMENT PRIMARY KEY) ENGINE=ROCKSDB" test
mysql -e "INSERT INTO t_logstatus (a) VALUES (1), (2), (3)" test

function insert_row()
{
  (while true ; do
    echo "INSERT INTO t_logstatus () VALUES ();"
    sleep 1;
  done) | mysql test
}

insert_row &
INSERT_PID=$!

full_backup_dir=$topdir/full_backup
xtrabackup --backup --page-tracking  --debug=d,page_tracking_checkpoint_behind --target-dir=$full_backup_dir

kill -SIGKILL ${INSERT_PID}
