#WL#16623 - This file adds test for ETAG() Support in MySQL

CREATE TABLE t1 (
  id INT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(255) NOT NULL,
  age BIGINT UNSIGNED,
  height DECIMAL(5,2),
  rand DECIMAL(65,30),
  weight DOUBLE,
  gender ENUM('male', 'female', 'other'),
  birthday DATE,
  email VARCHAR(255) UNIQUE,
  phone CHAR(10),
  address TEXT,
  bio BLOB,
  image LONGBLOB,
  created_at TIMESTAMP DEFAULT "2023-06-27 15:53:33",
  updated_at DATETIME DEFAULT "2023-06-27 15:53:33",
  secret BINARY(16),
  password VARBINARY(255),
  verified BOOL,
  login_time TIME,
  graduation_year YEAR,
  info JSON,
  jsontext TEXT,
  jsonpath VARCHAR(255)
) secondary_engine RAPID;

INSERT INTO t1 (name, age, height, weight, gender, birthday, email, phone, address, bio, image, secret, password, verified, login_time, graduation_year, info, rand)
VALUES
('Alice', 18446744073709551615, 999.99, 1.7976931348623157E+308, 'female', '1998-01-01', 'alice@example.com', '1234567890', '123 Main Street', NULL, NULL, 0x616C69636570, b'00000001', TRUE, '09:00:00', 2020, '{"x": 17, "x": "red", "x": [3, 5, 7]}', 99999999999999999999999999999.9999999999999999999999999999),
('Bob', 2, 180.3, 75.4, 'male', '1993-02-02', 'bob@example.com', '0987654321', '456 Second Avenue', NULL, NULL,  0x626F6270, b'00000010', FALSE, '10:00:00', 2019, '{"oracle": "oracle is a database company"}', 1000.3000),
('Charlie', 3, 175.6, 70.3, 'male', '1988-03-03', 'charlie@example.com', '1357924680', '789 Third Boulevard',  NULL, NULL, 0x636861726C70 , b'00000100', TRUE,'11:00:00', 2018, '{"key": "value", "key2": "value2"}', 1000.3000),
('Diana', 4 ,170.4 ,65.1 ,'female' ,'1983-04-04' ,'diana@example.com' ,'2468013579' ,'101 Fourth Lane' ,NULL ,NULL, 0x6469616E70 , b'00001000' ,FALSE ,'12:00:00' ,2017, '{"key2": "value2", "key": "value"}', 1000.3000),
('Evan', 5 ,185.7 ,80.5 ,'male' ,'1978-05-05' ,'evan@example.com' ,'9753186420' ,'202 Fifth Street' , NULL ,NULL, 0x6576616E70 , b'00010000' ,TRUE ,'13:00:00' ,2016, '{"id": 14, "name": "Aztalan"}', 1000.3000),
#Common
('Alice', 25, 165.5, 55.2, 'female', '1998-01-01', 'alice1@example.com', '1234567890', '123 Main Street', NULL, NULL, 0x616C69636570, b'00000001', TRUE, '09:00:00', 2020, '{"a": 1, "b": 2, "c": [3, 4, 5]}', 1000.3000),
('Bob', 30, 180.3, 75.4, 'male', '1993-02-02', 'bob2@example.com', '0987654321', '456 Second Avenue', NULL, NULL,  0x626F6270, b'00000010', FALSE, '10:00:00', 2019, '{"a": {"b": 1}, "c": {"b": 2}}', 1000.3000),
('Charlie', 35, 175.6, 70.3, 'male', '1988-03-03', 'charlie2@example.com', '1357924680', '789 Third Boulevard',  NULL, NULL, 0x636861726C70 , b'00000100', TRUE,'11:00:00', 2018, '[1, 2, 3, 4, 5]', 1000.3000),
('Diana', 40 ,170.4 ,65.1 ,'female' ,'1983-04-04' ,'diana2@example.com' ,'2468013579' ,'101 Fourth Lane' ,NULL ,NULL, 0x6469616E70 , b'00001000' ,FALSE ,'12:00:00' ,2017, '[1, 2, 3, 4, 5]', 1000.3000),
('Evan', 45 ,185.7 ,80.5 ,'male' ,'1978-05-05' ,'evan2@example.com' ,'9753186420' ,'202 Fifth Street' , NULL ,NULL, 0x6576616E70 , b'00010000' ,TRUE ,'13:00:00' ,2016, '["a", {"b": [true, false]}, [10, 20]]', 1000.3000),
('Alice1', 18446744073709551614, 165.5, 155.2, 'female', '1998-01-02', 'ald2ice@example.com', '1234567891', '122 Main Street', NULL, NULL, 0x616C69636570, b'00000001', TRUE, '09:00:00', 2020, '18446744073709551614', 1000.3000),
('Bob1', 18446744073709551614, 180.3, 175.4, 'male', '1993-02-01', 'bo2db@example.com', '0987654322', '451 Second Avenue', NULL, NULL,  0x626F6270, b'00000010', FALSE, '10:00:00', 2019, '18446744073709551615', 1000.3000),
('Charlie1', 18446744073709551614, 175.6, 70.3, 'male', '1988-03-02', 'chad2rlie@example.com', '1357924683', '781 Third Boulevard',  NULL, NULL, 0x636861726C70 , b'00000100', TRUE,'11:00:00', 2018, '18446744073709551613', 1000.3000),
('Diana1', 18446744073709551614, 110.4 ,65.1 ,'female' ,'1983-04-01' ,'ddid2ana@example.com' ,'2468013574' ,'111 Fourth Lane' ,NULL ,NULL, 0x6469616E70 , b'00001000' ,FALSE ,'12:00:00' ,2017, 'null', 1000.3000),
('Evan1', 145 ,115.7 ,80.5 ,'male' ,'1978-05-23' ,'evda2n@example.com' ,'9753186422' ,'212 Fifth Street' , NULL ,NULL, 0x6576616E70 , b'00010000' ,TRUE ,'13:00:00' ,2016, '{"x": 17, "x": "red", "x": [3, 5, 7]}', 1000.3000),
('Alice2', 225, 165.5, 155.2, 'female', '1998-01-02', 'al2dice@example.com', '1234567891', '122 Main Street', NULL, NULL, 0x616C69636570, b'00000001', TRUE, '09:00:00', 2020, '[3,10,5,17,[22,44,66]]', 1000.3000),
('Bob2', 230, 180.3, 175.4, 'male', '1993-02-01', 'bod2b@example.com', '0987654322', '451 Second Avenue', NULL, NULL,  0x626F6270, b'00000010', FALSE, '10:00:00', 2019, '{"MySQL":"DB"}', 1000.3000),
('Charlie2', 235, 175.6, 70.3, 'male', '1988-03-02', 'chad2drlie@example.com', '1357924683', '781 Third Boulevard',  NULL, NULL, 0x636861726C70 , b'00000100', TRUE,'11:00:00', 2018, '{"x": 17, "x": "red", "x": [3, 5, 7]}', 1000.3000);

