############################################################################
# Bug #1182995: Add SST testing to PXC test framework.
############################################################################

. inc/common.sh

require_galera

node1=1
# node2 will be getting SST
node2=901
ADDR=127.0.0.1
SSTPASS="password"
SUSER="root"
SMDSUM="9f6f3edb78f9a5957ecaf8f99953b5af"

debug=""
pdebug=""
if [[ -n ${WSREP_DEBUG:-} ]];then 
    debug="--wsrep-debug=1"
    pdebug=";debug=1"
fi

recv_addr1="${ADDR}:$(get_free_port 2)"
recv_addr2="${ADDR}:$(get_free_port 3)"
listen_addr1="${ADDR}:$(get_free_port 4)"
listen_addr2="${ADDR}:$(get_free_port 5)"

vlog "Starting server $node1"
start_server_with_id $node1 --innodb_file_per_table  --binlog-format=ROW \
                     --loose-pxc_strict_mode=DISABLED \
                     --wsrep-provider=$LIBGALERA_PATH \
                     --wsrep_cluster_address=gcomm:// \
                     --wsrep_sst_receive_address=$recv_addr1 \
                     --wsrep_node_incoming_address=$ADDR \
                     --wsrep_provider_options="gmcast.listen_addr=tcp://$listen_addr1${pdebug}" \
                     --wsrep_sst_method=xtrabackup-v2 \
                     --wsrep_sst_auth=$SUSER:$SSTPASS  \
                     --wsrep_node_address=$ADDR $debug

load_sakila

vlog "Setting password to 'password'"
run_cmd ${MYSQL} ${MYSQL_ARGS} <<EOF
 SET PASSWORD FOR 'root'@'localhost' = PASSWORD('password');
EOF

vlog "Starting server $node2"
start_server_with_id $node2 --innodb_file_per_table --binlog-format=ROW \
                     --loose-pxc_strict_mode=DISABLED \
                     --wsrep-provider=$LIBGALERA_PATH \
                     --wsrep_cluster_address=gcomm://$listen_addr1 \
                     --wsrep_sst_receive_address=$recv_addr2 \
                     --wsrep_node_incoming_address=$ADDR \
                     --wsrep_provider_options="gmcast.listen_addr=tcp://$listen_addr2${pdebug}" \
                     --wsrep_sst_method=xtrabackup-v2 \
                     --wsrep_sst_auth=$SUSER:$SSTPASS  \
                     --wsrep_node_address=$ADDR $debug
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

# Lightweight verification till lp:1199656 is fixed
mdsum=$(${MYSQL} ${MYSQL_ARGS} -e 'select * from sakila.actor;' | md5sum | cut -d" " -f1)

if [[ $mdsum != $SMDSUM ]];then 
    vlog "Integrity verification failed: found: $mdsum expected: $SMDSUM"
    exit 1
fi

stop_server_with_id $node2
stop_server_with_id $node1


free_reserved_port ${listen_addr1}
free_reserved_port ${listen_addr2}
free_reserved_port ${recv_addr1}
free_reserved_port ${recv_addr2}
