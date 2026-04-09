let $backup_schema = `SELECT DATABASE()`;

--echo ########################################
--echo # This file contains security-related tests for LIBRARY feature
--echo # when 'lower_case_table_names' is set to 1
--echo ########################################

SHOW GLOBAL VARIABLES LIKE 'lower_case_table_names';

CREATE DATABASE UPPER_CASE_DB;

--echo ########################################
--echo # Setup users & permissions
--echo ########################################

CREATE USER 'userCreates'@localhost;
GRANT CREATE ROUTINE ON UPPER_CASE_DB.* TO 'userCreates'@localhost;

--echo ########################################
--echo # Test creating with implicit DB
--echo ########################################

--echo # Switch to userCreates: can only create in UPPER_CASE_DB
connect (c_userCreates, localhost, userCreates, ,);
connection c_userCreates;

USE UPPER_CASE_DB;

--echo # Should succeed, because even if implicitly defined, db should be lower case
CREATE LIBRARY lib_foo2 LANGUAGE JAVASCRIPT
AS $$
  export function foo() {return 1v}
$$;

--echo ########################################
--echo # Cleanup
--echo ########################################

--echo # Switch to default
connection default;

disconnect c_userCreates;

DROP DATABASE UPPER_CASE_DB;

DROP USER 'userCreates'@localhost;
