--source include/have_component_deinit_no_deadlock.inc

--echo # setup
INSTALL COMPONENT "file://component_test_component_deinit_no_deadlock";

--echo # test registry synchronized access within component's deinit() has no deadlock problem
UNINSTALL COMPONENT "file://component_test_component_deinit_no_deadlock";
