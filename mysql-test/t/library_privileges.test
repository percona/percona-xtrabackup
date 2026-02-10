let $backup_schema = `SELECT DATABASE()`;

--echo ########################################
--echo # This file contains security-related tests for LIBRARY feature
--echo # with the focus on 2 things:
--echo #   1. the visibility of library's inside information_schema
--echo #     - requires SHOW_ROUTINE or global SELECT, or
--echo #     - CREATE ROUTINE or ALTER ROUTINE or EXECUTE
--echo #   2. the following operations:
--echo #     CREATE LIBRARY       (CREATE ROUTINE)
--echo #     DROP LIBRARY         (ALTER ROUTINE)
--echo #     ALTER LIBRARY        (ALTER ROUTINE)
--echo #     SHOW CREATE LIBRARY  (SHOW_ROUTINE or CREATE ROUTINE or ALTER ROUTINE or global SELECT)
--echo #     SHOW LIBRARY CODE    (SHOW_ROUTINE or CREATE ROUTINE or ALTER ROUTINE or global SELECT) - not supported
--echo #     SHOW LIBRARY STATUS  (SHOW_ROUTINE or CREATE ROUTINE or ALTER ROUTINE or global SELECT)
--echo #
--echo # This test file does NOT test importing of libraries.
--echo #
--echo ########################################

--echo ########################################
--echo # Setup
--echo ########################################

SHOW GLOBAL VARIABLES LIKE 'automatic_sp_privileges';

CREATE DATABASE my_data;
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE DATABASE db_root;

USE db_root;
CREATE LIBRARY lib_root LANGUAGE JAVASCRIPT
AS $$
  export function foo() {
    let r = session.runSql("SELECT * FROM my_data.t")
    return "FOO"
  }
  let r2 = session.runSql("SELECT * FROM my_data.t")
$$;

CREATE LIBRARY lib_root_replacement LANGUAGE JAVASCRIPT
AS $$
  export function foo() {
    return "YOU SHALL NOT SEE THIS ROOT COMMENT !!!"
  }
$$;

CREATE FUNCTION function_root() RETURNS VARCHAR(128) LANGUAGE JAVASCRIPT USING (lib_root) AS
"return lib_root.foo()";

--echo # We need an account less than SYSTEM_USER, because we need to avoid creating objects with SYSTEM_USER,
--echo # because then no user can alter or drop them except SYSTEM_USER
CREATE USER 'myRoot'@localhost;
GRANT SELECT ON *.* TO 'myRoot'@'localhost' WITH GRANT OPTION;
GRANT CREATE ON *.* TO 'myRoot'@'localhost' WITH GRANT OPTION;
GRANT INSERT ON *.* TO 'myRoot'@'localhost' WITH GRANT OPTION;
GRANT EXECUTE ON *.* TO 'myRoot'@'localhost' WITH GRANT OPTION;
GRANT CREATE ROUTINE ON *.* TO 'myRoot'@'localhost' WITH GRANT OPTION;
GRANT ALTER ROUTINE ON *.* TO 'myRoot'@'localhost' WITH GRANT OPTION;
GRANT SHOW_ROUTINE ON *.* TO 'myRoot'@'localhost' WITH GRANT OPTION;
GRANT CREATE USER ON *.* TO 'myRoot'@'localhost' WITH GRANT OPTION;
FLUSH PRIVILEGES;
connect (c_myRoot, localhost, myRoot, ,);
connection c_myRoot;

--echo # Define some tables, no one should have access to them, but it doesn't matter,
--echo # since they wont be used.
USE my_data;
CREATE TABLE t (col_a VARCHAR(2));
INSERT INTO t (col_a) VALUES ('a1');
INSERT INTO t (col_a) VALUES ('a2');

--echo # Define all the libraries from myRoot's account
USE db1;

CREATE LIBRARY lib_foo LANGUAGE JAVASCRIPT
AS $$
  export function foo() {
    let r = session.runSql("SELECT * FROM my_data.t")
    return "FOO"
  }
  let r2 = session.runSql("SELECT * FROM my_data.t")
$$;

CREATE LIBRARY lib_foo_replacement LANGUAGE JAVASCRIPT
AS $$
  export function foo() {
    return "YOU SHALL NOT SEE THIS F00 COMMENT !!!"
  }
$$;

CREATE LIBRARY lib_bar LANGUAGE JAVASCRIPT
AS $$
  export function bar() {
    let r = session.runSql("SELECT * FROM my_data.t")
    return "BAR"
  }
  let r2 = session.runSql("SELECT * FROM my_data.t")
$$;

