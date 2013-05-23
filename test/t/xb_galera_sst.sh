. inc/common.sh

node1=1
# node2 will be getting SST
node2=901
ADDR=127.0.0.1
recv_addr1="${ADDR}:$(get_free_port 2)"
recv_addr2="${ADDR}:$(get_free_port 3)"
listen_addr1="${ADDR}:$(get_free_port 4)"
listen_addr2="${ADDR}:$(get_free_port 5)"
SSTPASS="password"

set +e
${MYSQLD} --basedir=$MYSQL_BASEDIR  --user=$USER --help --verbose --wsrep-sst-method=rsync| grep -q wsrep
probe_result=$?
if [[ "$probe_result" == "0" ]]
    then
        vlog "Server supports wsrep"
    else
        echo "Requires WSREP enabled" > $SKIPPED_REASON
        exit $SKIPPED_EXIT_CODE
fi
set -e

vlog "Starting server $node1"
start_server_with_id $node1  --binlog-format=ROW --wsrep-provider=${MYSQL_BASEDIR}/lib/libgalera_smm.so --wsrep_cluster_address=gcomm:// --wsrep_sst_receive_address=$recv_addr1 --wsrep_node_incoming_address=$ADDR --wsrep_provider_options="gmcast.listen_addr=tcp://$listen_addr1" --wsrep_sst_method=xtrabackup --wsrep_sst_auth=$USER:$SSTPASS

vlog "Setting password to 'password'"
run_cmd ${MYSQL} ${MYSQL_ARGS} <<EOF
 SET PASSWORD FOR 'root'@'localhost' = PASSWORD('password');
EOF

vlog "Starting server $node2"
start_server_with_id $node2 --binlog-format=ROW --wsrep-provider=${MYSQL_BASEDIR}/lib/libgalera_smm.so --wsrep_cluster_address=gcomm://$listen_addr1 --wsrep_sst_receive_address=$recv_addr2 --wsrep_node_incoming_address=$ADDR --wsrep_provider_options="gmcast.listen_addr=tcp://$listen_addr2" --wsrep_sst_auth=$USER:$SSTPASS --wsrep_sst_method=xtrabackup

vlog "Sleeping while SST for 10 seconds"
sleep 10

switch_server $node2

# The password propagates through SST
MYSQL_ARGS="${MYSQL_ARGS} -ppassword"


if [[ "`${MYSQL} ${MYSQL_ARGS} -Ns -e 'SHOW STATUS LIKE "wsrep_local_state_uuid"'|awk {'print $2'}`" == "`sed  -re 's/:.+$//' $MYSQLD_DATADIR/xtrabackup_galera_info`" && "`${MYSQL} ${MYSQL_ARGS} -Ns -e 'SHOW STATUS LIKE "wsrep_last_committed"'|awk {'print $2'}`" == "`sed  -re 's/^.+://' $MYSQLD_DATADIR/xtrabackup_galera_info`" ]]
then
	vlog "SST successful"
else
	vlog "SST failed"
	exit 1
fi

stop_server_with_id $node2
stop_server_with_id $node1


free_reserved_port ${listen_addr1}
free_reserved_port ${listen_addr2}
free_reserved_port ${recv_addr1}
free_reserved_port ${recv_addr2}
