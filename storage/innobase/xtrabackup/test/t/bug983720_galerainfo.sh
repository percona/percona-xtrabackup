############################################################################
# Bug983720: ib_lru_dump and --galera-info fail with --stream=xbstream
############################################################################

. inc/common.sh

require_galera

ADDR=127.0.0.1

debug=""
pdebug=""
if [[ -n ${WSREP_DEBUG:-} ]];then 
    debug="--wsrep-debug=1"
    pdebug=";debug=1"
fi

galera_port=`get_free_port 2`

start_server --log-bin=`hostname`-bin --binlog-format=ROW \
             --wsrep-provider=$LIBGALERA_PATH \
             --wsrep_cluster_address=gcomm:// $debug \
             --wsrep_provider_options="base_port=${galera_port}${pdebug}" \
             --wsrep_node_address=$ADDR

has_backup_locks && skip_test "Requires server without backup locks support"

# take a backup with stream mode
mkdir -p $topdir/backup
innobackupex --galera-info --stream=xbstream $topdir/backup > $topdir/backup/stream.xbs

xbstream -xv -C $topdir/backup < $topdir/backup/stream.xbs
if [ -f $topdir/backup/xtrabackup_galera_info ] ; then
    vlog "galera info has been backed up"
else
    vlog "galera info has not been backed up"
    exit -1
fi

stop_server
free_reserved_port ${galera_port}