CREATE LIBRARY lib_uses_both LANGUAGE JAVASCRIPT
AS $$
  import {foo} from "/db1/lib_foo"
  import {bar} from "/db1/lib_bar"

  export function both() {return foo() + "and " + bar()}
$$;

--echo # Different object type, same name. Should not be confused when granting/revoking privileges
CREATE FUNCTION lib_uses_both() RETURNS INT LANGUAGE JAVASCRIPT
AS $$
  return 42;
$$;

--echo # Different object type, same name. Should not be confused when granting/revoking privileges
CREATE PROCEDURE lib_uses_both() LANGUAGE JAVASCRIPT
AS $$
  console.log("Hello world")
$$;

USE db2;

CREATE LIBRARY lib_foo LANGUAGE JAVASCRIPT
AS $$
  export function foo() {
    let r = session.runSql("SELECT * FROM my_data.t")
    return "FOO"
  }
  let r2 = session.runSql("SELECT * FROM my_data.t")
$$;

CREATE LIBRARY lib_bar LANGUAGE JAVASCRIPT
AS $$
  export function bar() {
    let r = session.runSql("SELECT * FROM my_data.t")
    return "BAR"
  }
  let r2 = session.runSql("SELECT * FROM my_data.t")
$$;

CREATE LIBRARY lib_uses_both LANGUAGE JAVASCRIPT
AS $$
  import {foo} from "/db2/lib_foo"
  import {bar} from "/db2/lib_bar"

  export function both() {return foo() + "and " + bar()}
$$;

USE db_root;
CREATE LIBRARY lib_no_root LANGUAGE JAVASCRIPT
AS $$
  export function foo() {
    let r = session.runSql("SELECT * FROM my_data.t")
    return "FOO"
  }
  let r2 = session.runSql("SELECT * FROM my_data.t")
$$;

--eval USE $backup_schema

--echo ########################################
--echo # Setup users & permissions
--echo ########################################

CREATE USER 'userNothing'@localhost;

--echo # Note: for CREATE, we cannot pass the object type
CREATE USER 'userCreatesDb2'@localhost;
GRANT CREATE ROUTINE ON db2.* TO 'userCreatesDb2'@localhost;
CREATE USER 'userCreatesAll'@localhost;
GRANT CREATE ROUTINE ON *.* TO 'userCreatesAll'@localhost;

CREATE USER 'userAltersDb1Both'@localhost;
GRANT ALTER ROUTINE ON LIBRARY db1.lib_uses_both TO 'userAltersDb1Both'@localhost;
--error ER_SP_DOES_NOT_EXIST
GRANT ALTER ROUTINE ON LIBRARY db1.lib_uses_both2222 TO 'userAltersDb1Both'@localhost;
CREATE USER 'userAltersDb2'@localhost;
GRANT ALTER ROUTINE ON db2.* TO 'userAltersDb2'@localhost;
CREATE USER 'userAltersAll'@localhost;
GRANT ALTER ROUTINE ON *.* TO 'userAltersAll'@localhost;

--echo # Note: for SELECT, we cannot scope to a single SP, since select is on tables
CREATE USER 'userSelectsDb2'@localhost;
GRANT SELECT ON db2.* TO 'userSelectsDb2'@localhost;
CREATE USER 'userSelectsAll'@localhost;
GRANT SELECT ON *.* TO 'userSelectsAll'@localhost;

--echo # Note: SHOW_ROUTINE is only global
CREATE USER 'userShowsAll'@localhost;
--error ER_ILLEGAL_PRIVILEGE_LEVEL
GRANT SHOW_ROUTINE ON LIBRARY db1.lib_uses_both TO 'userShowsAll'@localhost;
--error ER_ILLEGAL_PRIVILEGE_LEVEL
GRANT SHOW_ROUTINE ON db2.* TO 'userShowsAll'@localhost;
GRANT SHOW_ROUTINE ON *.* TO 'userShowsAll'@localhost;

CREATE USER 'userExecutesDb1Both'@localhost;
GRANT EXECUTE ON LIBRARY db1.lib_uses_both TO 'userExecutesDb1Both'@localhost;
CREATE USER 'userExecutesDb2'@localhost;
GRANT EXECUTE ON db2.* TO 'userExecutesDb2'@localhost;
CREATE USER 'userExecutesAll'@localhost;
GRANT EXECUTE ON *.* TO 'userExecutesAll'@localhost;

SELECT USER, PRIV, WITH_GRANT_OPTION FROM mysql.global_grants WHERE USER NOT LIKE 'mysql.%' AND USER NOT LIKE 'root';

SELECT user, db, Routine_name, Routine_type, Proc_priv FROM mysql.procs_priv;

