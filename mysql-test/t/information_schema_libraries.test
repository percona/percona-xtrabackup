# Result differences depending on FS case sensitivity.
if (!$require_case_insensitive_file_system)
{
  --source include/have_case_sensitive_file_system.inc
}

#------------------------------------------------------------------------------
# i_s_libraries.test
# .test file for MySQL regression suite
# Purpose:  To test the presence, structure, and behavior
#                    of INFORMATION_SCHEMA.LIBRARIES
# Author:   WL#16360
#------------------------------------------------------------------------------

-- echo ################################################################################
-- echo # Testcase libraries.1: Ensure that the INFORMATION_SCHEMA.LIBRARIES
-- echo #                   table has the following columns, in the following order:
-- echo #
-- echo #                   LIBRARY_CATALOG,
-- echo #                   LIBRARY_SCHEMA (shows the database, or schema, in which
-- echo #                          the library resides),
-- echo #                   LIBRARY_NAME (shows the library name),
-- echo #                   LIBRARY_DEFINITION (shows as much of the library body as
-- echo #                          is possible in the allotted space),
-- echo #                   LANGUAGE (currently only JavaScript is supported),
-- echo #                   CREATED (shows the timestamp of the time the library was
-- echo #                          created),
-- echo #                   LAST_ALTERED (shows the timestamp of the time the library
-- echo #                          was last altered),
-- echo #                   CREATOR (shows the user who created the library).
-- echo ################################################################################
-- echo # ========== libraries.1 ==========
USE INFORMATION_SCHEMA;
SHOW CREATE VIEW INFORMATION_SCHEMA.LIBRARIES;

SELECT * FROM information_schema.columns
WHERE table_schema = 'information_schema'
  AND table_name   = 'LIBRARIES'
ORDER BY ordinal_position;

DESCRIBE INFORMATION_SCHEMA.LIBRARIES;

-- echo ###############################################################################
-- echo # Testcase libraries.2:  Successful library CREATE will populate
-- echo #                        I_S.LIBRARIES view but not the I_S.ROUTINES
-- echo ###############################################################################
-- echo # ========== libraries.2 ==========
CREATE DATABASE i_s_libraries_test;
USE i_s_libraries_test;

CREATE LIBRARY test_2 LANGUAGE JAVASCRIPT AS $$
export function f(n) {  return n } $$;
CREATE FUNCTION test_2_func (s char(20)) RETURNS CHAR(50)
RETURN CONCAT('Hello, ',s,'!');
delimiter //;
CREATE PROCEDURE test_2_proc (OUT param1 INT)
  BEGIN
   SELECT 2+2 as param1;
  END;
//
delimiter ;//
-- echo # must show only the library, not the procedure nor the function.
--replace_column 6 <created> 7 <modified>
SELECT * FROM INFORMATION_SCHEMA.LIBRARIES
WHERE LIBRARY_SCHEMA = 'i_s_libraries_test';
-- echo # must show only the procedures and routines, not the library.
--replace_column 24 <created> 25 <modified>
SELECT ROUTINE_NAME, ROUTINE_TYPE FROM INFORMATION_SCHEMA.ROUTINES
WHERE ROUTINE_SCHEMA = 'i_s_libraries_test' ORDER BY ROUTINE_TYPE, ROUTINE_NAME;
DROP FUNCTION test_2_func;
DROP PROCEDURE test_2_proc;

-- echo # Use duplicate names
CREATE FUNCTION test_2 (s char(20)) RETURNS CHAR(50)
RETURN CONCAT('Hello, ',s,'!');
delimiter //;
CREATE PROCEDURE test_2 (OUT param1 INT)
  BEGIN
   SELECT 2+2 as param1;
  END;
//
delimiter ;//
-- echo # must show only the library
--replace_column 6 <created> 7 <modified>
SELECT * FROM INFORMATION_SCHEMA.LIBRARIES
WHERE LIBRARY_SCHEMA = 'i_s_libraries_test';
-- echo # must show only the procedures and routines.
--replace_column 24 <created> 25 <modified>
SELECT ROUTINE_NAME, ROUTINE_TYPE FROM INFORMATION_SCHEMA.ROUTINES
WHERE ROUTINE_SCHEMA = 'i_s_libraries_test' ORDER BY ROUTINE_TYPE, ROUTINE_NAME;

-- echo # Remove duplicate names
DROP FUNCTION test_2;
DROP PROCEDURE test_2;
--replace_column 6 <created> 7 <modified>
SELECT * FROM INFORMATION_SCHEMA.LIBRARIES
WHERE LIBRARY_SCHEMA = 'i_s_libraries_test';
-- echo # must be empty.
--replace_column 24 <created> 25 <modified>
SELECT ROUTINE_NAME, ROUTINE_TYPE FROM INFORMATION_SCHEMA.ROUTINES
WHERE ROUTINE_SCHEMA = 'i_s_libraries_test' ORDER BY ROUTINE_TYPE, ROUTINE_NAME;

DROP DATABASE i_s_libraries_test;
-- echo ###############################################################################
-- echo # Testcase libraries.3:  Unsuccessful library CREATE will not populate
-- echo #                     I_S.LIBRARIES view
-- echo ###############################################################################
-- echo # ========== libraries.3 ==========
CREATE DATABASE i_s_libraries_test;
USE i_s_libraries_test;

-- echo # Missing closing ')' character at the end of 's char(20) in func declaration
--error ER_PARSE_ERROR
CREATE LIBRARY test_library_3 LANGUAGE JAVASCRIPT (AS $$
export function f(n) {
  return n
}
$$;
--replace_column 6 <created> 7 <modified>
SELECT * FROM INFORMATION_SCHEMA.LIBRARIES
WHERE LIBRARY_SCHEMA = 'i_s_libraries_test';

-- echo ###############################################################################
-- echo # Testcase libraries.4:  DROP LIBRARY - Verify DROP of a library
-- echo #                                     removes I_S.LIBRARIES data for that
-- echo #                                     function / procedure
-- echo ###############################################################################
-- echo # ========== libraries.4 ==========
DROP DATABASE IF EXISTS i_s_libraries_test;

CREATE DATABASE i_s_libraries_test;
USE i_s_libraries_test;

CREATE LIBRARY test_library_4 LANGUAGE JAVASCRIPT AS $$
export function f(n) {  return n } $$;
--replace_column 6 <created> 7 <modified>
SELECT * FROM INFORMATION_SCHEMA.LIBRARIES
WHERE LIBRARY_SCHEMA = 'i_s_libraries_test';
DROP LIBRARY test_library_4;
--replace_column 6 <created> 7 <modified>
SELECT * FROM INFORMATION_SCHEMA.LIBRARIES
WHERE LIBRARY_SCHEMA = 'i_s_libraries_test';

-- echo # final clean up
DROP DATABASE i_s_libraries_test;
USE test;

SELECT * FROM INFORMATION_SCHEMA.LIBRARIES;
--echo # End of INFORMATION_SCHEMA.LIBRARIES tests
