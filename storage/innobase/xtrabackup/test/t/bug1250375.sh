. inc/common.sh

require_galera
require_server_version_higher_than 5.6.0

ADDR=127.0.0.1

if [[ -n ${WSREP_DEBUG:-} ]];then 
    start_server --log-bin=`hostname`-bin --binlog-format=ROW --wsrep-provider=${MYSQL_BASEDIR}/lib/libgalera_smm.so --wsrep_cluster_address=gcomm:// --wsrep-debug=1 --wsrep_provider_options="debug=1" --wsrep_node_address=$ADDR --gtid_mode=ON
else 
    start_server --log-bin=`hostname`-bin --binlog-format=ROW --wsrep-provider=${MYSQL_BASEDIR}/lib/libgalera_smm.so --wsrep_cluster_address=gcomm:// --wsrep_node_address=$ADDR --gtid_mode=ON

fi


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
