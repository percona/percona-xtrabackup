--echo #
--echo # WL#15254: SQL syntax extensions to support external language routines
--echo #

DELIMITER //;

--echo #
--echo # Tests for LANGUAGE clause
--echo #

CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE SQL
  BEGIN RETURN a-1; END //
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_name = "foo" //
DROP FUNCTION foo//

--echo # SQL is default
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER DETERMINISTIC
  BEGIN RETURN a-1; END //
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS where Language like "%JAVA%"//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_name = "foo" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION JAVASCRIPT(x INTEGER) RETURNS INTEGER DETERMINISTIC
LANGUAGE JAVASCRIPT
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION JAVASCRIPT//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'JAVASCRIPT'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT JAVASCRIPT(2)//
DROP FUNCTION JAVASCRIPT//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE PYTHON
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE RUBY
AS $$
  x-1
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo # Test lower-case
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE javascript
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo # Test mixed-case
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE jAvaScRIpt
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo # Any valid identifier can be used for language name
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE JAVA1
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE $NOLANG
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC
LANGUAGE $$JAVASCRIPT
AS $$
  return x-1;
$$
//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE 123j
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE __
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC
LANGUAGE "JAVASCRIPT"
AS $$
  return x-1;
$$
//


--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC
LANGUAGE 'JAVASCRIPT'
AS $$
  return x-1;
$$
//


--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE " "
AS $$
  return x-1;
$$
//


--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE J?
AS $$
  return x-1;
$$
//


--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC
LANGUAGE \JAVASCRIPT
AS $$
  return x-1;
$$
//


--echo # An identifier may not consist solely of digits
--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE 123
AS $$
  return x-1;
$$
//


--echo # MySQL allows multiple clauses for the same characteristics
--echo # (This is not according to SQL standard). Last one will take effect.
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER
LANGUAGE JAVASCRIPT DETERMINISTIC LANGUAGE SQL
  BEGIN RETURN a-1; END //
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
LANGUAGE SQL DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo # Multiple languages in one clause is not allowed
--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
LANGUAGE SQL DETERMINISTIC LANGUAGE JAVASCRIPT PYTHON
AS $$
  return x-1;
$$
//


--echo Check "weird" white space before and after JAVASCRIPT.
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
LANGUAGE
JAVASCRIPT

  AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
LANGUAGE	
	 JAVASCRIPT
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
LANGUAGE	  	 JAVASCRIPT  DETERMINISTIC	
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
LANGUAGE
-- Comment
JAVASCRIPT
-- Comment
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo The language will here be interpreted to be BEGIN
--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE
BEGIN RETURN a-1; END //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER
LANGUAGE SQL DETERMINISTIC LANGUAGE
BEGIN RETURN a-1; END //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE
BEGIN a-1; END //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE
AS $$
  return x-1;
$$
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
LANGUAGE JAVASCRIPT DETERMINISTIC LANGUAGE
AS $$
  return x-1;
$$
//

--echo #Combinations with other characteristics
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC NO SQL
LANGUAGE JAVASCRIPT
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//


CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER DETERMINISTIC NO SQL
LANGUAGE SQL
  BEGIN RETURN a-1; END //
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE SQL NO SQL
  BEGIN RETURN a-1; END //
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE JAVASCRIPT
CONTAINS SQL
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//


CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC READS SQL DATA
LANGUAGE JAVASCRIPT
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//


CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE JAVASCRIPT
MODIFIES SQL DATA
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE SQL
CONTAINS SQL
  BEGIN RETURN a-1; END //
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER DETERMINISTIC READS SQL DATA
LANGUAGE SQL
  BEGIN RETURN a-1; END //
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE SQL
MODIFIES SQL DATA
  BEGIN RETURN a-1; END //
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER NOT DETERMINISTIC LANGUAGE SQL
  BEGIN RETURN a-1; END //
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER NOT DETERMINISTIC
LANGUAGE JAVASCRIPT
AS $$
  return x-1;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo # This is not SQL
--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE SQL
AS $$ return a-1; $$ //

--echo # This is not Javascript
--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
BEGIN RETURN a-1; END //

--echo # Repeat test cases for PROCEDURE
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE SQL
  BEGIN DECLARE b INTEGER DEFAULT a; END //
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_name = "bar" //
DROP PROCEDURE bar//

--echo # SQL is default
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC
  BEGIN DECLARE b INTEGER DEFAULT a; END //
DROP PROCEDURE bar//

--echo # Currently, no other language than SQL is supported
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS where Language like "%JAVA%"//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_name = "bar" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP PROCEDURE bar//

CREATE PROCEDURE JAVASCRIPT(a INTEGER) DETERMINISTIC
LANGUAGE JAVASCRIPT
AS $$ let b = a; $$ //
SHOW CREATE procedure JAVASCRIPT//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'JAVASCRIPT'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL JAVASCRIPT(2)//
DROP procedure JAVASCRIPT//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE PYTHON
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE RUBY
AS $$ b = a $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo # Test lower-case
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE javascript
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo # Test mixed-case
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE jAvaScRIpt
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo # Any valid identifier can be used for language name
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE JAVA1
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE $NOLANG
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE $$JAVASCRIPT
AS $$ let b = a; $$ //

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE 123j
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE __
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE "JAVASCRIPT"
AS $$ let b = a; $$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE 'JAVASCRIPT'
AS $$ let b = a; $$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE " "
AS $$ let b = a; $$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE J?
AS $$ let b = a; $$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE \JAVASCRIPT
AS $$ let b = a; $$ //

--echo # An identifier may not consist solely of digits
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE 123
AS $$ let b = a; $$ //

--echo # MySQL allows multiple clauses for the same characteristics
--echo # (This is not according to SQL standard). Last one will take effect.
CREATE PROCEDURE bar(a INTEGER)
LANGUAGE JAVASCRIPT DETERMINISTIC LANGUAGE SQL
  BEGIN DECLARE b INTEGER DEFAULT a; END //
DROP PROCEDURE bar//

CREATE PROCEDURE bar(a INTEGER)
LANGUAGE SQL DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo # Multiple languages in one clause is not allowed
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
LANGUAGE SQL DETERMINISTIC LANGUAGE JAVASCRIPT PYTHON
AS $$ b = a $$ //

--echo Check "weird" white space before and after JAVASCRIPT.
CREATE PROCEDURE bar(a INTEGER)
LANGUAGE
JAVASCRIPT

AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
LANGUAGE	
	 JAVASCRIPT
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
LANGUAGE	  	 JAVASCRIPT  DETERMINISTIC	
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
LANGUAGE
-- Comment
JAVASCRIPT
-- Comment
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo The language will here be interpreted to be BEGIN
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE
BEGIN DECLARE b INTEGER DEFAULT a; END //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
LANGUAGE SQL DETERMINISTIC LANGUAGE
BEGIN DECLARE b INTEGER DEFAULT a; END //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE
BEGIN a-1; END //

--echo #Combinations with other characteristics
CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC NO SQL
LANGUAGE JAVASCRIPT
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC NO SQL
LANGUAGE SQL
  BEGIN DECLARE b INTEGER DEFAULT a; END //
DROP PROCEDURE bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE SQL NO SQL
  BEGIN DECLARE b INTEGER DEFAULT a; END //
DROP PROCEDURE bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE JAVASCRIPT
CONTAINS SQL
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC READS SQL DATA
LANGUAGE JAVASCRIPT
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE JAVASCRIPT
MODIFIES SQL DATA
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE SQL
CONTAINS SQL
  BEGIN DECLARE b INTEGER DEFAULT a; END //
DROP PROCEDURE bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC READS SQL DATA
LANGUAGE SQL
  BEGIN DECLARE b INTEGER DEFAULT a; END //
DROP PROCEDURE bar//

CREATE PROCEDURE bar(a INTEGER) DETERMINISTIC LANGUAGE SQL
MODIFIES SQL DATA
  BEGIN DECLARE b INTEGER DEFAULT a; END //
DROP PROCEDURE bar//

CREATE PROCEDURE bar(a INTEGER) NOT DETERMINISTIC LANGUAGE SQL
  BEGIN DECLARE b INTEGER DEFAULT a; END //
DROP PROCEDURE bar//

CREATE PROCEDURE bar(a INTEGER) NOT DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$ let b = a; $$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo # This is not SQL
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
LANGUAGE SQL DETERMINISTIC LANGUAGE SQL
$$ let b = a; $$ //

--echo # This is not Javascript
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
BEGIN DECLARE b INTEGER DEFAULT a; END //

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE SQL
BEGIN DECLARE b INTEGER DEFAULT a; END //

--echo # Changing language will not be allowed
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER DETERMINISTIC LANGUAGE SQL
  BEGIN RETURN a-1; END //
--error ER_SP_NO_ALTER_LANGUAGE
ALTER FUNCTION foo LANGUAGE JAVASCRIPT //
SHOW CREATE FUNCTION foo//
ALTER FUNCTION foo LANGUAGE SQL NO SQL //
SHOW CREATE FUNCTION foo//
--error ER_PARSE_ERROR
ALTER FUNCTION foo LANGUAGE NO SQL //
DROP FUNCTION foo //

--error ER_SP_NO_ALTER_LANGUAGE
ALTER PROCEDURE bar LANGUAGE JAVASCRIPT //
ALTER PROCEDURE bar LANGUAGE SQL NO SQL //
SHOW CREATE PROCEDURE bar//
--error ER_PARSE_ERROR
ALTER PROCEDURE bar LANGUAGE NO SQL //
DROP PROCEDURE bar //

--echo #
--echo # Tests for inline code in external language
--echo #
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  return x-1;
$$ //
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS 'return x-1;' //
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS "return x-1;" //
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS '
  return x-1;
' //
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
"
   return x-1;
" //
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo Routine body must come after characteristics
--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
AS $$
  return x-1;
$$
DETERMINISTIC LANGUAGE JAVASCRIPT
//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
'
  return "$$" + a + "$$";
'
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$code$
  return "$$" + a + "$$";
$code$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$coDE$
  return "$$" + a + "$$";
$coDE$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$CODE$
  return "$$" + a + "$$";
$CODE$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$tag$
  return "$$" + a + "$$";
$tag$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$TAG$
  return "$$" + a + "$$";
$TAG$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$NULL$
  return "$$" + a + "$$";
$NULL$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$abdc_1234$
  return "$$" + a + "$$";
$abdc_1234$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$_a$
  return "$$" + a + "$$";
$_a$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$_$
  return "$$" + a + "$$";
$_$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$0$
  return "$$" + a + "$$";
$0$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$100$
  return "$$" + a + "$$";
$100$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$12345678901234$
  return "$$" + a + "$$";
$12345678901234$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM$
  return "$$" + a + "$$";
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$¡$
  return "$$" + a + "$$";
$¡$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM¡™£¢∞§¶•ªº–≠œ∑´®†¥¨ˆøπ“‘«åß∂ƒ©˙∆˚¬…æΩ≈ç√∫µ≤≥÷⁄€‹›ﬁﬂ‡°·‚—±ÅÍÎÏ˝ÓÔÒÚÆ¸˛Ç◊ıÂ¯˘¿$
  return "$$" + a + "$$";
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM¡™£¢∞§¶•ªº–≠œ∑´®†¥¨ˆøπ“‘«åß∂ƒ©˙∆˚¬…æΩ≈ç√∫µ≤≥÷⁄€‹›ﬁﬂ‡°·‚—±ÅÍÎÏ˝ÓÔÒÚÆ¸˛Ç◊ıÂ¯˘¿$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS	      $$
  const $x = 3;
  const $y$ = 4;
  return $x + $y$ + a;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$code$
  const x$$ = 3;
  const $y$$z = 4;
  return x$$ + $y$$z + a;

$code$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
$$
  return x-1;
$$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE 'return x-1;' //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE return x-1 //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC AS LANGUAGE JAVASCRIPT
"return x-1;" //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
$code$
  return "$$" + a + "$$";
$code$
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $
  return x-1;
$ //

--echo # Test conflicting dollar quotes
--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  return "$$" + a + "$$";
$$
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$
  return "$code$" + a + "$code$";
$code$
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  return x-1;
$code$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$code$
  return "$$" + a + "$$";
$$
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$Code$
  return "$$" + a + "$$";
$CODE$
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$CodE$
  return "$$" + a + "$$";
$code$
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS	      $$
  const $x = 3;
  const $y$ = 4;
  return $x + $y$ + a;
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$code$
  const x$$ = 3;
  const $y$$z = 4;
  return x$$ + $y$$z + a;

$
//

