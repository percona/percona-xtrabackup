###############################################################################
# Bug #1418584: Do not overwrite xtrabackup_galera_info when using autorecovery
###############################################################################

require_galera

ADDR=127.0.0.1

if [[ -n ${WSREP_DEBUG:-} ]];then
    start_server --log-bin=`hostname`-bin --binlog-format=ROW \
                 --wsrep-provider=$LIBGALERA_PATH \
                 --wsrep_cluster_address=gcomm:// \
                 --wsrep-debug=1 --wsrep_provider_options="debug=1" \
                 --wsrep_node_address=$ADDR
else
    start_server --log-bin=`hostname`-bin --binlog-format=ROW \
                 --wsrep-provider=$LIBGALERA_PATH \
                 --wsrep_cluster_address=gcomm:// --wsrep_node_address=$ADDR
fi

has_backup_locks && skip_test "Requires server without backup locks support"

backup_dir=$topdir/backup

backup_dir=$topdir/backup

innobackupex --no-timestamp --galera-info $backup_dir

test -f $backup_dir/xtrabackup_galera_info ||
  die "xtrabackup_galera_info was not created"

echo "test" > $backup_dir/xtrabackup_galera_info

cp $backup_dir/xtrabackup_galera_info $backup_dir/xtrabackup_galera_info_copy

innobackupex --apply-log $backup_dir

# Test that xtrabackup_galera_info has not been overwritten on --apply-log
diff -u $backup_dir/xtrabackup_galera_info \
     $backup_dir/xtrabackup_galera_info_copy
