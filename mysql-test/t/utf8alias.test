# WL#16779 User controlled aliasing for 'utf8'

--echo ################################################################
--echo # Verify that "utf8" maps to "utf8mb3" for clients.
--echo ################################################################

SET @@sql_mode=REPLACE(@@sql_mode,'INTERPRET_UTF8_AS_UTF8MB4','');

--exec $MYSQL -e "SELECT @@character_set_client" 2>&1
--exec $MYSQL --default-character-set=utf8 -e "SELECT @@character_set_client" 2>&1

SET @@sql_mode=CONCAT(@@sql_mode,",","INTERPRET_UTF8_AS_UTF8MB4");

--exec $MYSQL -e "SELECT @@character_set_client" 2>&1
--exec $MYSQL --default-character-set=utf8 -e "SELECT @@character_set_client" 2>&1

SET @@sql_mode=default;

--echo ################################################################
--echo # Verify that character set introducers and COLLATE respect SQL_MODE.
--echo ################################################################

SET @@sql_mode=REPLACE(@@sql_mode,'INTERPRET_UTF8_AS_UTF8MB4','');

--error ER_INVALID_CHARACTER_STRING
SELECT _utf8 x'F09F8DA3' AS result;

--disable_warnings
EXPLAIN
SELECT _utf8 'ß' COLLATE utf8_german2_ci = _utf8 'ss' COLLATE utf8_german2_ci;
--enable_warnings

--echo
SET @@sql_mode=CONCAT(@@sql_mode,",","INTERPRET_UTF8_AS_UTF8MB4");

SELECT _utf8  x'F09F8DA3' AS result;
EXPLAIN SELECT _utf8 x'F09F8DA3';
EXPLAIN SELECT _utf8 "スシ";

EXPLAIN
SELECT _utf8 'ß' COLLATE utf8_german2_ci = _utf8 'ss' COLLATE utf8_german2_ci;

--echo
SET @@sql_mode=default;

--echo ################################################################
--echo # Verify that functional indexes respect SQL_MODE.
--echo ################################################################

SET @@sql_mode=REPLACE(@@sql_mode,'INTERPRET_UTF8_AS_UTF8MB4','');

--error ER_COLLATION_CHARSET_MISMATCH
CREATE TABLE
t1(f1 JSON, INDEX idx1 ((CAST(f1->>"$.name" AS CHAR(30)) COLLATE utf8_bin)));

--disable_warnings
CREATE TABLE
t1(f1 JSON,
   INDEX idx1 ((CAST(f1->>"$.name" AS CHAR(30) CHARSET utf8) COLLATE utf8_bin)));
--enable_warnings
SHOW CREATE TABLE t1;
DROP TABLE t1;

SET @@sql_mode=CONCAT(@@sql_mode,",","INTERPRET_UTF8_AS_UTF8MB4");

CREATE TABLE
t1(f1 JSON, INDEX idx1 ((CAST(f1->>"$.name" AS CHAR(30)) COLLATE utf8_bin)));
SHOW CREATE TABLE t1;
DROP TABLE t1;

CREATE TABLE
t1(f1 JSON,
   INDEX idx1 ((CAST(f1->>"$.name" AS CHAR(30) CHARSET utf8) COLLATE utf8_bin)));
SHOW CREATE TABLE t1;
DROP TABLE t1;

SET @@sql_mode=default;

--echo ################################################################
--echo # Verify that a TABLE created by a PROCEDURE will get charset/collation
--echo # determined by SQL_MODE when the PROCEDURE was created.
--echo ################################################################

SET @@sql_mode=REPLACE(@@sql_mode,'INTERPRET_UTF8_AS_UTF8MB4','');

--disable_warnings
delimiter $;
CREATE PROCEDURE p2()
BEGIN
  CREATE TABLE t2(a CHAR(1)) CHARACTER SET utf8;
END $
delimiter ;$
--enable_warnings

SET @@sql_mode=CONCAT(@@sql_mode,",","INTERPRET_UTF8_AS_UTF8MB4");

CALL p2();

DROP PROCEDURE p2;
# t2 should have  DEFAULT CHARSET=utf8mb3
SHOW CREATE TABLE t2;
DROP TABLE t2;

delimiter $;
CREATE PROCEDURE p2()
BEGIN
  CREATE TABLE t2(a CHAR(1)) CHARACTER SET utf8;
END $
delimiter ;$

SET @@sql_mode=REPLACE(@@sql_mode,'INTERPRET_UTF8_AS_UTF8MB4','');

CALL p2();

DROP PROCEDURE p2;
# t2 should have DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t2;
DROP TABLE t2;

SET @@sql_mode=default;

--echo ################################################################
--echo # Verify that PROCEDURE body is stored verbatim, with no utf8 substitutions.
--echo ################################################################

SET @@sql_mode=REPLACE(@@sql_mode,'INTERPRET_UTF8_AS_UTF8MB4','');

