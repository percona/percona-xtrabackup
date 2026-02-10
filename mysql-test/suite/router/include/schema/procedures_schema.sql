# -----------------------------------------------------
# Schema proc_schema
# -----------------------------------------------------
# Create schema that contains various stored procedures
--disable_query_log
--disable_result_log
DROP SCHEMA IF EXISTS `proc_schema` ;

--let $router_test_schemas=proc_schema;$router_test_schemas
CREATE SCHEMA IF NOT EXISTS `proc_schema`;
USE `proc_schema`;


CREATE TABLE `proc_schema`.`dummy_data` (
  `id` INTEGER NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(255) NOT NULL,
  `comments` VARCHAR(512) NULL,
  `date` DATETIME NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE INDEX `name_UNIQUE` (`name` ASC) VISIBLE);
  
INSERT INTO `proc_schema`.`dummy_data` (`name`, `comments`, `date`)
  VALUES("First row", "First comment", "1977-01-21"),
  ("Second row", NULL, "1977-01-21"),
  ("Thrid row", NULL, "1977-02-21"),
  ("4th row", "This row was inserted as forth row.", "1977-01-21"),
  ("5th row", "", "2021-03-01"),
  ("6th row", "", "2023-01-21"),
  ("7th row", "Not empty string", "2023-02-21"),
  ("8th row", "Next entry", "2023-03-21"),
  ("9th row", "", "2023-04-21"),
  ("10th row", "...", "2023-05-21"),
  ("11th row", "...", "2023-06-21"),
  ("12th row", "...", "2023-07-21"),
  ("13th row", "...", "2023-08-21"),
  ("14th row", "this is fourteenth row", "2023-09-21"),
  ("15th row", "...", "2023-10-21"),
  ("16th row", "New entry in this month", "2023-11-02"),
  ("17th row", "Second in this month", "2023-11-04"),
  ("18th row", "Next one", "2023-11-05"),
  ("19th row", "...", "2023-11-06"),
  ("20th row", "...", "2023-11-07"),
  ("21th row", "New customer", "2023-11-08"),
  ("22th row", "...", "2023-11-09"),
  ("23th row", "...", "2023-11-10"),
  ("24th row", "...", "2023-11-11"),
  ("25th row", "...", "2023-11-12"),
  ("26th row", "...", "2023-11-12"),
  ("27th row", "...", "2023-11-12");

DROP procedure IF EXISTS `on_resultset`;

DELIMITER $$;

CREATE PROCEDURE `proc_schema`.`procedure_first` ()
BEGIN
   select "first content";
END;$$

CREATE PROCEDURE `proc_schema`.`procedure_second` ()
BEGIN
   select "second content";
END;$$

CREATE PROCEDURE `proc_schema`.`one_resultset` ()
BEGIN
   select * from `proc_schema`.`dummy_data`;
END;$$

CREATE PROCEDURE `proc_schema`.`two_resultsets` ()
BEGIN
   select * from `proc_schema`.`dummy_data` WHERE id > 1 and id < 5;
   select count(*) from `proc_schema`.`dummy_data`;
END;$$

CREATE PROCEDURE `proc_schema`.`three_resultsets` ()
BEGIN
    select min(id) from `proc_schema`.`dummy_data`;
    select max(id) from `proc_schema`.`dummy_data`;
    select * from `proc_schema`.`dummy_data` where id=(select max(id) from `proc_schema`.`dummy_data`);
END;$$

CREATE PROCEDURE `proc_schema`.`mix` (INOUT a integer, INOUT b integer, OUT c integer)
BEGIN
    select min(id) from `proc_schema`.`dummy_data`;
    select max(id) from `proc_schema`.`dummy_data`;
    select * from `proc_schema`.`dummy_data` where id=(select max(id) from `proc_schema`.`dummy_data`);
    SET c = 10;
END;$$

CREATE PROCEDURE `proc_schema`.`no_resultset_out_param` (OUT v integer)
BEGIN
    SET v=(select min(id) from `proc_schema`.`dummy_data`);