SELECT * FROM mysql.db WHERE USER NOT LIKE 'mysql.%' AND USER NOT LIKE 'root';
SELECT User, Select_priv, Insert_priv, Update_priv, Delete_priv, Create_priv, Drop_priv, Reload_priv, Shutdown_priv, Process_priv, File_priv,Grant_priv, References_priv, Index_priv, Alter_priv, Show_db_priv, Super_priv, Create_tmp_table_priv, Lock_tables_priv, Execute_priv, Repl_slave_priv, Repl_client_priv, Create_view_priv, Show_view_priv, Create_routine_priv, Alter_routine_priv, Create_user_priv, Event_priv, Trigger_priv, Create_tablespace_priv FROM mysql.user WHERE USER NOT LIKE 'mysql.%' AND USER NOT LIKE 'root';

--echo ########################################
--echo # Test existence, visibility and SHOW CREATE
--echo ########################################

--echo # Root can see everything
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
SHOW CREATE LIBRARY db1.lib_foo;
SHOW CREATE LIBRARY db2.lib_foo;
--error ER_PARSE_ERROR
SHOW LIBRARY CODE WHERE Db='db1' and Name='lib_foo';
--replace_column 5 <modified> 6 <created>
SHOW LIBRARY STATUS WHERE Db='db1' and Name='lib_foo';

--echo # Switch to userNothing
connect (c_userNothing, localhost, userNothing, ,);
connection c_userNothing;
--echo # No permissions, so expect empty table
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--echo # For reference to show that it is the same error code for both functions and libraries
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE FUNCTION db1.lib_uses_both;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_foo;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_uses_both;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db2.lib_foo;

--echo # Switch to userCreatesDb2: can only see db2
connect (c_userCreatesDb2, localhost, userCreatesDb2, ,);
connection c_userCreatesDb2;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_foo;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_uses_both;
SHOW CREATE LIBRARY db2.lib_foo;

--echo # Switch to userCreatesAll: can also see all
connect (c_userCreatesAll, localhost, userCreatesAll, ,);
connection c_userCreatesAll;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
SHOW CREATE LIBRARY db1.lib_foo;
SHOW CREATE LIBRARY db1.lib_uses_both;
SHOW CREATE LIBRARY db2.lib_foo;

--echo # Switch to userAltersDb1Both: can only see db1.lib_uses_both
connect (c_userAltersDb1Both, localhost, userAltersDb1Both, ,);
connection c_userAltersDb1Both;

--echo # Ensure that it is empty, i.e. no proc nor func should be visible
SELECT ROUTINE_SCHEMA, ROUTINE_NAME FROM information_schema.routines WHERE ROUTINE_SCHEMA = "db1" ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME;

SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_foo;
ALTER LIBRARY db1.lib_uses_both COMMENT "Comment altered by userAltersDb1Both";
SELECT LIBRARY_COMMENT FROM information_schema.libraries WHERE LIBRARY_SCHEMA='db1' AND LIBRARY_NAME='lib_uses_both';
SHOW CREATE LIBRARY db1.lib_uses_both;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE FUNCTION db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER FUNCTION db1.lib_uses_both COMMENT "Comment altered by userAltersDb1Both - DISALLOWED";
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE PROCEDURE db1.lib_uses_both;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db2.lib_uses_both;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db2.lib_foo;

--echo # Switch to userAltersDb2: can only see db2
connect (c_userAltersDb2, localhost, userAltersDb2, ,);
connection c_userAltersDb2;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_foo;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_uses_both;
ALTER LIBRARY db2.lib_foo COMMENT "Comment altered by userAltersDb2";
SHOW CREATE LIBRARY db2.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT "Comment altered by userAltersDb2 - DISALLOWED";
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER FUNCTION db1.lib_uses_both COMMENT "Comment altered by userAltersDb2 - DISALLOWED";
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE PROCEDURE db1.lib_uses_both;