--disable_warnings
delimiter $;
CREATE PROCEDURE p2(in a TEXT CHARSET utf8)
BEGIN
  DECLARE str text CHARSET utf8;
  SET str := a;
  CREATE TABLE t2(a CHAR(1)) CHARACTER SET utf8;
END $
delimiter ;$
--enable_warnings

SET @@sql_mode=CONCAT(@@sql_mode,",","INTERPRET_UTF8_AS_UTF8MB4");

--disable_warnings
CALL p2('s s s s s s');
--enable_warnings
SHOW CREATE PROCEDURE p2;


SELECT ROUTINE_DEFINITION FROM INFORMATION_SCHEMA.ROUTINES WHERE SPECIFIC_NAME = "p2";

DROP PROCEDURE p2;
SHOW CREATE TABLE t2;
DROP TABLE t2;

delimiter $;
CREATE PROCEDURE p2(in a TEXT CHARSET utf8)
BEGIN
  DECLARE str text CHARSET utf8;
  SET str := a;
  CREATE TABLE t2(a CHAR(1)) CHARACTER SET utf8;
END $
delimiter ;$

SET @@sql_mode=REPLACE(@@sql_mode,'INTERPRET_UTF8_AS_UTF8MB4','');

CALL p2('s s s s s s');
SHOW CREATE PROCEDURE p2;

SELECT ROUTINE_DEFINITION FROM INFORMATION_SCHEMA.ROUTINES WHERE SPECIFIC_NAME = "p2";

DROP PROCEDURE p2;
SHOW CREATE TABLE t2;
DROP TABLE t2;

SET @@sql_mode=default;

--echo ################################################################
--echo # Verify that TABLE definitions will respect SQL_MODE.
--echo ################################################################

SET @@sql_mode=REPLACE(@@sql_mode,'INTERPRET_UTF8_AS_UTF8MB4','');

--disable_warnings
CREATE TABLE t1(
  a CHAR(42) CHARACTER SET UTF8,
  b CHAR(42) CHARACTER SET UTF8
  GENERATED ALWAYS AS (regexp_like(a, _UTF8'^[a-z0-9-]+$')) VIRTUAL
  );
--enable_warnings

SHOW CREATE TABLE t1;
DROP TABLE t1;

--disable_warnings
CREATE TABLE t1(
  a CHAR(42) CHARACTER SET utf8 collate utf8_tolower_ci,
  b CHAR(42) CHARACTER SET utf8
  GENERATED ALWAYS AS (regexp_like(a, _utf8'^[a-z0-9-]+$')) VIRTUAL
  );
--enable_warnings

SHOW CREATE TABLE t1;
DROP TABLE t1;

--disable_warnings
CREATE TABLE t1(
  a CHAR(42) CHARACTER SET utf8 collate utf8_general_mysql500_ci,
  b CHAR(42) CHARACTER SET utf8
  GENERATED ALWAYS AS (regexp_like(a, _utf8'^[a-z0-9-]+$')) VIRTUAL
  );
--enable_warnings

SHOW CREATE TABLE t1;
DROP TABLE t1;

--error ER_INVALID_CHARACTER_STRING
CREATE TABLE t1(
  a CHAR(42) CHARACTER SET UTF8 DEFAULT _utf8 x'F09F8DA3'
  );

--disable_warnings
CREATE TABLE t1(
  a CHAR(42) CHARACTER SET UTF8 DEFAULT _utf8 '\u00DF'
  );
--enable_warnings

SHOW CREATE TABLE t1;
DROP TABLE t1;

SET @@sql_mode=CONCAT(@@sql_mode,",","INTERPRET_UTF8_AS_UTF8MB4");

CREATE TABLE t1(
  a CHAR(42) CHARACTER SET UTF8,
  b CHAR(42) CHARACTER SET UTF8
  GENERATED ALWAYS AS (regexp_like(a, _UTF8'^[a-z0-9-]+$')) VIRTUAL
  );

SHOW CREATE TABLE t1;
DROP TABLE t1;

--error ER_UNKNOWN_COLLATION
CREATE TABLE t1(
  a CHAR(42) CHARACTER SET UTF8 COLLATE UTF8_TOLOWER_CI,
  b CHAR(42) CHARACTER SET UTF8
  GENERATED ALWAYS AS (regexp_like(a, _UTF8'^[a-z0-9-]+$')) VIRTUAL
  );
SHOW WARNINGS;

--error ER_UNKNOWN_COLLATION
CREATE TABLE t1(
  a CHAR(42) CHARACTER SET utf8 collate UTF8_GENERAL_MYSQL500_CI,
  b CHAR(42) CHARACTER SET utf8
  GENERATED ALWAYS AS (regexp_like(a, _utf8'^[a-z0-9-]+$')) VIRTUAL
  );
SHOW WARNINGS;

CREATE TABLE t1(
  a CHAR(42) CHARACTER SET UTF8 DEFAULT _utf8 x'F09F8DA3'
  );

SHOW CREATE TABLE t1;
DROP TABLE t1;

SET @@sql_mode=default;
