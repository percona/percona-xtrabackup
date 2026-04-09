--source include/have_debug.inc

--echo # Bug#37498680 mysqldump --routines doesn't work with servers < 9.2.0

create database mysqldump_test_db;

create procedure mysqldump_test_db.sp1() select 'hello';
--disable_warnings
create library  mysqldump_test_db.sp1 LANGUAGE JAVASCRIPT COMMENT "Library Comment"
AS " export function f(n) {  return n+1 } ";
create function mysqldump_test_db.sp1(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (mysqldump_test_db.sp1)
as " return sp1.f(n) ";
set sql_mode=ansi_quotes;
create library  mysqldump_test_db.library_1 LANGUAGE JAVASCRIPT AS $$ export function f(n) {  return n+1 } $$;
create function mysqldump_test_db.function_1(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (mysqldump_test_db.library_1 as lib, mysqldump_test_db.sp1)
as $$ return lib.f(n) $$;
set sql_mode=DEFAULT;
--enable_warnings
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'mysqldump_test_db'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

--echo # Restart the server without the library-related Information Schema views.
--let $restart_parameters=restart: --debug=+d,dd_register_view_sans_libraries
--source include/restart_mysqld.inc

-- let $version = `SELECT VERSION()`

--echo # Make sure that no library is dumped:
--exec $MYSQL_DUMP --skip-comments --routines --databases mysqldump_test_db > $MYSQLTEST_VARDIR/tmp/mysqldump.sql
--echo # Console dump.
--let $executed_gtid_set = `SELECT @@GLOBAL.GTID_EXECUTED`
--replace_result $version <version> $executed_gtid_set GTID_SET
--exec $MYSQL_DUMP --skip-comments --routines --databases mysqldump_test_db
--echo # Console XML dump.
--let $executed_gtid_set = `SELECT @@GLOBAL.GTID_EXECUTED`
--replace_result $version <version> $executed_gtid_set GTID_SET
--exec $MYSQL_DUMP --skip-comments --routines --databases mysqldump_test_db --xml

--echo # Grep for this error message : WARNING: old server version - Libraries missing. The following dump may be incomplete.
--let $grep_file    = $MYSQLTEST_VARDIR/tmp/mysqldump.sql
--let $grep_pattern = WARNING: old server version - Libraries missing. The following dump may be incomplete.
--let $grep_output  = boolean
--source include/grep_pattern.inc

--echo # Grep for this error message : WARNING: old server version - Libraries missing. The following dump may be incomplete.
--let $grep_file    = $MYSQLTEST_VARDIR/tmp/mysqldump.sql
--let $grep_pattern = It's most probably an old server
--let $grep_output  = boolean
--source include/grep_pattern.inc

--echo # Grep for CREATE LIBRARIES - should be none.
--let $grep_file    = $MYSQLTEST_VARDIR/tmp/mysqldump.sql
--let $grep_pattern = CREATE LIBRARY
--let $grep_output  = boolean
--source include/grep_pattern.inc

--echo # Grep for CREATE FUNCTION LANGUAGE JAVASCRIPT - should be one.
--let $grep_file    = $MYSQLTEST_VARDIR/tmp/mysqldump.sql
--let $grep_pattern = CREATE .* LANGUAGE JAVASCRIPT
--let $grep_output  = boolean
--source include/grep_pattern.inc

--echo # Restart the server without the extra instrumentation.
--let $restart_parameters=restart:
--source include/restart_mysqld.inc

--echo # Cleanup.
--remove_file $MYSQLTEST_VARDIR/tmp/mysqldump.sql
drop database mysqldump_test_db;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'mysqldump_test_db'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;