--echo # Switch to userAltersAll
connect (c_userAltersAll, localhost, userAltersAll, ,);
connection c_userAltersAll;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
SHOW CREATE LIBRARY db1.lib_foo;
SHOW CREATE LIBRARY db1.lib_uses_both;
SHOW CREATE LIBRARY db2.lib_foo;
ALTER LIBRARY db1.lib_uses_both COMMENT "Comment altered by userAltersAll";
SELECT LIBRARY_COMMENT FROM information_schema.libraries WHERE LIBRARY_SCHEMA='db2' AND LIBRARY_NAME='lib_foo';
SHOW CREATE LIBRARY db1.lib_uses_both;
ALTER PROCEDURE db1.lib_uses_both COMMENT "Comment altered by userAltersAll";
--echo # Even if userAltersAll has the permissions to alter the procedure and read the library, they do not have execute access to the library.
--error ER_SP_DOES_NOT_EXIST
ALTER PROCEDURE db1.lib_uses_both USING (db2.lib_foo);
--echo # Even if userAltersAll has the permissions to alter the procedure, they do not have execute access to the library.
--error ER_SP_DOES_NOT_EXIST
ALTER PROCEDURE db1.lib_uses_both USING (db1.lib_foo);
SHOW CREATE PROCEDURE db1.lib_uses_both;
SELECT ROUTINE_SCHEMA, ROUTINE_NAME, LIBRARY_SCHEMA, LIBRARY_NAME FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES WHERE ROUTINE_SCHEMA='db1' AND ROUTINE_NAME='lib_uses_both';
ALTER LIBRARY db2.lib_foo COMMENT "Comment altered by userAltersAll";
SHOW CREATE LIBRARY db2.lib_foo;
SELECT LIBRARY_COMMENT FROM information_schema.libraries WHERE LIBRARY_SCHEMA='db2' AND LIBRARY_NAME='lib_foo';

--echo # Switch to userSelectsDb2
connect (c_userSelectsDb2, localhost, userSelectsDb2, ,);
connection c_userSelectsDb2;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT "Comment altered by userSelectsDb2 - DISALLOWED";
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_foo;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_uses_both;
--echo # Global select is necessary, not only the schema-level select
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db2.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT "Comment altered by userSelectsDb2 - DISALLOWED";

--echo # Switch to userSelectsAll
connect (c_userSelectsAll, localhost, userSelectsAll, ,);
connection c_userSelectsAll;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
SHOW CREATE LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT "Comment altered by userSelectsAll - DISALLOWED";
SHOW CREATE LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER FUNCTION db1.lib_uses_both COMMENT "Comment altered by userSelectsAll - DISALLOWED";
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT "Comment altered by userSelectsAll - DISALLOWED";
SHOW CREATE LIBRARY db2.lib_foo;

--echo # Switch to userShowsAll
connect (c_userShowsAll, localhost, userShowsAll, ,);
connection c_userShowsAll;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
SHOW CREATE LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT "Comment altered by userShowsAll - DISALLOWED";
SHOW CREATE LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT "Comment altered by userShowsAll - DISALLOWED";
SHOW CREATE LIBRARY db2.lib_foo;

--echo # Switch to userExecutesDb1Both
connect (c_userExecutesDb1Both, localhost, userExecutesDb1Both, ,);
connection c_userExecutesDb1Both;

--echo # Ensure that it is empty, i.e. no proc nor func should be visible
SELECT ROUTINE_SCHEMA, ROUTINE_NAME FROM information_schema.routines WHERE ROUTINE_SCHEMA = "db1" ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME;

SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT "Comment altered by userExecutesDb1Both - DISALLOWED";
SHOW CREATE LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER FUNCTION db1.lib_uses_both COMMENT "Comment altered by userExecutesDb1Both - DISALLOWED";
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE FUNCTION db1.lib_uses_both;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE PROCEDURE db1.lib_uses_both;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db2.lib_uses_both;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db2.lib_foo;

--echo # Switch to userExecutesDb2
connect (c_userExecutesDb2, localhost, userExecutesDb2, ,);
connection c_userExecutesDb2;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_foo;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_uses_both COMMENT "Comment altered by userExecutesDb2 - DISALLOWED";
SHOW CREATE LIBRARY db2.lib_uses_both;
SHOW CREATE LIBRARY db2.lib_foo;

--echo # Switch to userExecutesAll
connect (c_userExecutesAll, localhost, userExecutesAll, ,);
connection c_userExecutesAll;
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT "Comment altered by userExecutesAll - DISALLOWED";
SHOW CREATE LIBRARY db1.lib_foo;
SHOW CREATE LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT "Comment altered by userExecutesAll - DISALLOWED";
SHOW CREATE LIBRARY db2.lib_foo;

--echo ########################################
--echo # Test CREATE LIBRARY
--echo ########################################

--echo # Root can create everything
connection c_myRoot;
CREATE LIBRARY db1.l_root LANGUAGE JAVASCRIPT AS 'var x = 1';
CREATE LIBRARY db2.l_root LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userNothing
connection c_userNothing;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--echo # Verify that error code is the same for functions
--error ER_DBACCESS_DENIED_ERROR
CREATE FUNCTION db2.l_fail() RETURNS INT LANGUAGE JAVASCRIPT AS 'return 42';

