# Setup semisync replica
#
# ==== Usage ====
#
# --source include/rpl/install_semisync_replica.inc

--let $include_filename= rpl/install_semisync_replica.inc
--source include/begin_include_file.inc

--source include/have_semisync_plugin.inc

--disable_query_log

--let $_isr_enabled = _enabled

--let $_isr_replica = replica
--let $_isr_library = $SEMISYNC_REPLICA_PLUGIN

--source include/rpl/stop_receiver.inc

--let $value= query_get_value(show variables like 'rpl_semi_sync_$_isr_replica$enabled', Value, 1)
if ($value == No such row)
{
  eval INSTALL PLUGIN rpl_semi_sync_$_isr_replica SONAME '$_isr_library';
  eval SET GLOBAL rpl_semi_sync_$_isr_replica$_isr_enabled = 1;
}

--source include/rpl/start_receiver.inc

--source include/end_include_file.inc
