--echo # Bug#37375233 Stored program that imports a non existing library is dumped in mysqldump.

--source include/no_valgrind_without_big.inc

# Binlog is required
--source include/have_log_bin.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--source include/have_innodb_max_16k.inc

CREATE DATABASE mysqldump_test_db;
USE mysqldump_test_db;

CREATE LIBRARY deleted_library LANGUAGE JAVASCRIPT AS " export function f(n) {  return n+1 } ";
CREATE FUNCTION orphaned_function(n INTEGER) RETURNS INTEGER
LANGUAGE JAVASCRIPT USING (deleted_library) AS "return lib.f(42)";
DROP LIBRARY deleted_library;

SHOW CREATE FUNCTION orphaned_function;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

--echo # Try to dump the routines - will produce an error.

--let $executed_gtid_set= `SELECT @@GLOBAL.GTID_EXECUTED`
--replace_result "$executed_gtid_set" GTID_SET

--error 2
--exec $MYSQL_DUMP --skip-comments --routines --databases mysqldump_test_db 2>&1

--echo # Cleanup.
DROP DATABASE mysqldump_test_db;