--echo # Switch to userCreatesDb2
connection c_userCreatesDb2;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
CREATE LIBRARY db2.l_userCreatesDb2 LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userCreatesAll
connection c_userCreatesAll;
CREATE LIBRARY db1.l_userCreatesAll LANGUAGE JAVASCRIPT AS 'var x = 1';
CREATE LIBRARY db2.l_userCreatesAll LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userAltersDb1Both
connection c_userAltersDb1Both;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userAltersDb2
connection c_userAltersDb2;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--echo # Verify that error code is the same for functions
--error ER_DBACCESS_DENIED_ERROR
CREATE FUNCTION db2.l_fail() RETURNS INT LANGUAGE JAVASCRIPT AS 'return 42';

--echo # Switch to userAltersAll
connection c_userAltersAll;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userSelectsDb2
connection c_userSelectsDb2;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userSelectsAll
connection c_userSelectsAll;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userShowsAll
connection c_userShowsAll;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userExecutesDb1Both
connection c_userExecutesDb1Both;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userExecutesDb2
connection c_userExecutesDb2;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to userExecutesAll
connection c_userExecutesAll;
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db1.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';
--error ER_DBACCESS_DENIED_ERROR
CREATE LIBRARY db2.l_fail LANGUAGE JAVASCRIPT AS 'var x = 1';

--echo # Switch to myRoot
connection c_myRoot;
--echo # Ensure that newly created LIBRARIES got ALTER and EXECUTE privileges for its creators
--echo # These are db2.l_userCreatesDb2, db1.l_userCreatesAll and db2.l_userCreatesAll
SELECT user, db, Routine_name, Routine_type, Proc_priv FROM mysql.procs_priv WHERE Routine_name LIKE 'l_userCreates%';

--echo ########################################
--echo # Test ALTER LIBRARY
--echo ########################################

--echo # Root can alter everything
ALTER LIBRARY db1.l_root COMMENT 'Test Alter Library Comment by root';
ALTER LIBRARY db2.l_root COMMENT 'Test Alter Library Comment by root';
--echo # Confirm the allowed changes are in place.
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM INFORMATION_SCHEMA.LIBRARIES ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # myRoot can not alter the root's functions and libraries.
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER LIBRARY db_root.lib_root COMMENT 'Test Alter Library Comment by myRoot - DISALLOWED';
SHOW CREATE LIBRARY db_root.lib_root;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER FUNCTION db_root.function_root USING (db_root.lib_root_replacement as lib_root);
SHOW CREATE FUNCTION db_root.function_root;

--echo # Switch to userNothing
connection c_userNothing;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';
--echo # Verify that error code is the same for functions
--error ER_PROCACCESS_DENIED_ERROR
ALTER FUNCTION db1.lib_uses_both COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to userCreatesDb2
connection c_userCreatesDb2;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';
--echo # NOTE: but user can alter his own libraries because automatic-sp-privileges = ON
ALTER LIBRARY db2.l_userCreatesDb2 COMMENT 'Test Alter Library Comment by userCreatesDb2';

--echo # Switch to userCreatesAll
connection c_userCreatesAll;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';
--echo # NOTE: but user can alter his own libraries because automatic-sp-privileges = ON
ALTER LIBRARY db1.l_userCreatesAll COMMENT 'Test Alter Library Comment by l_userCreatesAll';
ALTER LIBRARY db2.l_userCreatesAll COMMENT 'Test Alter Library Comment by l_userCreatesAll';
--echo # Confirm only the allowed changes are in place.
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM INFORMATION_SCHEMA.LIBRARIES ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to userSelectsDb2
connection c_userSelectsDb2;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to userSelectsAll
connection c_userSelectsAll;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to userShowsAll
connection c_userShowsAll;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to userExecutesDb1Both
connection c_userExecutesDb1Both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER FUNCTION db1.lib_uses_both COMMENT 'Test Alter Function Comment';
--error ER_PROCACCESS_DENIED_ERROR
ALTER FUNCTION db1.lib_uses_both COMMENT 'Test Alter Function Comment';

