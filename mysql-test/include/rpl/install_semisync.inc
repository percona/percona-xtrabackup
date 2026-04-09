# ==== Purpose ====
#
# Install and enable the semisync plugins.
#
# ==== Usage ====
#
# --source include/rpl/install_semisync.inc
#

--let $include_filename= rpl/install_semisync.inc
--source include/begin_include_file.inc

--source include/rpl/connection_source.inc
--source include/rpl/install_semisync_source.inc

--source include/rpl/connection_replica.inc
--source include/rpl/install_semisync_replica.inc

--source include/end_include_file.inc