SELECT ETAG(id), ETAG(name), ETAG(age), ETAG(height), ETAG(rand), ETAG(weight), ETAG(gender), ETAG(birthday), ETAG(email) from t1;
SELECT ETAG(phone), ETAG(address), ETAG(bio), ETAG(image), ETAG(created_at), ETAG(updated_at), HEX(ETAG(password)) from t1;
SELECT ETAG(verified), ETAG(login_time), ETAG(graduation_year), ETAG(info), ETAG(jsontext) from t1;
--error 1582
SELECT ETAG() from t1;
SELECT ETAG(1) from t1;
SELECT ETAG("constant") from t1;
SELECT id, name, age, height from t1 where ETAG(id,age) >'13647057490375175604';
SELECT ETAG(concat(id, name)) from t1;
SELECT ETAG(id, name) from t1;
SELECT ETAG(info, jsontext) from t1;
SELECT ETAG(id, name, info, jsontext) from t1;
SELECT ETAG(id, name, info) from t1;

DROP TABLE t1;

#
# NULL should return a same ETAG value
#
CREATE TABLE Z (f1 INTEGER DEFAULT NULL, f2 integer DEFAULT NULL, f3 varchar(100) DEFAULT NULL);
INSERT INTO Z(f1) VALUES (1);
INSERT INTO Z(f2) VALUES (100);
INSERT INTO Z(f3) VALUES ('database');
SELECT * from Z;
SELECT ETAG(f2), ETAG(f2,f3), ETAG(f3,f2), ETAG(f2,f2) from Z;
DROP TABLE Z;