--echo # Switch to userExecutesDb2
connection c_userExecutesDb2;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to userExecutesAll
connection c_userExecutesAll;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_uses_both COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to userAltersDb1Both
connection c_userAltersDb1Both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
ALTER LIBRARY db1.lib_uses_both COMMENT 'Test Alter Library Comment lib_uses_both';
--error ER_PROCACCESS_DENIED_ERROR
DROP FUNCTION db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP FUNCTION db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_uses_both COMMENT 'Disallowed Library Comment!!!';
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db2.lib_foo COMMENT 'Disallowed Library Comment!!!';
--echo # Confirm only the allowed changes are in place.
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM INFORMATION_SCHEMA.LIBRARIES ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to userAltersDb2
connection c_userAltersDb2;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db1.lib_foo COMMENT 'Disallowed Library Comment!!!';
ALTER LIBRARY db2.lib_foo COMMENT 'Test Alter Library Comment db2';
--echo # Confirm only the allowed changes are in place.
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM INFORMATION_SCHEMA.LIBRARIES ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to userAltersAll
connection c_userAltersAll;
ALTER LIBRARY db1.lib_foo COMMENT 'Test Alter Library Comment by UserAltersAll';
ALTER LIBRARY db2.lib_bar COMMENT 'Test Alter Library Comment by UserAltersAll';
--echo # Confirm only the allowed changes are in place.
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM INFORMATION_SCHEMA.LIBRARIES ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to myRoot
connection c_myRoot;
--echo # Ensure that the newly created LIBRARIES should be gone. Only userCreatesAll's and userCreatesDb2's libraries should remain.
SELECT user, db, Routine_name, Routine_type, Proc_priv FROM mysql.procs_priv WHERE Routine_name LIKE 'l_userCreates%';

--echo # Special case: ensure that nobody can alter db_root nor lib_root except the original root

USE db_root;
--echo # Switch to userAltersDb1Both
connection c_userAltersDb1Both;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db_root.lib_root COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to userAltersDb2
connection c_userAltersDb2;
--error ER_PROCACCESS_DENIED_ERROR
ALTER LIBRARY db_root.lib_root COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to userAltersAll
connection c_userAltersAll;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER LIBRARY db_root.lib_root COMMENT 'Disallowed Library Comment!!!';

--echo # Switch to myRoot
connection c_myRoot;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER LIBRARY db_root.lib_root COMMENT 'Disallowed Library Comment!!!';
ALTER LIBRARY db_root.lib_no_root COMMENT 'Test Alter Library Comment lib_no_root';
--echo # Confirm only the allowed changes are in place.
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM INFORMATION_SCHEMA.LIBRARIES ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo ########################################
--echo # Test DROP LIBRARY
--echo ########################################

--echo # Root can drop everything
DROP LIBRARY db1.l_root;
DROP LIBRARY db2.l_root;

--echo # Switch to userNothing
connection c_userNothing;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;
--echo # Verify that error code is the same for functions
--error ER_PROCACCESS_DENIED_ERROR
DROP FUNCTION db1.lib_uses_both;

--echo # Switch to userCreatesDb2
connection c_userCreatesDb2;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;
--echo # NOTE: but user can delete his own libraries because automatic-sp-privileges = ON
DROP LIBRARY db2.l_userCreatesDb2;

--echo # Switch to userCreatesAll
connection c_userCreatesAll;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;
--echo # NOTE: but user can delete his own libraries because automatic-sp-privileges = ON
DROP LIBRARY db1.l_userCreatesAll;
DROP LIBRARY db2.l_userCreatesAll;

--echo # Switch to userSelectsDb2
connection c_userSelectsDb2;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;

--echo # Switch to userSelectsAll
connection c_userSelectsAll;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;

--echo # Switch to userShowsAll
connection c_userShowsAll;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;

--echo # Switch to userExecutesDb1Both
connection c_userExecutesDb1Both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP FUNCTION db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP FUNCTION db1.lib_uses_both;

--echo # Switch to userExecutesDb2
connection c_userExecutesDb2;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;

--echo # Switch to userExecutesAll
connection c_userExecutesAll;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;

--echo # Switch to userAltersDb1Both
connection c_userAltersDb1Both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
DROP LIBRARY db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP FUNCTION db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP FUNCTION db1.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_uses_both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.lib_foo;

--echo # Switch to userAltersDb2
connection c_userAltersDb2;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.lib_foo;
DROP LIBRARY db2.lib_foo;

--echo # Switch to userAltersAll
connection c_userAltersAll;
DROP LIBRARY db1.lib_foo;
DROP LIBRARY db2.lib_bar;

--echo # Switch to myRoot
connection c_myRoot;
--echo # Ensure that this is EMPTY. Newly created LIBRARIES should be gone
SELECT user, db, Routine_name, Routine_type, Proc_priv FROM mysql.procs_priv WHERE Routine_name LIKE 'l_userCreates%';

--echo # Special case: ensure that nobody can drop db_root nor lib_root except the original root

USE db_root;
--echo # Switch to userAltersDb1Both
connection c_userAltersDb1Both;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db_root.lib_root;
--error ER_DBACCESS_DENIED_ERROR
DROP DATABASE db_root;

