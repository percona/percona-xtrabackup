
--source include/no_valgrind_without_big.inc
# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

# Binlog is required
--source include/have_log_bin.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--source include/have_innodb_max_16k.inc

--echo #
--echo # WL#16358 Support for Libraries
--echo #

create database mysqldump_test_db;

create procedure mysqldump_test_db.sp1() select 'hello';
--disable_warnings
create library  mysqldump_test_db.sp1 LANGUAGE JAVASCRIPT COMMENT "Library Comment"
AS " export function f(n) {  return n+1 } ";
create function mysqldump_test_db.sp1(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (mysqldump_test_db.sp1)
as " return sp1.f(n) ";
set sql_mode=ansi_quotes;
create library  mysqldump_test_db.`ansi"_"quoted"` LANGUAGE JAVASCRIPT AS $$ export function f(n) {  return n+1 } $$;
create library  mysqldump_test_db.binary_library_hex LANGUAGE WASM
COMMENT 'A binary library stored in binary encoding'
AS X'7072696E7466282754686973206973205741534D27293B';
create function mysqldump_test_db.`ansi"_"quoted"`(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (mysqldump_test_db.`ansi"_"quoted"` as lib, mysqldump_test_db.sp1)
as $$ return lib.f(n) $$;
set sql_mode=DEFAULT;
--enable_warnings
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'mysqldump_test_db'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

--exec $MYSQL_DUMP --skip-comments --routines --databases mysqldump_test_db > $MYSQLTEST_VARDIR/tmp/mysqldump.sql
--echo # Console dump.
--exec $MYSQL_DUMP --skip-comments --routines --databases mysqldump_test_db
--echo # Console XML dump.
--exec $MYSQL_DUMP --skip-comments --routines --databases mysqldump_test_db --xml

--echo # Drop the routines and the libraries and confirm they're gone.
drop procedure mysqldump_test_db.sp1;
drop function mysqldump_test_db.sp1;
drop function mysqldump_test_db.`ansi"_"quoted"`;
drop library mysqldump_test_db.`ansi"_"quoted"`;
drop library mysqldump_test_db.sp1;
drop library mysqldump_test_db.binary_library_hex;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'mysqldump_test_db'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

--echo # Import back the exported routines that were just deleted.
--exec $MYSQL test < $MYSQLTEST_VARDIR/tmp/mysqldump.sql
show create function mysqldump_test_db.sp1;
show create library mysqldump_test_db.sp1;
show create function mysqldump_test_db.`ansi"_"quoted"`;
show create library mysqldump_test_db.`ansi"_"quoted"`;
show create library mysqldump_test_db.binary_library_hex;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'mysqldump_test_db'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;
--remove_file $MYSQLTEST_VARDIR/tmp/mysqldump.sql

drop database mysqldump_test_db;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'mysqldump_test_db'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

--echo # Test with special characters and quote modes.
--echo # NOTE: this SP won't be executable, JavaScript does not support identifiers with these characters.
CREATE LIBRARY `library_with_special_characters_!@#$%^&*(){}[];<>/?'"_end` LANGUAGE JAVASCRIPT
AS $$ export function exported_function(n) {return n+1} $$;
CREATE FUNCTION using_special_characters_function(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (`library_with_special_characters_!@#$%^&*(){}[];<>/?'"_end`) AS $$ return 42 $$;
SHOW CREATE FUNCTION using_special_characters_function;
SHOW CREATE LIBRARY `library_with_special_characters_!@#$%^&*(){}[];<>/?'"_end`;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;
--echo # Test ANSI_QUOTES sql mode
set sql_mode=ansi_quotes;
SHOW CREATE FUNCTION using_special_characters_function;
SHOW CREATE LIBRARY `library_with_special_characters_!@#$%^&*(){}[];<>/?'"_end`;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;
CREATE LIBRARY "library_in_ansi_quotes_!@#$%^&*(){}[];<>/?'`_end" LANGUAGE JAVASCRIPT
AS $$ export function exported_function(n) {return n+1} $$;
CREATE FUNCTION function_in_ansi_mode(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING ("library_in_ansi_quotes_!@#$%^&*(){}[];<>/?'`_end") AS $$ return 42 $$;
SHOW CREATE FUNCTION function_in_ansi_mode;
CREATE FUNCTION function_using_both(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING ("library_in_ansi_quotes_!@#$%^&*(){}[];<>/?'`_end", `library_with_special_characters_!@#$%^&*(){}[];<>/?'"_end`) AS $$ return 42 $$;
SHOW CREATE FUNCTION function_using_both;
SHOW CREATE LIBRARY "library_in_ansi_quotes_!@#$%^&*(){}[];<>/?'`_end";
--echo # Test NORMAL_QUOTES sql mode
set sql_mode=DEFAULT;
SHOW CREATE FUNCTION using_special_characters_function;
SHOW CREATE FUNCTION function_using_both;
SHOW CREATE LIBRARY `library_with_special_characters_!@#$%^&*(){}[];<>/?'"_end`;
SHOW CREATE FUNCTION function_in_ansi_mode;
SHOW CREATE LIBRARY `library_in_ansi_quotes_!@#$%^&*(){}[];<>/?'``_end`;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

--echo # cleanup
DROP FUNCTION function_using_both;
DROP FUNCTION function_in_ansi_mode;
DROP FUNCTION using_special_characters_function;
DROP LIBRARY `library_with_special_characters_!@#$%^&*(){}[];<>/?'"_end`;
DROP LIBRARY `library_in_ansi_quotes_!@#$%^&*(){}[];<>/?'``_end`;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
