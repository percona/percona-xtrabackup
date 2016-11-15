. inc/common.sh

require_galera
require_server_version_higher_than 5.6.0

ADDR=127.0.0.1

MYSQLD_EXTRA_MY_CNF_OPTS="
log_bin=`hostname`-bin
binlog_format=ROW
log_slave_updates=ON
enforce_gtid_consistency=ON
gtid_mode=ON
wsrep_provider=$LIBGALERA_PATH
wsrep_cluster_address=gcomm://
wsrep_node_address=$ADDR
"

start_server

# Load some data so we have a non-empty Executed_Gtid_Set
$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1(a INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1), (2), (3);
EOF

innobackupex --no-timestamp --galera-info $topdir/backup 
backup_dir=$topdir/backup
vlog "Backup created in directory $backup_dir"

master_file=`get_binlog_file`

cd $backup_dir

if [ ! -f $master_file ]
then
    vlog "Could not find bin-log file in backup set, expecting to find $master_file"
    vlog "Backup directory contents:"
    ls -l
    exit 1
fi