--echo # Switch to userAltersDb2
connection c_userAltersDb2;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db_root.lib_root;
--error ER_DBACCESS_DENIED_ERROR
DROP DATABASE db_root;

--echo # Switch to userAltersAll
connection c_userAltersAll;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
DROP LIBRARY db_root.lib_root;
--error ER_DBACCESS_DENIED_ERROR
DROP DATABASE db_root;

--echo # Switch to myRoot
connection c_myRoot;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
DROP LIBRARY db_root.lib_root;
DROP LIBRARY db_root.lib_no_root;
--error ER_DBACCESS_DENIED_ERROR
DROP DATABASE db_root;

--echo ########################################
--echo # Test automatic_sp_privileges = OFF
--echo ########################################

--echo # Switch to default so we can set the global variable
connection default;

SET GLOBAL automatic_sp_privileges = 'OFF';

--echo # Switch to myRoot to verify the variable
connection c_myRoot;
SHOW GLOBAL VARIABLES LIKE 'automatic_sp_privileges';

--echo # Switch to userCreatesDb2
connection c_userCreatesDb2;
CREATE LIBRARY db2.l_userCreatesDb2Again LANGUAGE JAVASCRIPT AS 'var x = 1';
--echo # Now, that automatic_sp_privileges is OFF, user cannot import his own LIBRARIES.
--error ER_PROCACCESS_DENIED_ERROR
CREATE FUNCTION db2.f_userCreatesDb2Again() RETURNS INT LANGUAGE JAVASCRIPT USING (db2.l_userCreatesDb2Again) AS 'return 42';
--echo # Now, that automatic_sp_privileges is OFF, user cannot delete his own LIBRARY
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.l_userCreatesDb2Again;

--echo # Switch to userCreatesAll
connection c_userCreatesAll;
CREATE LIBRARY db1.l_userCreatesAllAgain LANGUAGE JAVASCRIPT AS 'var x = 1';
CREATE LIBRARY db2.l_userCreatesAllAgain LANGUAGE JAVASCRIPT AS 'var x = 1';
--echo # Now, that automatic_sp_privileges is OFF, user cannot import his own LIBRARIES.
--error ER_PROCACCESS_DENIED_ERROR
CREATE FUNCTION db1.f_userCreatesAllAgain() RETURNS INT LANGUAGE JAVASCRIPT USING (db1.l_userCreatesAllAgain) AS 'return 42';
--echo # Now, that automatic_sp_privileges is OFF, user cannot delete his own LIBRARIES
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db1.l_userCreatesAllAgain;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db2.l_userCreatesAllAgain;

--echo ########################################
--echo # Test revoke
--echo ########################################

--echo # Switch to myRoot
connection c_myRoot;

CREATE DATABASE db3;
USE db3;

CREATE LIBRARY lib_foo LANGUAGE JAVASCRIPT
AS $$
  export function foo() {return 42}
$$;

CREATE LIBRARY lib_foo_replacement LANGUAGE JAVASCRIPT
AS $$
  export function foo() {return 41}
$$;

CREATE LIBRARY lib_bar LANGUAGE JAVASCRIPT
AS $$
  export function bar() {return 43}
$$;

CREATE FUNCTION foo() RETURNS INT LANGUAGE JAVASCRIPT USING (lib_foo) AS 'return lib_foo.foo()';

CREATE USER 'userExecutesDb3foo'@localhost;
GRANT EXECUTE ON FUNCTION db3.foo TO 'userExecutesDb3foo'@localhost;
GRANT EXECUTE ON LIBRARY db3.lib_foo TO 'userExecutesDb3foo'@localhost;
GRANT EXECUTE ON LIBRARY db3.lib_foo_replacement TO 'userExecutesDb3foo'@localhost;
GRANT EXECUTE ON LIBRARY db3.lib_bar TO 'userExecutesDb3foo'@localhost;

CREATE USER 'userAltersDb3foo'@localhost;
GRANT ALTER ROUTINE ON FUNCTION db3.foo TO 'userAltersDb3foo'@localhost;
GRANT ALTER ROUTINE ON LIBRARY db3.lib_foo TO 'userAltersDb3foo'@localhost;
GRANT ALTER ROUTINE ON LIBRARY db3.lib_bar TO 'userAltersDb3foo'@localhost;

--echo # Switch to userExecutesDb3foo
connect (c_userExecutesDb3foo, localhost, userExecutesDb3foo, ,);
connection c_userExecutesDb3foo;
--echo # Should see both libs in db3
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Even if the user has EXECUTE permissions, they lack the ALTER one.
--error ER_PROCACCESS_DENIED_ERROR
ALTER FUNCTION db3.foo USING (db3.lib_foo_replacement);
SELECT ROUTINE_NAME, LIBRARY_NAME FROM information_schema.routine_libraries WHERE ROUTINE_SCHEMA = 'db3' ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to myRoot
connection c_myRoot;
REVOKE EXECUTE ON LIBRARY db3.lib_foo FROM 'userExecutesDb3foo'@localhost;