# Tag differs in last char
--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM™∞•–≠∑†π“‘∂ƒ˙∆˚…Ω≈√∫≤≥⁄€‹›ﬁﬂ‡‚—˝˛◊ı˘$
  return "$$" + a + "$$";
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM™∞•–≠∑†π“‘∂ƒ˙∆˚…Ω≈√∫≤≥⁄€‹›ﬁﬂ‡‚—˝˛◊ı˗$
//

--echo Check cases with invalid dollar tags
--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $+a$
  return x-1;
$+a$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $a-$
  return x-1;
$a-$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $a(b$
  return x-1;
$a(b$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $a~b$
  return x-1;
$a~b$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $ $
  return x-1;
$ $ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $I'm$
  return x-1;
$I'm$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $'Hi'$
  return x-1;
$'Hi'$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $a"b"$
  return x-1;
$a"b"$ //

--echo # Test multi-byte characters in code (∂ is 3 bytes in utf8mb3)
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  return "∂" + a + "$∂$";
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$
  return "∂" + a + "$∂$$";
$code$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$
  return "∂" + a + "$∂$$∂";
$code$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$∂
  return "∂" + a + "$∂$";
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo Test multi-byte characters in quote tags
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $∂$
  return "∂" + a + "∂$";
$∂$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $co∂e$
  return "∂" + a + "$∂$$";
$co∂e$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

# The 4-byte utf8mb3 characters below may not be correctly displayed in an editor
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $😀$
  return "😀$😔" + a + "😀$😔$";
$😀$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $😀😔$
  return "😀$😔" + a + "😀$😔$";
$😀😔$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $x😀$
  return "$x😀😔" + a + "$x😀😔$";
$x😀$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $😀x$
  return "$😀x😔$" + a + "$😀😔x$";
$😀x$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $x😀y$
  return "$x😀😔y$" + a + "$x😀y😔$";
$x😀y$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $x😀y😔$
  return "😀😔$" + a + "😀$😔$";
$x😀y😔$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $😀😔$
  return "😀$😔" + a + "😀$😔$";
$😔😀$
//

--echo # Escape sequences is not supported in dollar quoted strings
--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  return "\$$" + a + "\$code$";
$$
//

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$
  return "\$$" + a + "\$code$";
$code$
//


--echo # Check some variants that should give external parse error
--echo # if the language is supported
CREATE FUNCTION foo(x INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$ rtrn "$$" + x + "$foo$"; $code$ //
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

CREATE FUNCTION foo(x INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  const $x = x,
  const $y = 1
  return $x + $y;
$$
//
SHOW CREATE FUNCTION foo//
--replace_column 6 <modified> 7 <created>
SHOW FUNCTION STATUS WHERE Name = 'foo'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
SELECT foo(2)//
DROP FUNCTION foo//

--echo # Multiple occurrences of routine body is not allowed
--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS 'return "$$" + a + "$foo$";'
AS $$ return a-1; $$ //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS VARCHAR(20)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$ return "$$" + a + "$foo$"; $code$
AS $$ return a-1; $$ //

--echo # Both external code and SQL code is not allowed
--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$ return a-1; $$
BEGIN RETURN a-1; END //

--error ER_PARSE_ERROR
CREATE FUNCTION foo(a INTEGER) RETURNS INTEGER
DETERMINISTIC LANGUAGE SQL
AS $$ return a-1; $$
BEGIN RETURN a-1; END //

--echo # Parse error if no routine body
--error ER_PARSE_ERROR
CREATE FUNCTION foo() RETURNS INTEGER
DETERMINISTIC LANGUAGE JAVASCRIPT//

--echo # Repeat test cases for procedures
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  let b = a;
$$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS 'let b = a;' //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS "let b = a;" //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS '
  let b = a;
' //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
"
   let b = a;
" //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo Routine body must come after characteristics
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
AS $$
  let b = a;
$$
DETERMINISTIC LANGUAGE JAVASCRIPT
//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
'
  let b = "$$" + a + "$$";
'
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$code$
  let b = "$$" + a + "$$";
$code$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$coDE$
  let b = "$$" + a + "$$";
$coDE$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$CODE$
  let b = "$$" + a + "$$";
$CODE$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$tag$
  let b = "$$" + a + "$$";
$tag$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$TAG$
  let b = "$$" + a + "$$";
$TAG$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$NULL$
  let b = "$$" + a + "$$";
$NULL$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$abdc_1234$
  let b = "$$" + a + "$$";
$abdc_1234$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$_a$
  let b = "$$" + a + "$$";
$_a$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$_$
  let b = "$$" + a + "$$";
$_$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$0$
  let b = "$$" + a + "$$";
$0$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$100$
  let b = "$$" + a + "$$";
$100$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$12345678901234$
  let b = "$$" + a + "$$";
$12345678901234$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM$
  let b = "$$" + a + "$$";
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$¡$
  let b = "$$" + a + "$$";
$¡$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM¡™£¢∞§¶•ªº–≠œ∑´®†¥¨ˆøπ“‘«åß∂ƒ©˙∆˚¬…æΩ≈ç√∫µ≤≥÷⁄€‹›ﬁﬂ‡°·‚—±ÅÍÎÏ˝ÓÔÒÚÆ¸˛Ç◊ıÂ¯˘¿$
  let b = "$$" + a + "$$";
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM¡™£¢∞§¶•ªº–≠œ∑´®†¥¨ˆøπ“‘«åß∂ƒ©˙∆˚¬…æΩ≈ç√∫µ≤≥÷⁄€‹›ﬁﬂ‡°·‚—±ÅÍÎÏ˝ÓÔÒÚÆ¸˛Ç◊ıÂ¯˘¿$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS	      $$
  const $x = 3;
  const $y$ = 4;
  let b = $x + $y$ + a;
$$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$code$
  const x$$ = 3;
  const $y$$z = 4;
  let b = x$$ + $y$$z + a;

$code$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
$$
  let b = a;
$$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE 'let b = a;' //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE let b = a //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC AS LANGUAGE JAVASCRIPT
"let b = a;" //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
$code$
  let b = "$$" + a + "$$";
$code$
//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $
  let b = a;
$ //

--echo # Test conflicting dollar quotes
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  let b = "$$" + a + "$$";
$$
//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$
  let b = "$code$" + a + "$code$";
$code$
//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  let b = a;
$code$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$code$
  let b = "$$" + a + "$$";
$$
//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$Code$
  let b = "$$" + a + "$$";
$CODE$
//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$CodE$
  let b = "$$" + a + "$$";
$code$
//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS	      $$
  const $x = 3;
  const $y$ = 4;
  let b = $x + $y$ + a;
//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$code$
  const x$$ = 3;
  const $y$$z = 4;
  let b = x$$ + $y$$z + a;

$
//

# Tag differs in last char
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM™∞•–≠∑†π“‘∂ƒ˙∆˚…Ω≈√∫≤≥⁄€‹›ﬁﬂ‡‚—˝˛◊ı˘$
  let b = "$$" + a + "$$";
$1234567890qwertyuiopasdfghjklzxcvbnm_QWERTYUIOPASDFGHJKLZXCVBNM™∞•–≠∑†π“‘∂ƒ˙∆˚…Ω≈√∫≤≥⁄€‹›ﬁﬂ‡‚—˝˛◊ı˗$
//

--echo Check cases with invalid dollar tags
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $+a$
  let b = a;
$+a$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $a-$
  let b = a;
$a-$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $a(b$
  let b = a;
$a(b$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $a~b$
  let b = a;
$a~b$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $ $
  let b = a;
$ $ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $I'm$
  let b = a;
$I'm$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $'Hi'$
  let b = a;
$'Hi'$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $a"b"$
  let b = a;
$a"b"$ //

--echo # Test multi-byte characters in code (∂ is 3 bytes in utf8mb3)
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  let b = "∂" + a + "$∂$";
$$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$
  let b = "∂" + a + "$∂$$";
$code$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$
  let b = "∂" + a + "$∂$$∂";
$code$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$∂
  let b = "∂" + a + "$∂$";
$$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo Test multi-byte characters in quote tags
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $∂$
  let b = "∂" + a + "∂$";
$∂$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $co∂e$
  let b = "∂" + a + "$∂$$";
$co∂e$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

# The 4-byte utf8mb3 characters below may not be correctly displayed in an editor
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $😀$
  let b = "😀$😔" + a + "😀$😔$";
$😀$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $😀😔$
  let b = "😀$😔" + a + "😀$😔$";
$😀😔$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $x😀$
  let b = "$x😀😔" + a + "$x😀😔$";
$x😀$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $😀x$
  let b = "$😀x😔$" + a + "$😀😔x$";
$😀x$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $x😀y$
  let b = "$x😀😔y$" + a + "$x😀y😔$";
$x😀y$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $x😀y😔$
  let b = "😀😔$" + a + "😀$😔$";
$x😀y😔$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $😀😔$
  let b = "😀$😔" + a + "😀$😔$";
$😔😀$
//

--echo # Escape sequences is not supported in dollar quoted strings
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  let b = "\$$" + a + "\$code$";
$$
//

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$
  let b = "\$$" + a + "\$code$";
$code$
//


--echo # Check some variants that should give external parse error
--echo # if the language is supported
CREATE PROCEDURE bar(x INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$ let b := "$$" + x + "$foo$"; $code$ //
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

CREATE PROCEDURE bar(x INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$
  const $x = x,
  const $y = 1
  let b = $x + $y;
$$
//
SHOW CREATE procedure bar//
--replace_column 6 <modified> 7 <created>
SHOW PROCEDURE STATUS WHERE Name = 'bar'//
SELECT routine_name, routine_type, data_type, routine_body,
       external_name, external_language, parameter_style, routine_definition
FROM information_schema.routines
WHERE routine_schema = "test" //
--error ER_LANGUAGE_COMPONENT_NOT_AVAILABLE
CALL bar(2)//
DROP procedure bar//

--echo # Multiple occurrences of routine body is not allowed
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS 'let b = "$$" + a + "$foo$";'
AS $$ let b = a-1; $$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $code$ let b = "$$" + a + "$foo$"; $code$
AS $$ let b = a-1; $$ //

--echo # Both external code and SQL code is not allowed
--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE JAVASCRIPT
AS $$ let b = a-1; $$
BEGIN RETURN a-1; END //

--error ER_PARSE_ERROR
CREATE PROCEDURE bar(a INTEGER)
DETERMINISTIC LANGUAGE SQL
AS $$ let b = a-1; $$
BEGIN RETURN a-1; END //

--echo # Parse error if no routine body
--error ER_PARSE_ERROR
CREATE PROCEDURE bar()
DETERMINISTIC LANGUAGE JAVASCRIPT//

--echo # Check that dollar quotes can not be used in other context
--error ER_PARSE_ERROR
CREATE FUNCTION $$foo(x INTEGER) INTEGER DETERMINISTIC
LANGUAGE JAVASCRIPT AS $$ let b = x-1; $$ //

--error ER_PARSE_ERROR
CREATE PROCEDURE $$bar(a INTEGER) DETERMINISTIC
LANGUAGE JAVASCRIPT AS $$ let b = a; $$ //

delimiter ;//

--error ER_PARSE_ERROR
SELECT $$Hello world!$$;
--error ER_PARSE_ERROR
SELECT $hi$Hello world!$hi$;

--echo # Check that dollars can still be used in identifiers
SELECT 1 AS $hi;
SELECT 1 AS h$i;
SELECT 1 AS h$$i;
SELECT 1 AS h$i$;

--echo # Identifiers matching dollar quotes can no longer be used
--error ER_PARSE_ERROR
SELECT 1 AS $$;
--error ER_PARSE_ERROR
SELECT 1 AS $hi$;
--error ER_PARSE_ERROR
SELECT 1 AS $h$i;

--echo # ... unless quoted
SELECT 1 AS `$$`;
SELECT 1 AS `$hi$`;
SELECT 1 AS `$h$i`;

--echo #
--echo # WL#16359: SQL syntax for JavaScript Libraries
--echo #

CREATE LIBRARY lib1 LANGUAGE JAVASCRIPT
AS $$
export function f(n) {
  return n
}
$$;

--echo # Tests for valid identifiers
--error ER_TOO_LONG_IDENT
CREATE LIBRARY
this_name_is_too_long23456789312345678941234567895123456789612345
LANGUAGE JAVASCRIPT
AS "export function f(n) {return n}";

CREATE LIBRARY
test.this_name_is_not_too_long678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS "export function f(n) {return n}";
DROP LIBRARY
this_name_is_not_too_long678931234567894123456789512345678961234;

--error ER_PARSE_ERROR
CREATE LIBRARY 01234
LANGUAGE JAVASCRIPT
AS "export function f(n) {return n}";

CREATE LIBRARY
`01234`
LANGUAGE JAVASCRIPT
AS "export function f(n) {return n}";
DROP LIBRARY `01234`;

--echo # Tests for IF NOT EXISTS
CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';

--error ER_PARSE_ERROR
CREATE LIBRARY IF EXISTS test.lib2 LANGUAGE JAVASCRIPT
AS "export function f(n) {return n}";

--echo # This will not fail since component is not installed
CREATE LIBRARY IF NOT EXISTS test.lib2 LANGUAGE JAVASCRIPT
AS "whatever";

--echo # Tests for language clause
CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE Javascript
AS 'export function f(n) {return n}';

CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE JavaScript
AS 'export function f(n) {return n}';

CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE javascript
AS 'export function f(n) {return n}';

--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS lib1
AS 'export function f(n) {return n}'
LANGUAGE javascript;

--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS lib1
AS 'export function f(n) {return n}';

--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE
AS 'export function f(n) {return n}';

# SQL is a reserved keyword, so this will give parse error
--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS test.lib2 LANGUAGE SQL
AS "WHATEVER";

CREATE LIBRARY IF NOT EXISTS test.lib3 LANGUAGE PYTHON
AS $$ def fib(n): return n $$;
DROP LIBRARY test.lib3;

CREATE LIBRARY IF NOT EXISTS test.lib3 LANGUAGE JAVA1
AS 'export function f(n) {return n}';
DROP LIBRARY test.lib3;

CREATE LIBRARY IF NOT EXISTS test.lib3 LANGUAGE $JAVASCRIPT
AS 'export function f(n) {return n}';
DROP LIBRARY test.lib3;

--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS test.lib2 LANGUAGE $$JAVASCRIPT
AS 'export function f(n) {return n}';

CREATE LIBRARY IF NOT EXISTS test.lib3 LANGUAGE 123j
AS 'export function f(n) {return n}';
DROP LIBRARY test.lib3;

--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS test.lib2 LANGUAGE 123
AS 'export function f(n) {return n}';

CREATE LIBRARY IF NOT EXISTS test.lib3 LANGUAGE `123`
AS 'export function f(n) {return n}';
DROP LIBRARY test.lib3;

CREATE LIBRARY IF NOT EXISTS test.lib3 LANGUAGE __
AS 'export function f(n) {return n}';
DROP LIBRARY test.lib3;

--echo # Tests for AS clause
--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE JAVASCRIPT;

--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE JAVASCRIPT
AS export function f(n) {return n};

--error ER_PARSE_ERROR
CREATE LIBRARY IF NOT EXISTS lib1 LANGUAGE JAVASCRIPT
'export function f(n) {return n}';

--echo # Tests for CREATE LIBRARY with a COMMENT clause

CREATE LIBRARY library_with_comment COMMENT "library comment" LANGUAGE JAVASCRIPT AS " export function f(n) {  return n+1 } ";
CREATE LIBRARY library_with_comment2 LANGUAGE JAVASCRIPT COMMENT "library comment" AS " export function f(n) {  return n+1 } ";
CREATE LIBRARY library_with_empty_comment COMMENT "" LANGUAGE JAVASCRIPT AS " export function f(n) {  return n+1 } ";
CREATE LIBRARY library_with_extra_longcomment COMMENT "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789" LANGUAGE JAVASCRIPT AS " export function f(n) {  return n+1 } ";
--replace_column 5 <modified> 6 <created>
SHOW LIBRARY STATUS WHERE db = 'test';
SHOW CREATE LIBRARY library_with_comment;
SHOW CREATE LIBRARY library_with_empty_comment;
SHOW CREATE LIBRARY library_with_extra_longcomment;

--echo # Tests for ALTER LIBRARY
ALTER LIBRARY library_with_comment COMMENT "updated comment";
SHOW CREATE LIBRARY library_with_comment;
CREATE LIBRARY  library_without_comment LANGUAGE JAVASCRIPT AS " export function f(n) {  return n+1 } ";
SHOW CREATE LIBRARY library_without_comment;
ALTER LIBRARY library_without_comment COMMENT "added comment";
SHOW CREATE LIBRARY library_without_comment;
ALTER LIBRARY library_with_empty_comment COMMENT "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
SHOW CREATE LIBRARY library_with_empty_comment;
--echo # BUG#37563225 : An empty comment is ignored.
ALTER LIBRARY library_with_empty_comment COMMENT '';
SHOW CREATE LIBRARY library_with_empty_comment;
ALTER LIBRARY library_with_extra_longcomment COMMENT "";
SHOW CREATE LIBRARY library_with_extra_longcomment;
ALTER LIBRARY library_without_comment COMMENT "added comment";
SHOW CREATE LIBRARY library_without_comment;
--ERROR ER_PARSE_ERROR
ALTER LIBRARY library_without_comment LANGUAGE JavaScript;
--ERROR ER_PARSE_ERROR
ALTER LIBRARY library_without_comment AS " export function f(n) { return n+2 }";
--ERROR ER_PARSE_ERROR
ALTER LIBRARY library_without_comment;
SHOW CREATE LIBRARY library_without_comment;
--ERROR ER_SP_DOES_NOT_EXIST
ALTER LIBRARY it_should_not_exist COMMENT "updated comment";
--replace_column 5 <modified> 6 <created>
SHOW LIBRARY STATUS WHERE db = 'test';

--echo # Tests for DROP LIBRARY
--error ER_SP_DOES_NOT_EXIST
DROP LIBRARY lib3;

DROP LIBRARY IF EXISTS lib3;

--error ER_PARSE_ERROR
DROP LIBRARY IF NOT EXISTS lib3;

--error ER_TOO_LONG_IDENT
DROP LIBRARY
this_name_is_too_long23456789312345678941234567895123456789612345;

--error ER_PARSE_ERROR
DROP LIBRARY lib1 LANGUAGE JAVASCRIPT;

--error ER_SP_DOES_NOT_EXIST
SHOW CREATE LIBRARY db2.lib1;

--echo # Test CREATE and ALTER FUNCTION/PROCEDURE WITH USING clause
CREATE FUNCTION f(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (lib1 AS lib2)
AS $$ return lib2.f(n) $$;
SHOW CREATE FUNCTION f;
ALTER FUNCTION f USING(lib2);
SHOW CREATE FUNCTION f;
ALTER FUNCTION f USING(lib1, lib2);
SHOW CREATE FUNCTION f;
ALTER FUNCTION f USING(lib2 as lib1, lib1);
SHOW CREATE FUNCTION f;
ALTER FUNCTION f USING(lib1, lib2, lib1 as lib3, lib1);
SHOW CREATE FUNCTION f;
--error ER_SP_DOES_NOT_EXIST
ALTER FUNCTION f USING(lib1, lib2, lib3);
SHOW CREATE FUNCTION f;
--error ER_SP_DOES_NOT_EXIST
ALTER FUNCTION f USING(lib0 as lib2, lib1);
SHOW CREATE FUNCTION f;
DROP FUNCTION f;

CREATE PROCEDURE p(n INTEGER)
USING (lib1, lib2 AS lib3)
LANGUAGE JAVASCRIPT
AS $$ let a = n $$;
SHOW CREATE PROCEDURE p;
ALTER PROCEDURE p USING(lib2);
SHOW CREATE PROCEDURE p;
DROP PROCEDURE p;

CREATE FUNCTION f(n INTEGER) RETURNS INTEGER
USING (lib1 AS lib2)
LANGUAGE JAVASCRIPT
AS $$ return lib2.f(n) $$;
SHOW CREATE FUNCTION f;
DROP FUNCTION f;

--error ER_PARSE_ERROR
CREATE FUNCTION f(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (lib1 AS lib2)
USING (lib3)
AS $$ return lib2.f(n) $$;

--error ER_PARSE_ERROR
CREATE FUNCTION f(n INTEGER) RETURNS INTEGER
USING (lib3)
LANGUAGE JAVASCRIPT
USING (lib3 AS lib1, lib2)
AS $$ return lib2.f(n) $$;

--error ER_PARSE_ERROR
CREATE FUNCTION f(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (lib1 AS lib2)
AS $$ return lib2.f(n) $$
USING (lib3);

--error ER_LIBRARIES_NOT_SUPPORTED
CREATE FUNCTION f(n INTEGER) RETURNS INTEGER LANGUAGE SQL
USING (lib1)
RETURN n;

CREATE PROCEDURE procedure_with_comment() COMMENT "procedure comment" LANGUAGE JAVASCRIPT USING (library_with_comment) AS "let i=2";
SHOW CREATE PROCEDURE procedure_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'procedure_with_comment';
ALTER PROCEDURE procedure_with_comment USING (library_without_comment);
SHOW CREATE PROCEDURE procedure_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'procedure_with_comment';
ALTER PROCEDURE procedure_with_comment USING ();
SHOW CREATE PROCEDURE procedure_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'procedure_with_comment';
ALTER PROCEDURE procedure_with_comment USING (library_without_comment);
SHOW CREATE PROCEDURE procedure_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'procedure_with_comment';
--error ER_SP_DOES_NOT_EXIST
ALTER PROCEDURE procedure_with_comment USING (non_existing_library);
SHOW CREATE PROCEDURE procedure_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'procedure_with_comment';
ALTER PROCEDURE procedure_with_comment COMMENT 'Updated Temporary Procedure Comment';
SHOW CREATE PROCEDURE procedure_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'procedure_with_comment';
ALTER PROCEDURE procedure_with_comment COMMENT 'Updated Procedure Comment' USING (library_with_comment AS library_without_comment);
SHOW CREATE PROCEDURE procedure_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'procedure_with_comment';
create function function_with_comment(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT COMMENT "function comment" USING (library_with_comment AS lib) AS "return lib.f(42)";
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
ALTER FUNCTION function_with_comment USING (library_without_comment lib);
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
ALTER FUNCTION function_with_comment USING ();
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
ALTER FUNCTION function_with_comment COMMENT 'updated comment' USING (library_with_comment AS lib);
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
ALTER FUNCTION function_with_comment USING () COMMENT 'updated comment';
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
--error ER_PARSE_ERROR
ALTER FUNCTION function_with_comment USING (library_with_comment AS lib) COMMENT 'invalid syntax comment' USING();
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
--error ER_PARSE_ERROR
ALTER FUNCTION function_with_comment USING () COMMENT 'invalid inverted syntax comment' USING(library_with_comment AS lib);
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
ALTER FUNCTION function_with_comment USING (library_without_comment lib) COMMENT "temporary comment";
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
--error ER_SP_DOES_NOT_EXIST
ALTER FUNCTION function_with_comment USING (non_existing_library) COMMENT "invalid comment";
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';
ALTER FUNCTION function_with_comment COMMENT "altered function comment";
SHOW CREATE FUNCTION function_with_comment;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
WHERE ROUTINE_SCHEMA = 'test' AND ROUTINE_NAME = 'function_with_comment';

CREATE DATABASE db2;

CREATE LIBRARY db2.lib1 LANGUAGE JAVASCRIPT
AS $$ export function f(n) {return n} $$;

--error ER_SP_ALREADY_EXISTS
CREATE LIBRARY test.lib1 LANGUAGE JAVASCRIPT
AS $$ export function f(n) {return n} $$;

--echo # lib1 should refer to db2.lib1 (function's schema)
CREATE FUNCTION db2.f(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (lib1 AS lib2)
AS $$ return lib2.f(n) $$;
SHOW CREATE FUNCTION db2.f;
DROP FUNCTION db2.f;

--echo # db2.lib2 does not exist
--error ER_SP_DOES_NOT_EXIST
CREATE FUNCTION db2.f(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (lib1 AS lib2, db2.lib2)
AS $$ return lib2.f(n) $$;

USE db2;

CREATE FUNCTION f(n INTEGER) RETURNS INTEGER LANGUAGE JAVASCRIPT
USING (lib1 AS lib2)
AS $$ return lib2.f(n) $$;
SHOW CREATE FUNCTION f;
DROP FUNCTION f;

USE test;

DROP LIBRARY lib1;
DROP LIBRARY test.lib2;
DROP LIBRARY IF EXISTS db2.lib1;
DROP LIBRARY IF EXISTS lib1;
DROP LIBRARY IF EXISTS db2.lib1;
DROP FUNCTION function_with_comment;
DROP PROCEDURE procedure_with_comment;
DROP LIBRARY library_without_comment;
DROP LIBRARY library_with_comment;
DROP LIBRARY library_with_comment2;
DROP LIBRARY library_with_empty_comment;
DROP LIBRARY library_with_extra_longcomment;

--echo # Test use of CREATE/DROP LIBRARY in stored procedures and prepared statements
CREATE PROCEDURE p1() LANGUAGE SQL
CREATE LIBRARY lib LANGUAGE JAVASCRIPT
AS $$ export function f(n) {return n} $$;
CALL p1();
--echo # Verify re-execution behavior of SP
DROP LIBRARY lib;
CALL p1();
--error ER_SP_ALREADY_EXISTS
CALL p1();
DROP PROCEDURE p1;

DELIMITER //;
--echo # Test use of ALTER LIBRARY COMMENT as an SP argument.
--error ER_PARSE_ERROR
CREATE PROCEDURE p1(in comment_text varchar(25)) LANGUAGE SQL
BEGIN
  PREPARE stmt FROM 'ALTER LIBRARY lib COMMENT ?';
  EXECUTE stmt USING comment_text;
END //
DELIMITER ;//

CREATE PROCEDURE p2() LANGUAGE SQL
ALTER LIBRARY lib COMMENT 'Comment from p2()';
CALL p2();
CREATE PROCEDURE p3() LANGUAGE SQL
SHOW CREATE LIBRARY lib;
--replace_column 5 <modified> 6 <created>
SHOW LIBRARY STATUS WHERE name = 'lib';
CALL p3();
CREATE PROCEDURE p4() LANGUAGE SQL
DROP LIBRARY lib;
CALL p4();
DROP PROCEDURE p2;
DROP PROCEDURE p3;
DROP PROCEDURE p4;

PREPARE stmt1 FROM 'CREATE LIBRARY lib LANGUAGE JAVASCRIPT AS $$ export function f(n) {return n} $$';
EXECUTE stmt1;
DROP LIBRARY lib;
EXECUTE stmt1;
--error ER_SP_ALREADY_EXISTS
EXECUTE stmt1;
PREPARE stmt2 FROM 'ALTER LIBRARY lib COMMENT "Comment from stmt2"';
EXECUTE stmt2;
PREPARE stmt3 FROM 'SHOW CREATE LIBRARY lib';
EXECUTE stmt3;
PREPARE stmt4 FROM 'DROP LIBRARY lib';
EXECUTE stmt4;
DROP PREPARE stmt1;
DROP PREPARE stmt2;
DROP PREPARE stmt3;
DROP PREPARE stmt4;

DELIMITER //;
CREATE PROCEDURE p1() LANGUAGE SQL
BEGIN
  PREPARE stmt1 FROM 'CREATE LIBRARY lib LANGUAGE JAVASCRIPT AS $$ export function f(n) {return n} $$';
  PREPARE stmt2 FROM 'ALTER LIBRARY lib COMMENT "Comment from p1.stmt2()"';
  PREPARE stmt3 FROM 'SHOW CREATE LIBRARY lib';
  PREPARE stmt4 FROM 'DROP LIBRARY lib';
  EXECUTE stmt1;
  EXECUTE stmt2;
  EXECUTE stmt3;
  EXECUTE stmt4;
END //

CALL p1()//

DROP PREPARE stmt1//
DROP PREPARE stmt2//
DROP PREPARE stmt3//
DROP PREPARE stmt4//
DROP PROCEDURE p1//

CREATE PROCEDURE p1() LANGUAGE SQL
BEGIN
  PREPARE stmt1 FROM 'CREATE LIBRARY lib LANGUAGE JAVASCRIPT AS $$ export function f(n) {return n} $$';
  PREPARE stmt2 FROM 'ALTER LIBRARY lib COMMENT "p1.stmt2 again"';
  PREPARE stmt3 FROM 'SHOW CREATE LIBRARY lib';
  PREPARE stmt4 FROM 'DROP LIBRARY lib';
  EXECUTE stmt1;
  EXECUTE stmt2;
  EXECUTE stmt3;
END//

CALL p1()//
EXECUTE stmt4//

DROP PREPARE stmt1//
DROP PREPARE stmt2//
DROP PREPARE stmt3//
DROP PREPARE stmt4//
DROP PROCEDURE p1//

CREATE PROCEDURE p1() LANGUAGE SQL
BEGIN
  PREPARE stmt1 FROM 'CREATE LIBRARY lib LANGUAGE JAVASCRIPT COMMENT "Created Comment" AS $$ export function f(n) {return n} $$';
  PREPARE stmt2 FROM 'ALTER LIBRARY lib COMMENT "Comment Changed"';
  PREPARE stmt3 FROM 'SHOW CREATE LIBRARY lib';
  PREPARE stmt4 FROM 'DROP LIBRARY lib';
  EXECUTE stmt1;
  EXECUTE stmt2;
  EXECUTE stmt3;
END//

CALL p1()//
EXECUTE stmt4//

DROP PREPARE stmt1//
DROP PREPARE stmt2//
DROP PREPARE stmt3//
DROP PREPARE stmt4//
DROP PROCEDURE p1//

CREATE PROCEDURE p1() LANGUAGE SQL
BEGIN
  PREPARE stmt1 FROM 'CREATE LIBRARY lib LANGUAGE JAVASCRIPT AS $$ export function f(n) {return n} $$';
  PREPARE stmt2 FROM 'DROP LIBRARY lib';
  EXECUTE stmt1;
  EXECUTE stmt2;
  DROP PREPARE stmt1;
  DROP PREPARE stmt2;
END//

CALL p1()//
--echo # Verify re-execution behavior of SP
CALL p1()//
CALL p1()//
DROP PROCEDURE p1//

CREATE PROCEDURE p1()
BEGIN
  PREPARE create_stmt FROM 'CREATE LIBRARY js_lib LANGUAGE JAVASCRIPT AS $$ export function foo() {return -1} $$;';
  EXECUTE create_stmt;
  DEALLOCATE PREPARE create_stmt;
  PREPARE alter_stmt FROM 'ALTER LIBRARY js_lib COMMENT "DEALLOCATED comment";';
  EXECUTE alter_stmt;
  DEALLOCATE PREPARE alter_stmt;
END//
CALL p1()//
--echo # Verify re-execution behavior of SP
DROP LIBRARY js_lib//
CALL p1()//
--error ER_SP_ALREADY_EXISTS
CALL p1()//
DROP PROCEDURE p1//
SHOW CREATE LIBRARY js_lib//

CREATE PROCEDURE p1()
BEGIN
  PREPARE drop_stmt FROM 'DROP LIBRARY js_lib;';
  EXECUTE drop_stmt;
  DEALLOCATE PREPARE drop_stmt;
END//
CALL p1()//
DROP PROCEDURE p1//

DELIMITER ;//

--echo # Check what happens if another database than current DB is used.
CREATE PROCEDURE db2.p1() LANGUAGE SQL
CREATE LIBRARY lib LANGUAGE JAVASCRIPT
AS $$ export function f(n) {return n} $$;
CALL db2.p1();
CREATE PROCEDURE db2.p2() LANGUAGE SQL
ALTER LIBRARY lib COMMENT 'another DB';
CALL db2.p2();
CREATE PROCEDURE db2.p3() LANGUAGE SQL
SHOW CREATE LIBRARY lib;
CALL db2.p3();
CREATE PROCEDURE p3() LANGUAGE SQL
DROP LIBRARY db2.lib;
CALL p3();
DROP PROCEDURE db2.p1;
DROP PROCEDURE p3;

DROP DATABASE db2;

--echo # extra long library name
CREATE DATABASE schema_891123456789212345678931234567894123456789512345678961234;
USE schema_891123456789212345678931234567894123456789512345678961234;
CREATE LIBRARY library_91123456789212345678931234567894123456789512345678961234 LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';

USE test;

CREATE FUNCTION
function_1123456789212345678931234567894123456789512345678961234()
RETURNS INT LANGUAGE JAVASCRIPT
USING (schema_891123456789212345678931234567894123456789512345678961234.library_91123456789212345678931234567894123456789512345678961234 AS alias_7891123456789212345678931234567894123456789512345678961234)
AS $$ return 42 $$;
SHOW CREATE FUNCTION function_1123456789212345678931234567894123456789512345678961234;
DROP FUNCTION function_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_891123456789212345678931234567894123456789512345678961234;

CREATE DATABASE schema_1_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_2_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_3_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_4_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_5_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_6_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_7_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_8_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_9_1123456789212345678931234567894123456789512345678961234;
CREATE DATABASE schema_0_1123456789212345678931234567894123456789512345678961234;
CREATE LIBRARY schema_1_1123456789212345678931234567894123456789512345678961234.library_1_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_2_1123456789212345678931234567894123456789512345678961234.library_2_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_3_1123456789212345678931234567894123456789512345678961234.library_3_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_4_1123456789212345678931234567894123456789512345678961234.library_4_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_5_1123456789212345678931234567894123456789512345678961234.library_5_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_6_1123456789212345678931234567894123456789512345678961234.library_6_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_7_1123456789212345678931234567894123456789512345678961234.library_7_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_8_1123456789212345678931234567894123456789512345678961234.library_8_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_9_1123456789212345678931234567894123456789512345678961234.library_9_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE LIBRARY schema_0_1123456789212345678931234567894123456789512345678961234.library_0_123456789212345678931234567894123456789512345678961234
LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';

--echo # extra long USING clause
CREATE FUNCTION
function_2123456789212345678931234567894123456789512345678961234()
RETURNS INT LANGUAGE JAVASCRIPT
USING (
  schema_1_1123456789212345678931234567894123456789512345678961234.library_1_123456789212345678931234567894123456789512345678961234 AS alias_1_91123456789212345678931234567894123456789512345678961234,
  schema_2_1123456789212345678931234567894123456789512345678961234.library_2_123456789212345678931234567894123456789512345678961234 AS alias_2_91123456789212345678931234567894123456789512345678961234,
  schema_3_1123456789212345678931234567894123456789512345678961234.library_3_123456789212345678931234567894123456789512345678961234 AS alias_3_91123456789212345678931234567894123456789512345678961234,
  schema_4_1123456789212345678931234567894123456789512345678961234.library_4_123456789212345678931234567894123456789512345678961234 AS alias_4_91123456789212345678931234567894123456789512345678961234,
  schema_5_1123456789212345678931234567894123456789512345678961234.library_5_123456789212345678931234567894123456789512345678961234 AS alias_5_91123456789212345678931234567894123456789512345678961234,
  schema_6_1123456789212345678931234567894123456789512345678961234.library_6_123456789212345678931234567894123456789512345678961234 AS alias_6_91123456789212345678931234567894123456789512345678961234,
  schema_7_1123456789212345678931234567894123456789512345678961234.library_7_123456789212345678931234567894123456789512345678961234 AS alias_7_91123456789212345678931234567894123456789512345678961234,
  schema_8_1123456789212345678931234567894123456789512345678961234.library_8_123456789212345678931234567894123456789512345678961234 AS alias_8_91123456789212345678931234567894123456789512345678961234,
  schema_9_1123456789212345678931234567894123456789512345678961234.library_9_123456789212345678931234567894123456789512345678961234 AS alias_9_91123456789212345678931234567894123456789512345678961234,
  schema_0_1123456789212345678931234567894123456789512345678961234.library_0_123456789212345678931234567894123456789512345678961234 AS alias_0_91123456789212345678931234567894123456789512345678961234
) AS $$ return 42 $$;
SHOW CREATE FUNCTION function_2123456789212345678931234567894123456789512345678961234;
SELECT * FROM INFORMATION_SCHEMA.ROUTINE_LIBRARIES
ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE, LIBRARY_CATALOG, LIBRARY_SCHEMA, LIBRARY_NAME, LIBRARY_VERSION;
DROP FUNCTION function_2123456789212345678931234567894123456789512345678961234;

--echo # Orphaned SP where one of its libraries is deleted.
CREATE LIBRARY deleted_library LANGUAGE JAVASCRIPT
AS 'export function f(n) {return n}';
CREATE FUNCTION orphaned_function() RETURNS INT LANGUAGE JAVASCRIPT USING (deleted_library)
AS 'return deleted_library.f(42)';
DROP LIBRARY deleted_library;
SHOW CREATE FUNCTION orphaned_function;
SHOW WARNINGS;
DROP FUNCTION orphaned_function;

DROP DATABASE schema_1_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_2_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_3_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_4_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_5_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_6_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_7_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_8_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_9_1123456789212345678931234567894123456789512345678961234;
DROP DATABASE schema_0_1123456789212345678931234567894123456789512345678961234;

SHOW STATUS LIKE 'Com%library';