END;$$

CREATE PROCEDURE `proc_schema`.`one_resultset_out_param` (OUT v integer)
BEGIN
    select * from `proc_schema`.`dummy_data` where id  = (select max(id) from `proc_schema`.`dummy_data`);
    SET v=(select min(id) from `proc_schema`.`dummy_data`);
END;$$

CREATE PROCEDURE `proc_schema`.`proc_do_nothing` ()
BEGIN
END;$$


CREATE PROCEDURE `proc_schema`.`proc_sum` (a integer, b integer)
BEGIN
  SELECT a + b as result;
END;$$

CREATE PROCEDURE `proc_schema`.`proc_concat` (a TEXT, b TEXT)
BEGIN
  SELECT CONCAT(a,b) as result;
END;$$

CREATE PROCEDURE `proc_schema`.`proc_sum_out` (a integer, b integer, OUT result INTEGER)
BEGIN
  SET result = (SELECT a + b as result);
END;$$

CREATE PROCEDURE `proc_schema`.`proc_concat_out` (a TEXT, b TEXT, OUT result TEXT)
BEGIN
  SET result = (SELECT CONCAT(a,b) as result);
END;$$

CREATE PROCEDURE `proc_schema`.`move_char` (a VARCHAR(20), OUT result VARCHAR(20))
BEGIN
  SET result = CONCAT(a," appended");
END;$$

CREATE PROCEDURE `proc_schema`.`move_date` (a DATE, OUT result DATE)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_year` (a YEAR, OUT result YEAR)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_time` (a TIME, OUT result TIME)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_bit` (a BIT(1), OUT result BIT(1))
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_tinyint1` (a TINYINT(1), OUT result TINYINT(1))
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_tinyint8` (a TINYINT(8), OUT result TINYINT(8))
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_decimal` (a DECIMAL, OUT result DECIMAL)
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_float` (a FLOAT, OUT result FLOAT)
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_double` (a DOUBLE, OUT result DOUBLE)
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_vector` (a VECTOR, OUT result VECTOR)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_geometry` (a GEOMETRY, OUT result GEOMETRY)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_blob` (a BLOB, OUT result BLOB)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_timestamp` (a TIMESTAMP, OUT result TIMESTAMP)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_json` (a JSON, OUT result JSON)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`resultset_vector` (a VECTOR)
BEGIN
  SELECT a;
END;$$





CREATE PROCEDURE `proc_schema`.`inc_enum` (INOUT result enum('one','two','three'))
BEGIN
  SELECT result + 1 into result;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_set` (INOUT result SET('one','two'))
BEGIN
  SELECT result | 2 into result;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_char` (INOUT result CHAR(20) CHARACTER SET utf8 COLLATE utf8_general_ci)
BEGIN
  SET result = CONCAT(result," a");
END;$$


CREATE PROCEDURE `proc_schema`.`inc_text` (INOUT result TEXT CHARACTER SET utf8 COLLATE utf8_general_ci)
BEGIN
  SELECT CONCAT(result,"B") into result;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_ttext` (INOUT result TINYTEXT CHARACTER SET utf8 COLLATE utf8_general_ci)
BEGIN
  SELECT CONCAT(result,"B") into result;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_int` (INOUT result integer)
BEGIN
  SET result = (SELECT  result + 1);
END;$$

CREATE PROCEDURE `proc_schema`.`inc_varchar` (INOUT result VARCHAR(20))
BEGIN
  SET result = CONCAT(result," a");
END;$$

CREATE PROCEDURE `proc_schema`.`inc_date` (INOUT result DATE)
BEGIN
  SET result = DATE_ADD(result, INTERVAL 10 DAY);
END;$$

CREATE PROCEDURE `proc_schema`.`inc_year` (INOUT result YEAR)
BEGIN
  SET result = result +  10;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_time` (INOUT result TIME)
BEGIN
  SET result = DATE_ADD(result, INTERVAL 10 HOUR);
END;$$

CREATE PROCEDURE `proc_schema`.`inc_bit` (INOUT result BIT(1))
BEGIN
  SET result = IF(result,0,1);