CREATE TABLE example_table (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    data JSON
);

#Insert some data into the table
INSERT INTO example_table (data) VALUES ('{"name": "John", "email":
"john.doe@example.com", "age": 30}'); INSERT INTO example_table (data) VALUES
('{"_metadata": "example", "name": "Jane", "email": "jane.doe@example.com",
"age": 25}'); INSERT INTO example_table (data) VALUES ('{"name": "Jane",
"email": "jane.doe@example.com", "age": 25}');
SELECT data, ETAG(data) FROM example_table;
DROP TABLE example_table;

CREATE TABLE t1 (
    -- Numeric types
    tinyint_col TINYINT,
    smallint_col SMALLINT,
    mediumint_col MEDIUMINT,
    int_col INT,
    bigint_col BIGINT,
    decimal_col DECIMAL(10, 2),
    float_col FLOAT,
    double_col DOUBLE,
    bit_col BIT(1),

    -- String types
    char_col CHAR(10),
    varchar_col VARCHAR(255),
    binary_col BINARY(16),
    tinyblob_col TINYBLOB,
    blob_col BLOB,
    mediumblob_col MEDIUMBLOB,
    longblob_col LONGBLOB,
    tinytext_col TINYTEXT,
    text_col TEXT,
    mediumtext_col MEDIUMTEXT,
    longtext_col LONGTEXT,
    enum_col ENUM('value1', 'value2', 'value3'),
    set_col SET('value1', 'value2', 'value3'),

    -- Date and time types
    date_col DATE,
    datetime_col DATETIME,
    timestamp_col TIMESTAMP,
    time_col TIME,
    year_col YEAR,

    -- JSON type
    json_col JSON,

    -- Spatial types
    geometry_col GEOMETRY,
    point_col POINT,
    linestring_col LINESTRING,
    polygon_col POLYGON,
    multipoint_col MULTIPOINT,
    multilinestring_col MULTILINESTRING,
    multipolygon_col MULTIPOLYGON,
    geometrycollection_col GEOMETRYCOLLECTION
);

INSERT INTO t1 (
    tinyint_col, smallint_col, mediumint_col, int_col, bigint_col,
    decimal_col, float_col, double_col, bit_col, char_col, varchar_col,
    binary_col, tinyblob_col, blob_col, mediumblob_col,
    longblob_col, tinytext_col, text_col, mediumtext_col, longtext_col,
    enum_col, set_col, date_col, datetime_col, timestamp_col, time_col, year_col,
    json_col, geometry_col, point_col, linestring_col, polygon_col,
    multipoint_col, multilinestring_col, multipolygon_col, geometrycollection_col
) VALUES (
    1, 123, 12345, 123456, 1234567890123,
    1234.56, 12.34, 12345.6789, b'1', 'A', 'Hello, world!',
    BINARY('binary_data'), 'tiny_blob', 'blob_data', 'medium_blob_data',
    'long_blob_data', 'tiny_text', 'text_data', 'medium_text_data', 'long_text_data',
    'value1', 'value1,value2', '2024-01-01', '2024-01-01 12:34:56', CURRENT_TIMESTAMP, '12:34:56', 2024,
    '{"key": "value"}',
    ST_GeomFromText('POINT(1 1)'),
    ST_GeomFromText('POINT(1 1)'),
    ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)'),
    ST_GeomFromText('POLYGON((0 0, 1 1, 1 0, 0 0))'),
    ST_GeomFromText('MULTIPOINT((0 0), (1 1))'),
    ST_GeomFromText('MULTILINESTRING((0 0, 1 1), (2 2, 3 3))'),
    ST_GeomFromText('MULTIPOLYGON(((0 0, 1 1, 1 0, 0 0)), ((2 2, 3 3, 3 2, 2 2)))'),
    ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(0 0, 1 1))')
);

