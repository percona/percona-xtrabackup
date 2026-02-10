--echo #
--echo # TODO:WL#16737 count the successful and failed statements.
--echo #

CREATE DATABASE counting;
USE counting;

--echo # CREATE LIBRARY

CREATE LIBRARY lib1 LANGUAGE JAVASCRIPT
AS $$ export function f(n) { return n } $$;
SHOW CREATE LIBRARY lib1;

CREATE LIBRARY lib2 COMMENT "Library Commnet" LANGUAGE JAVASCRIPT
AS $$ export function f(n) { return n } $$;
SHOW CREATE LIBRARY lib2;

SHOW STATUS LIKE 'Com%library';

--error ER_PARSE_ERROR
CREATE LIBRARY library_with_error
COMMENT 'my comment'
AS $$
  export function foo() {return 1}
$$;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY library_with_error;

--echo # should be 2 Com_create_library and 2 Com_show_create_library
SHOW STATUS LIKE 'Com%library';

CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
SHOW CREATE LIBRARY lib1;

--echo # should be 2 Com_create_library and 3 Com_show_create_library
SHOW STATUS LIKE 'Com%library';

--echo # ALTER LIBRARY #
ALTER LIBRARY lib1 COMMENT "JS Library Comment";
ALTER LIBRARY lib2 COMMENT "JS Library Comment";
--echo # should be 2 Com_alter_library
SHOW STATUS LIKE 'Com%library';

--error ER_SP_DOES_NOT_EXIST
ALTER LIBRARY library_with_error COMMENT "JS Library Comment";
--echo # should be 2 Com_alter_library
SHOW STATUS LIKE 'Com%library';

--error ER_PARSE_ERROR
ALTER LIBRARY lib1 LANGUAGE JAVASCRIPT;
--echo # should be 2 Com_alter_library
SHOW STATUS LIKE 'Com%library';

--echo # DROP LIBRARY #
DROP LIBRARY lib1;
DROP LIBRARY lib2;
--echo # should be 2 Com_drop_library
SHOW STATUS LIKE 'Com%library';
--error ER_SP_DOES_NOT_EXIST
DROP LIBRARY library_with_error;
--echo # should be 2 Com_drop_library
SHOW STATUS LIKE 'Com%library';
DROP LIBRARY IF EXISTS lib1;
--echo # should be 2 Com_drop_library
SHOW STATUS LIKE 'Com%library';

--echo ###################
--echo # CREATE FUNCTION #
--echo ###################
CREATE FUNCTION f1(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
AS $$ return 42 $$;
SHOW CREATE FUNCTION f1;

CREATE FUNCTION f2(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
AS $$ return 42 $$;
SHOW CREATE FUNCTION f2;

SHOW STATUS LIKE 'Com%function';

--error ER_PARSE_ERROR
CREATE FUNCTION function_with_error(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (lib1 AS lib2)
USING (lib3)
AS $$ return lib2.f(n) $$;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE FUNCTION function_with_error;

--echo # should be 2 Com_create_function and 2 Com_show_create_function
SHOW STATUS LIKE 'Com%function';

CREATE FUNCTION IF NOT EXISTS f1(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
AS $$ return lib2.f(n) $$;
SHOW CREATE FUNCTION f1;

--echo # should be 2 Com_create_function and 3 Com_show_create_function
SHOW STATUS LIKE 'Com%function';

--echo # ALTER FUNCTION #
ALTER FUNCTION f1 COMMENT "Updated Function's Comment";
SHOW CREATE FUNCTION f1;
ALTER FUNCTION f2 COMMENT "Updated Function's Comment";
SHOW CREATE FUNCTION f1;
--echo # should be 2 Com_create_function and 3 Com_show_create_function
SHOW STATUS LIKE 'Com%function';

--error ER_SP_DOES_NOT_EXIST
ALTER FUNCTION function_with_error COMMENT "Updated Function's Comment";
--echo # should be 2 Com_create_function and 3 Com_show_create_function
SHOW STATUS LIKE 'Com%function';

--error ER_SP_DOES_NOT_EXIST
SHOW CREATE FUNCTION function_with_error;
--echo # should be 2 Com_create_function and 3 Com_show_create_function
SHOW STATUS LIKE 'Com%function';

--error ER_PARSE_ERROR
ALTER FUNCTION f1 COMMENT "Updated Function's Comment" UNKNOWN_ARGUMENT;
SHOW CREATE FUNCTION f1;
--echo # should be 2 Com_create_function and 3 Com_show_create_function
SHOW STATUS LIKE 'Com%function';

--echo # Cleanup.
DROP DATABASE counting;

SHOW STATUS LIKE 'Com%library';
