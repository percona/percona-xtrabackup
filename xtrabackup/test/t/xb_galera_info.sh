. inc/common.sh

ADDR=127.0.0.1
set +e
${MYSQLD} --basedir=$MYSQL_BASEDIR --user=$USER --help --verbose --wsrep-sst-method=rsync| grep -q wsrep
probe_result=$?
if [[ "$probe_result" == "0" ]]
    then
        vlog "Server supports wsrep"
    else
        skip_test "Requires WSREP enabled"
fi
set -e

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