--echo # Switch to userExecutesDb3foo again
connection c_userExecutesDb3foo;
--echo # Should see only db3.lib_bar
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to myRoot
connection c_myRoot;
GRANT ALTER ROUTINE ON FUNCTION db3.foo TO 'userExecutesDb3foo'@localhost;

--echo # Switch to userExecutesDb3foo again
connection c_userExecutesDb3foo;
ALTER FUNCTION db3.foo USING (db3.lib_foo_replacement);
SELECT ROUTINE_NAME, LIBRARY_NAME FROM information_schema.routine_libraries WHERE ROUTINE_SCHEMA = 'db3' ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--echo # The permission to import that library was revoked.
--error ER_SP_DOES_NOT_EXIST
ALTER FUNCTION db3.foo USING (db3.lib_foo);
SELECT ROUTINE_NAME, LIBRARY_NAME FROM information_schema.routine_libraries WHERE ROUTINE_SCHEMA = 'db3' ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to myRoot
connection c_myRoot;
ALTER FUNCTION db3.foo USING (db3.lib_foo);
SELECT ROUTINE_NAME, LIBRARY_NAME FROM information_schema.routine_libraries WHERE ROUTINE_SCHEMA = 'db3' ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to userAltersDb3foo
connect (c_userAltersDb3foo, localhost, userAltersDb3foo, ,);
connection c_userAltersDb3foo;
--echo # Should see both libs in db3
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
DROP LIBRARY db3.lib_foo;
--echo # The user lacks the permissions to import the library.
--error ER_SP_DOES_NOT_EXIST
ALTER FUNCTION db3.foo USING (db3.lib_foo_replacement);
SELECT ROUTINE_NAME, LIBRARY_NAME FROM information_schema.routine_libraries WHERE ROUTINE_SCHEMA = 'db3' ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;

--echo # Switch to myRoot
connection c_myRoot;
REVOKE ALTER ROUTINE ON LIBRARY db3.lib_bar FROM 'userAltersDb3foo'@localhost;

--echo # Switch to userAltersDb3foo
connection c_userAltersDb3foo;
--echo # Should be empty because lib_foo was dropped and privileges to lib_bar were revoked
SELECT LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_COMMENT FROM information_schema.libraries ORDER BY LIBRARY_SCHEMA, LIBRARY_NAME;
--error ER_PROCACCESS_DENIED_ERROR
DROP LIBRARY db3.lib_bar;

--echo ########################################
--echo # Cleanup
--echo ########################################

--echo # Switch to default
connection default;

--echo # The original configuration should not be modified.
SELECT * FROM information_schema.routine_libraries ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, LIBRARY_SCHEMA, LIBRARY_NAME;

SET GLOBAL automatic_sp_privileges = 'ON';

disconnect c_userNothing;
disconnect c_userCreatesDb2;
disconnect c_userCreatesAll;
disconnect c_userSelectsDb2;
disconnect c_userSelectsAll;
disconnect c_userShowsAll;
disconnect c_userExecutesDb1Both;
disconnect c_userExecutesDb2;
disconnect c_userExecutesAll;
disconnect c_userAltersDb1Both;
disconnect c_userAltersDb2;
disconnect c_userAltersAll;
disconnect c_userExecutesDb3foo;
disconnect c_userAltersDb3foo;
disconnect c_myRoot;

DROP DATABASE db1;
DROP DATABASE db2;
DROP DATABASE db3;
DROP DATABASE my_data;
DROP DATABASE db_root;

DROP USER 'myRoot'@localhost;
DROP USER 'userNothing'@localhost;
DROP USER 'userCreatesDb2'@localhost;
DROP USER 'userCreatesAll'@localhost;
DROP USER 'userAltersDb1Both'@localhost;
DROP USER 'userAltersDb2'@localhost;
DROP USER 'userAltersAll'@localhost;
DROP USER 'userSelectsDb2'@localhost;
DROP USER 'userSelectsAll'@localhost;
DROP USER 'userShowsAll'@localhost;
DROP USER 'userExecutesDb1Both'@localhost;
DROP USER 'userExecutesDb2'@localhost;
DROP USER 'userExecutesAll'@localhost;
DROP USER 'userExecutesDb3foo'@localhost;
DROP USER 'userAltersDb3foo'@localhost;
