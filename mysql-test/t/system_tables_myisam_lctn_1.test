#
--source include/force_restart.inc
# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

--echo #
--echo # Bug#30248138 - adding a function once mysql.func is converted to myisam
--echo #                leads to crash
--echo #

--echo #-----------------------------------------------------------------------
--echo # Test cases to verify system table's behavior with storage engines
--echo # InnoDB and MyISAM.
--echo #
--echo # Table name comparison is "case insensitive" with lower_case_table_name=1.
--echo # Run "system_tables_storage_engine_tests.inc" tests with upper case
--echo # system table names .
--echo #-----------------------------------------------------------------------
--let uppercase_system_table_names= `SELECT @@global.lower_case_table_names`
--source include/system_tables_storage_engine_tests.inc

# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
