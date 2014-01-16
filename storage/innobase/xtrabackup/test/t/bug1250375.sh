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
wsrep_provider=${MYSQL_BASEDIR}/lib/libgalera_smm.so
wsrep_cluster_address=gcomm://
wsrep_node_address=$ADDR
"

if [ -n ${WSREP_DEBUG:-} ]
then
    MYSQLD_EXTRA_MY_CNF_OPTS="$MYSQLD_EXTRA_MY_CNF_OPTS
wsrep_provider_options=\"debug=1\"
"
fi

start_server

innobackupex --no-timestamp --galera-info $topdir/backup 
backup_dir=$topdir/backup
vlog "Backup created in directory $backup_dir"

count=0
master_file=

while read line; do
    if [ $count -eq 1 ] # File:
    then
        master_file=`echo "$line" | sed s/File://`
        break
    fi
    count=$((count+1))
done <<< "`run_cmd $MYSQL $MYSQL_ARGS -Nse 'SHOW MASTER STATUS\G' mysql`"

stop_server

if [ -f "$backup_dir/$master_file" ]
then
    vlog "Could not find bin-log file in backup set, expecting to find $master_file"
    exit 1
fi