END;$$

CREATE PROCEDURE `proc_schema`.`inc_tinyint1` (INOUT result TINYINT(1))
BEGIN
  SET result = result+1;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_tinyint8` (INOUT result TINYINT(8))
BEGIN
  SET result = result+1;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_decimal` (INOUT result DECIMAL)
BEGIN
  SET result = result+1;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_float` (INOUT result FLOAT)
BEGIN
  SET result = result+1;
END;$$

CREATE PROCEDURE `proc_schema`.`inc_double` (INOUT result DOUBLE)
BEGIN
  SET result = result+1;
END;$$

CREATE PROCEDURE `proc_schema`.`set_vector` (INOUT result VECTOR)
BEGIN
  SET result = STRING_TO_VECTOR(REPLACE(JSON_MERGE(CAST(CONVERT(VECTOR_TO_STRING(result) using utf8) as JSON) , "[1,2,3]")," ",""));
END;$$


CREATE PROCEDURE `proc_schema`.`report_back_mysql_error1` (mysql_error INTEGER)
BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'This stored procedure signaled an error.', MYSQL_ERRNO = mysql_error;
END;$$

CREATE PROCEDURE `proc_schema`.`report_back_mysql_error2` (mysql_error INTEGER, mysql_message TEXT)
BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = mysql_message, MYSQL_ERRNO = mysql_error;
END;$$

CREATE PROCEDURE `proc_schema`.`report_back_mysql_error_if`(IN error_out BOOLEAN)
BEGIN
    SELECT 1;
    IF (error_out = 1) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'This stored procedured signaled an error.', MYSQL_ERRNO = 5511;
    ELSE
        SELECT 2;
    END IF;
END;$$

CREATE PROCEDURE `proc_schema`.`insert_data`(name VARCHAR(255),
                                             comments VARCHAR(512),
                                             date DATETIME)
BEGIN
  INSERT INTO `proc_schema`.`dummy_data` (`name`, `comments`, `date`)
    VALUES(name, comments, date);
END;$$

CREATE PROCEDURE `proc_schema`.`insert_and_return` (name VARCHAR(255),
                                             comments VARCHAR(512),
                                             date DATETIME)
BEGIN
  INSERT INTO `proc_schema`.`dummy_data` (`name`, `comments`, `date`)
    VALUES(name, comments, date);

  select min(id) from `proc_schema`.`dummy_data`;
END;$$

CREATE PROCEDURE `proc_schema`.`insert_no_param` ()
BEGIN
  INSERT INTO `proc_schema`.`dummy_data` (`name`, `comments`, `date`)
    VALUES("Mike", "comments", NOW());

  select min(id) from `proc_schema`.`dummy_data`;
END;$$

CREATE PROCEDURE `proc_schema`.`in_out_and_return` (IN name VARCHAR(255),
                                             OUT v integer)
BEGIN
  INSERT INTO `proc_schema`.`dummy_data` (`name`)
    VALUES(name);

  SET v=(select min(id) from `proc_schema`.`dummy_data`);
  select min(id) from `proc_schema`.`dummy_data`;
END;$$

CREATE PROCEDURE `proc_schema`.`string_in` (IN name VARCHAR(255))
BEGIN
  SELECT 1;
END;$$

CREATE PROCEDURE `proc_schema`.`signal_warning` ()
BEGIN
  SIGNAL SQLSTATE '01000' SET MESSAGE_TEXT = 'Some warning';
END;$$

CREATE PROCEDURE `proc_schema`.`signal_no_data` ()
BEGIN
  SIGNAL SQLSTATE '02000' SET MESSAGE_TEXT = 'Some warning';
END;$$

CREATE PROCEDURE `proc_schema`.`signal_exception` ()
BEGIN
  SIGNAL SQLSTATE '03000' SET MESSAGE_TEXT = 'Some warning';
END;$$

--enable_query_log
--enable_result_log

--echo # DB `proc_schema` - created

DELIMITER ;$$
