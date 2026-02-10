--source include/have_component_init_then_register.inc

--echo #
--echo # This tests checks dynamic loader rollback
--echo # for case when component's init() fails.
--echo #

--echo
--echo # load the component with init() which allways fails
--error ER_COMPONENTS_LOAD_CANT_INITIALIZE
INSTALL COMPONENT "file://component_test_component_init_fail";

--echo
--echo # check the component is not installed
SELECT COUNT(component_urn) FROM mysql.component WHERE component_urn='file://component_test_component_init_fail';

