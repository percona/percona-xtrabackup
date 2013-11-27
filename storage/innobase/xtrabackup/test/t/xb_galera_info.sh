. inc/common.sh

require_galera

ADDR=127.0.0.1

if [[ -n ${WSREP_DEBUG:-} ]];then 
    start_server --log-bin=`hostname`-bin --binlog-format=ROW --wsrep-provider=${MYSQL_BASEDIR}/lib/libgalera_smm.so --wsrep_cluster_address=gcomm:// --wsrep-debug=1 --wsrep_provider_options="debug=1" --wsrep_node_address=$ADDR
else 
    start_server --log-bin=`hostname`-bin --binlog-format=ROW --wsrep-provider=${MYSQL_BASEDIR}/lib/libgalera_smm.so --wsrep_cluster_address=gcomm:// --wsrep_node_address=$ADDR
fi


innobackupex --no-timestamp --galera-info $topdir/backup 
backup_dir=$topdir/backup
vlog "Backup created in directory $backup_dir"
if [[ "`${MYSQL} ${MYSQL_ARGS} -Ns -e 'SHOW STATUS LIKE "wsrep_local_state_uuid"'|awk {'print $2'}`" == "`sed  -re 's/:.+$//' $topdir/backup/xtrabackup_galera_info`" && "`${MYSQL} ${MYSQL_ARGS} -Ns -e 'SHOW STATUS LIKE "wsrep_last_committed"'|awk {'print $2'}`" == "`sed  -re 's/^.+://' $topdir/backup/xtrabackup_galera_info`" ]]
then
	vlog "File is written correctly"
else
	vlog "File incorrect"
	exit 1
fi

stop_server