INSERT INTO t1 (
    tinyint_col, smallint_col, mediumint_col, int_col, bigint_col,
    decimal_col, float_col, double_col, bit_col, char_col, varchar_col,
    binary_col, tinyblob_col, blob_col, mediumblob_col,
    longblob_col, tinytext_col, text_col, mediumtext_col, longtext_col,
    enum_col, set_col, date_col, datetime_col, timestamp_col, time_col, year_col,
    json_col, geometry_col, point_col, linestring_col, polygon_col,
    multipoint_col, multilinestring_col, multipolygon_col, geometrycollection_col
) VALUES (
    2, 123, 12345, 123456, 1234567890123,
    1234.56, 1234.567777777777777777, 1234.567777777777777777, b'1', 'A', 'Hello, world!',
    BINARY('binary_data'), 'tiny_blob', 'blob_data', 'medium_blob_data',
    'long_blob_data', 'tiny_text', 'text_data', 'medium_text_data', 'long_text_data',
    'value1', 'value1,value2', '2024-01-01', '2024-01-01 12:34:56', CURRENT_TIMESTAMP, '12:34:56', 2024,
    '{"key": "value"}', ST_GeomFromText('POINT(1 1)'), ST_GeomFromText('POINT(1 1)'), ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)'),
    ST_GeomFromText('POLYGON((0 0, 1 1, 1 0, 0 0))'), ST_GeomFromText('MULTIPOINT((0 0), (1 1))'),
    ST_GeomFromText('MULTILINESTRING((0 0, 1 1), (2 2, 3 3))'),
    ST_GeomFromText('MULTIPOLYGON(((0 0, 1 1, 1 0, 0 0)), ((2 2, 3 3, 3 2, 2 2)))'),
    ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(0 0, 1 1))')
);

SELECT 
    ETAG(tinyint_col),
    ETAG(smallint_col),
    ETAG(mediumint_col),
    ETAG(int_col),
    ETAG(bigint_col),
    ETAG(decimal_col),
    ETAG(float_col),
    ETAG(double_col),
    ETAG(bit_col),
    ETAG(char_col),
    ETAG(varchar_col),
    ETAG(binary_col),
    ETAG(tinyblob_col),
    ETAG(blob_col),
    ETAG(mediumblob_col),
    ETAG(longblob_col),
    ETAG(tinytext_col),
    ETAG(text_col),
    ETAG(mediumtext_col),
    ETAG(longtext_col),
    ETAG(enum_col),
    ETAG(set_col),
    ETAG(date_col),
    ETAG(datetime_col),
    ETAG(time_col),
    ETAG(year_col),
    ETAG(json_col)
FROM t1;

SELECT float_col, ETAG(float_col), double_col, ETAG(double_col) FROM t1;
SELECT ETAG(CONCAT(float_col, double_col, multipolygon_col)) FROM t1;
SELECT ETAG(CONCAT(float_col, double_col,longtext_col)) FROM t1;
SELECT ETAG(float_col) FROM t1 GROUP BY ETAG(float_col) order by 1 limit 5;
SELECT ETAG(character_length(tinytext_col)) FROM t1 GROUP BY ETAG(character_length(tinytext_col)) order by 1 limit 5;
SELECT ETAG(float_col) FROM t1 WHERE ETAG(character_length(tinytext_col)) > 1 order by 1 limit 5;
SELECT ETAG(float_col) FROM t1 WHERE ETAG(character_length(tinytext_col)) > ETAG(longtext_col);
SELECT ETAG(float_col) FROM t1 WHERE ETAG(character_length(tinytext_col)) > json_col;
SELECT ETAG(tinytext_col) FROM t1 WHERE ETAG(character_length(tinytext_col)) > date_col order by 1 limit 5;
--error ER_ETAG_NOT_SUPPORTED
SELECT ETAG(multipolygon_col) FROM t1;

