# Result differences depending on FS case sensitivity.
if (!$require_case_insensitive_file_system)
{
  --source include/have_case_sensitive_file_system.inc
}

#------------------------------------------------------------------------------
# i_s_routine_libraries.test
# .test file for MySQL regression suite
# Purpose:  To test the presence, structure, and behavior
#                    of INFORMATION_SCHEMA.ROUTINE_LIBRARIES
# Author:   WL#16360
#------------------------------------------------------------------------------

-- echo ################################################################################
-- echo # Testcase routine_libraries.1: Ensure that the INFORMATION_SCHEMA.ROUTINE_LIBRARIES
-- echo #                   table has the following columns, in the following order:
-- echo #
-- echo #                   ROUTINE_CATALOG,
-- echo #                   ROUTINE_SCHEMA (shows the database, or schema, in which
-- echo #                          the routine that imports the libraries resides),
-- echo #                   ROUTINE_NAME (shows the importing routine's name),
-- echo #                   ROUTINE_TYPE (shows the type of the importing routine -
-- echo #                          either PROCEDURE or FUNCTION),
-- echo #                   LIBRARY_CATALOG,
-- echo #                   LIBRARY_SCHEMA (shows the database, or schema, in which
-- echo #                          the imported library resides),
-- echo #                   LIBRARY_NAME (shows the imported library name -
-- echo #                          may not exist),
-- echo #                   LIBRARY_VERSION (shows the library's version),
-- echo ################################################################################
-- echo # ========== routine_libraries.1 ==========
USE INFORMATION_SCHEMA;
SHOW CREATE VIEW INFORMATION_SCHEMA.ROUTINE_LIBRARIES;

SELECT * FROM information_schema.columns
WHERE table_schema = 'information_schema'
  AND table_name   = 'ROUTINE_LIBRARIES'
ORDER BY ordinal_position;

DESCRIBE INFORMATION_SCHEMA.ROUTINE_LIBRARIES;

-- echo ###############################################################################
-- echo # Testcase routine_libraries.2:  Unsuccessful stored procedure CREATE will not
-- echo #                     populate I_S.ROUTINE_LIBRARIES view
-- echo ###############################################################################
-- echo # ========== routine_libraries.2 ==========
CREATE DATABASE i_s_routine_libraries_test;
USE i_s_routine_libraries_test;

-- echo # Missing closing ')' character at the end of 's char(20) in func declaration
CREATE LIBRARY test_library_2 LANGUAGE JAVASCRIPT AS $$
export function f(n) {  return n } $$;
--error ER_PARSE_ERROR
CREATE FUNCTION test_function_2(n INTEGER RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (test_library_2)
AS $$ return test_library_2.f(n) $$;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'i_s_routine_libraries_test'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

-- echo ###############################################################################
-- echo # Testcase routine_libraries.3:  DROP LIBRARY - Verify DROP of a function
-- echo #                                     removes I_S.ROUTINE_LIBRARIES data for
-- echo #                                     that function / procedure
-- echo ###############################################################################
-- echo # ========== routine_libraries.3 ==========
DROP DATABASE IF EXISTS i_s_routine_libraries_test;

CREATE DATABASE i_s_routine_libraries_test;
USE i_s_routine_libraries_test;

CREATE LIBRARY test_library_3 LANGUAGE JAVASCRIPT AS $$
export function f(n) {  return n } $$;
CREATE FUNCTION test_function_3(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (i_s_routine_libraries_test.test_library_3)
AS $$ return test_library_3.f(n) $$;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'i_s_routine_libraries_test'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;
-- echo # Dropping a library in use won't affect the routines that import it.
DROP LIBRARY test_library_3;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'i_s_routine_libraries_test'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;
DROP FUNCTION test_function_3;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'i_s_routine_libraries_test'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

-- echo ###############################################################################
-- echo # Testcase routine_libraries.4:  ALTER ROUTINE USING
-- echo ###############################################################################
-- echo # ========== routine_libraries.4 ==========
DROP DATABASE IF EXISTS i_s_routine_libraries_test;

CREATE DATABASE i_s_routine_libraries_test;
USE i_s_routine_libraries_test;

CREATE LIBRARY test_library_4 LANGUAGE JAVASCRIPT AS $$
export function f(n) {  return n } $$;
CREATE FUNCTION test_function_4(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (i_s_routine_libraries_test.test_library_4)
AS $$ return test_library_4.f(n) $$;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'i_s_routine_libraries_test'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;
ALTER FUNCTION test_function_4 COMMENT 'new comment added';
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'i_s_routine_libraries_test'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

-- echo ###############################################################################
-- echo # Testcase routine_libraries.5:  Extra long USING clause
-- echo ###############################################################################
-- echo # ========== routine_libraries.5 ==========
DROP DATABASE IF EXISTS i_s_routine_libraries_test;

CREATE DATABASE schema_1_0123456789212345678931234567894123456789512345678961234;
USE schema_1_0123456789212345678931234567894123456789512345678961234;
CREATE LIBRARY library_1_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_2_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_3_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_4_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_5_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_6_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_7_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_8_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_9_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";
CREATE LIBRARY library_0_123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT AS " export function f(n) {  return n } ";

CREATE DATABASE i_s_routine_libraries_test;
USE i_s_routine_libraries_test;

--echo # extra long USING clause
CREATE FUNCTION
function_2123456789212345678931234567894123456789512345678961234()
RETURNS INT LANGUAGE JAVASCRIPT
USING (
  schema_1_0123456789212345678931234567894123456789512345678961234.library_1_123456789212345678931234567894123456789512345678961234 AS alias_1_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_2_123456789212345678931234567894123456789512345678961234 AS alias_2_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_3_123456789212345678931234567894123456789512345678961234 AS alias_3_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_4_123456789212345678931234567894123456789512345678961234 AS alias_4_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_5_123456789212345678931234567894123456789512345678961234 AS alias_5_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_6_123456789212345678931234567894123456789512345678961234 AS alias_6_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_7_123456789212345678931234567894123456789512345678961234 AS alias_7_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_8_123456789212345678931234567894123456789512345678961234 AS alias_8_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_9_123456789212345678931234567894123456789512345678961234 AS alias_9_91123456789212345678931234567894123456789512345678961234,
  schema_1_0123456789212345678931234567894123456789512345678961234.library_0_123456789212345678931234567894123456789512345678961234 AS alias_0_91123456789212345678931234567894123456789512345678961234
) AS $$ return 42 $$;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'i_s_routine_libraries_test'
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;

-- echo # final clean up
DROP DATABASE i_s_routine_libraries_test;
DROP DATABASE schema_1_0123456789212345678931234567894123456789512345678961234;
USE test;

SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES;
--echo # End of INFORMATION_SCHEMA.ROUTINE_LIBRARIES tests
