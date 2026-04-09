--source ../include/have_component_test_file_service.inc

INSTALL COMPONENT 'file://component_test_mysql_file_service';

SELECT test_mysql_file_run_test();

UNINSTALL COMPONENT 'file://component_test_mysql_file_service';