CREATE TABLE CTAS_t1 as SELECT ETAG(tinyint_col),
    ETAG(smallint_col),
    ETAG(mediumint_col),
    ETAG(int_col),
    ETAG(bigint_col),
    ETAG(decimal_col),
    ETAG(float_col),
    ETAG(double_col),
    ETAG(bit_col),
    ETAG(char_col),
    ETAG(varchar_col),
    ETAG(binary_col),
    ETAG(tinyblob_col),
    ETAG(blob_col),
    ETAG(mediumblob_col),
    ETAG(longblob_col),
    ETAG(tinytext_col),
    ETAG(text_col),
    ETAG(mediumtext_col),
    ETAG(longtext_col),
    ETAG(enum_col),
    ETAG(set_col),
    ETAG(date_col),
    ETAG(datetime_col),
    ETAG(time_col),
    ETAG(year_col),
    ETAG(json_col) from t1;

SELECT * from CTAS_t1;
DROP TABLE CTAS_t1;

DROP TABLE t1;

CREATE TABLE tn1 (a datetime, b varchar(33) DEFAULT (ETAG(a)));
INSERT INTO tn1(a) VALUES ('2014-10-10 01:01:01');
SELECT ETAG(b) FROM tn1;
DROP TABLE tn1;

CREATE TABLE spatial_data (id INT AUTO_INCREMENT PRIMARY KEY, location GEOMETRY NOT NULL);
INSERT INTO spatial_data (location)  VALUES (ST_GeomFromText('LINESTRING(10 10, 30 40, 20 20)'));
INSERT INTO spatial_data (location)  VALUES (ST_GeomFromText('LINESTRING(10 10, 20 20, 30 40)'));

--sorted_result
--error ER_ETAG_NOT_SUPPORTED
SELECT ETAG(location) FROM spatial_data;

--sorted_result
SELECT ETAG(CAST(location as JSON)) FROM spatial_data;

DROP TABLE spatial_data;

CREATE TABLE departments (
  department_code VARCHAR(30),
  department_name VARCHAR(50),
  region          VARCHAR(50)
);

CREATE TABLE employees (
  emp_id      INT PRIMARY KEY,
  first_name  VARCHAR(50),
  last_name   VARCHAR(50),
  salary      DECIMAL(10,2),
  department  VARCHAR(30),
  hire_date   DATE
);

INSERT INTO departments (department_code, department_name, region) VALUES
('SALES', 'Sales Department', 'North America'),
('ENG', 'Engineering', 'North America'),
('HR', 'Human Resources', 'Europe'),
('MKT', 'Marketing', 'Asia'),
('FIN', 'Finance', 'Europe');

INSERT INTO employees (emp_id, first_name, last_name, salary, department, hire_date) VALUES
(1001, 'Alice', 'Johnson', 95000, 'SALES', '2021-03-15'),
(1002, 'Bob', 'Smith', 120000, 'ENG', '2022-07-10'),
(1003, 'Carol', 'Taylor', 75000, 'HR', '2019-11-25'),
(1004, 'David', 'Brown', 68000, 'MKT', '2023-01-20'),
(1005, 'Eva', 'Green', 105000, 'ENG', '2020-09-01'),
(1006, 'Frank', 'White', 45000, 'SALES', '2024-02-14'),
(1007, 'Grace', 'Lee', 56000, 'HR', '2023-10-05'),
(1008, 'Henry', 'Kim', 66000, 'MKT', '2022-04-30'),
(1009, 'Isla', 'Nguyen', 89000, 'FIN', '2021-12-17'),
(1010, 'Jack', 'Patel', 51000, 'ENG', '2023-08-25'),
(1011, 'Tom', 'Exact', 50000.000, 'FIN', '2022-01-01'),
(1012, 'grace', 'lee', 56000, 'HR', '2023-10-05'),
(1013, 'GRACE', 'LEE', 56000, 'HR', '2023-10-05'),
(1014, ' Alice ', 'Johnson', 95000, 'SALES', '2021-03-15'),
(1015, 'Alice', '  Johnson  ', 95000, 'SALES', '2021-03-15'),
(1016, 'Zane', 'DateEdge', 70000, 'ENG', '2023-08-25 14:32:55'),
(1017, 'Floaty', 'Numbers', 51000.0001, 'ENG', '2023-08-25');

