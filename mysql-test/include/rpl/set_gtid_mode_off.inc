# This file will only set gtid_mode to OFF if its current state is ON
# for all other cases use rpl/set_gtid_mode.inc.

--let $include_filename= rpl/set_gtid_mode_off.inc
--source include/begin_include_file.inc

--disable_query_log
SET @@GLOBAL.gtid_mode= ON_PERMISSIVE;
SET @@GLOBAL.gtid_mode= OFF_PERMISSIVE;
SET @@GLOBAL.gtid_mode= OFF;
SET @@GLOBAL.enforce_gtid_consistency= OFF;
--enable_query_log

--let $include_filename= rpl/set_gtid_mode_off.inc
--source include/end_include_file.inc
