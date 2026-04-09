--source include/have_component_init_then_register.inc

--echo #
--echo # init before register test
--echo #

--echo # install the component
INSTALL COMPONENT "file://component_test_component_init_then_register";

--echo #
--echo # check the component is actually installed
SELECT COUNT(component_urn) FROM mysql.component WHERE component_urn = 'file://component_test_component_init_then_register';

--echo #
--echo # uninstall the component
UNINSTALL COMPONENT "file://component_test_component_init_then_register";

--echo #
--echo # check the component is actually un-installed
SELECT * from mysql.component;
