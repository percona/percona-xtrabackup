# This file will only set gtid_mode to ON if its current state is OFF
# for all other cases use rpl/set_gtid_mode.inc.

--let $include_filename= rpl/set_gtid_mode_on.inc
--source include/begin_include_file.inc

--disable_query_log
SET @@GLOBAL.enforce_gtid_consistency= ON;
SET @@GLOBAL.gtid_mode= OFF_PERMISSIVE;
SET @@GLOBAL.gtid_mode= ON_PERMISSIVE;
SET @@GLOBAL.gtid_mode= ON;
--enable_query_log

--let $include_filename= rpl/set_gtid_mode_on.inc
--source include/end_include_file.inc