SELECT
  emp_id, first_name, last_name, salary,
  ETAG(CONCAT(first_name, last_name, salary)) AS etag,
  ETAG(LOWER(first_name)) AS etag_lower,
  ETAG(UPPER(first_name)) AS etag_upper,
  ETAG(CONCAT(first_name, ' ', last_name)) AS e1
FROM employees
WHERE department = 'SALES';

SELECT
  emp_id, COALESCE(first_name, 'Unknown') AS fname,
  CASE
    WHEN salary >= 100000 THEN 'High'
    WHEN salary BETWEEN 50000 AND 99999 THEN 'Medium'
    ELSE 'Low'
  END AS salary_grade,
  ETAG(COALESCE(first_name, 'Unknown'), salary) AS etag
FROM employees
WHERE department IN (
  SELECT DISTINCT department
  FROM employees
  WHERE hire_date >= STR_TO_DATE('2022-01-01', '%Y-%m-%d')
);

SELECT
  department, COUNT(*) AS total_employees, AVG(salary) AS avg_salary,
  ETAG(department, FORMAT(AVG(salary), 2)) AS dept_etag
FROM employees
GROUP BY department
HAVING AVG(salary) > 50000 ORDER BY department;

SELECT
  emp_id,
  CONCAT(UCASE(LEFT(first_name, 1)), LCASE(SUBSTRING(first_name, 2)), ' ',
         UCASE(LEFT(last_name, 1)), LCASE(SUBSTRING(last_name, 2))) AS full_name,
  salary,
  DATE_FORMAT(hire_date, '%Y-%m') AS hire_month,
  ETAG(
    UPPER(department),
    ROUND(salary, -3),
    DATE_FORMAT(hire_date, '%Y-%m')
  ) AS custom_etag
FROM employees
WHERE salary > (
  SELECT AVG(salary)
  FROM employees
  WHERE department = 'Engineering'
);

SELECT
  e.emp_id, e.first_name, e.last_name, d.department_name,
  ETAG(e.first_name, e.last_name, d.department_name) AS row_etag
FROM employees e
JOIN departments d ON e.department = d.department_code
WHERE d.region = 'North America';

SELECT
  emp_id, salary,
  ETAG(FORMAT(salary, 2)) AS etag1,
  ETAG(ROUND(salary, 2)) AS etag2,
  ETAG(DATE_FORMAT(hire_date, '%Y-%m-%d')) AS etag3,
  ETAG(salary * 1.0) AS etag_float,
  ETAG(salary) AS etag_int
FROM employees;

DROP TABLE employees, departments;

#Check the empty string presence in json_array
SELECT ETAG(json_array()), ETAG(json_array('')), ETAG(json_array('', '', '', ''));
SELECT ETAG(json_array('a', 'b')), ETAG(json_array('a', '', 'b', ''));
SELeCT ETAG(cast('null' as json)), ETAG(null);

#Check ETAG differnce between NULL and '\0'
SELECT ETAG(null), ETAG('\0');

#Check ETAG differnce between NULL and Json null
CREATE TABLE t1(x JSON);
INSERT INTO t1 VALUES (NULL);
SELECT ETAG(NULL), ETAG(x) FROM t1;
DROP TABLE t1;

#Check the equal ETAG values for const case
CREATE TABLE t1(x varchar(100));
INSERT INTO t1 VALUES ("x");
SELECT ETAG("x"), ETAG(x) FROM t1;
DROP TABLE t1;
