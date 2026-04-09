--source include/have_debug.inc

--echo #
--echo # Bug#37162611: mysqld crashes during upgrade when adding an event to sys schema
--echo #

--echo # Running mysql_upgrade with debug flag to induce EVENT statement when
--echo # upgrading sys schema
--let $restart_parameters = restart:--upgrade=FORCE --debug=+d,try_event_in_fix_sys_schema
--let $wait_counter= 10000
--source include/restart_mysqld.inc

# Restore default options
--let $restart_parameters = restart:
--source include/restart_mysqld.inc

