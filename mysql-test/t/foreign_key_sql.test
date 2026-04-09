#WL 11249 SQL engine layer foreign key support
--echo # FR 1.1) Test foreign key during INSERT operation on the child table
--echo # FR 1.1.1) Test FK reference on integer primary key
--echo # Create parent table referenced and child table referencing with foreign key constraints.
--echo # Insert data into the parent table and verify that inserting invalid foreign key values into
--echo #  the child table reports ER_NO_REFERENCED_ROW_2.
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1));
INSERT INTO referenced VALUES (10, 10);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (1, 1);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (1, 10);
INSERT INTO referencing VALUES (10, 1);
INSERT INTO referencing VALUES (10, 10);
DROP TABLE referencing, referenced;

--echo # FR 1.1.2) Test FK on VARCHAR types with multiple indexes
--echo # Create parent table referenced with multiple unique indexes and child table referencing with
--echo #  a foreign key constraint referencing one of the unique indexes.
--echo # Insert data into the parent table and verify that inserting invalid foreign key values into
--echo #  the child table reports ER_NO_REFERENCED_ROW_2.
CREATE TABLE referenced(id1 INT, id2 VARCHAR(20), id3 INT, UNIQUE KEY(id1), unique key(id2), unique key(id3)) engine=innodb;
CREATE TABLE referencing(idd1 INT, idd2 VARCHAR(20), FOREIGN KEY(idd2) REFERENCES referenced(id2));
INSERT INTO referenced VALUES (1, 'SomeValue', 10);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (1, 'NoValue') ;
INSERT INTO referencing VALUES (5, 'SomeValue');
DROP TABLE referencing, referenced;

--echo # FR 1.1.3) Test FK with multiple references
--echo # Create multiple parent tables (referenced1, referenced2, referenced3) and a child table referencing
--echo #  with multiple foreign key constraints referencing the parent tables.
--echo # Insert data into the parent tables and verify that inserting invalid foreign key values into the
--echo #  child table reports ER_NO_REFERENCED_ROW_2.
CREATE TABLE referenced1(id11 INT, UNIQUE KEY(id11)) engine=innodb;
CREATE TABLE referenced2(id12 INT, UNIQUE KEY(id12)) engine=innodb;
CREATE TABLE referenced3(id13 INT, UNIQUE KEY(id13)) engine=innodb;
CREATE TABLE referencing(idd1 INT, idd2 INT, idd3 INT,
  FOREIGN KEY(idd1) REFERENCES referenced1(id11),
  FOREIGN KEY(idd2) REFERENCES referenced2(id12),
  FOREIGN KEY(idd3) REFERENCES referenced3(id13));

insert INTO referenced1 VALUES (1);
insert INTO referenced2 VALUES (2);
insert INTO referenced3 VALUES (3);

--error ER_NO_REFERENCED_ROW_2
insert INTO referencing VALUES (1,1,1);
insert INTO referencing VALUES (1,2,3);
--error ER_NO_REFERENCED_ROW_2
insert INTO referencing VALUES (1,2,1);
DROP TABLE referencing, referenced1, referenced2, referenced3;

--echo # FR 1.1.4) Test multiple references for same column from different tables
--echo # Create multiple parent tables (t1, t2) and a child table t3 with multiple foreign key constraints
--echo #  referencing the same column in the parent tables.
--echo # Insert data into the parent tables and verify that inserting invalid foreign key values into
--echo #  the child table reports ER_NO_REFERENCED_ROW_2.
CREATE TABLE t1(f1 INT PRIMARY KEY);
CREATE TABLE t2(f1 INT PRIMARY KEY);
CREATE TABLE t3(f1 INT PRIMARY KEY, FOREIGN KEY (f1) REFERENCES t1(f1), FOREIGN KEY(f1) REFERENCES t2(f1));
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
INSERT INTO t2 VALUES (1);
INSERT INTO t2 VALUES (3);
INSERT INTO t3 VALUES (1);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t3 VALUES (2);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t3 VALUES (3);
DROP TABLE t1, t2, t3;

--echo # FR 1.1.5) Test INSERT with autoincrement on FK column
--echo # Create parent table t1 and child table t2 with a foreign key constraint and an auto-increment column
--echo #  in the child table.
--echo # Insert data into the parent table and verify that inserting data into the child table with an
--echo #  auto-increment value works correctly.
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 CHAR(6)) ENGINE=InnoDB;
CREATE TABLE t2 (f1 INT AUTO_INCREMENT, f2 INT, FOREIGN KEY(f1) REFERENCES t1(f1) ON DELETE CASCADE) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1, 'one'), (2, 'two'), (3, 'three');
INSERT INTO t2 VALUES (NULL, 1);
SELECT LAST_INSERT_ID();
INSERT INTO t2 VALUES (2, 2);
INSERT INTO t2 VALUES (NULL, 3);
SELECT LAST_INSERT_ID();
SELECT * FROM t2;
DROP TABLE t2, t1;

--echo # FR 1.2) Test foreign key support during UPDATE operation on the child table.
--echo # Create parent table referenced and child table referencing with foreign key constraints.
--echo # Insert data into the parent and child tables and verify that updating the foreign key value
--echo #  in the child table to an invalid value reports ER_NO_REFERENCED_ROW_2.
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1));
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (20, 20);
INSERT INTO referencing VALUES (10, 10);
UPDATE referencing set idd2 = 30 WHERE idd2 = 10; #works because fk is not updated
--error ER_NO_REFERENCED_ROW_2
UPDATE referencing set idd1 = 30 WHERE idd1 = 10;
UPDATE referencing set idd1 = 20 WHERE idd1 = 10;
SELECT * FROM referencing;
DROP TABLE referencing, referenced;

--echo # FR 1.3) Test foreign key support with DELETE RESTRICT clause on the parent table.
--echo # FR 1.3.1) Test FK reference on PRIMARY key with ON DELETE RESTRICT
--echo # Create parent table referenced and child table referencing with foreign key constraints and
--echo #  ON DELETE RESTRICT.
--echo # Insert data into the parent and child tables and verify that deleting a row from the parent table
--echo #  that has a corresponding foreign key value in the child table reports ER_ROW_IS_REFERENCED_2.
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1) ON DELETE RESTRICT);
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (20, 20);
INSERT INTO referencing VALUES (10, 10);
DELETE FROM referenced WHERE id1 = 20;
--error ER_ROW_IS_REFERENCED_2
DELETE FROM referenced WHERE id1 = 10;
DROP TABLE referencing, referenced;

--echo # FR 1.3.2) Test FK reference with ON DELETE NO ACTION and existing partial key
CREATE TABLE referenced(id1 INT, id2 INT, UNIQUE KEY(id1));
CREATE TABLE referencing(idd1 INT, idd2 INT, UNIQUE KEY(idd1, idd2), 
  FOREIGN KEY (idd1) REFERENCES referenced(id1) ON DELETE NO ACTION);
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (20, 20);
INSERT INTO referencing VALUES (10, 10);
DELETE FROM referenced WHERE id1 = 20;
--error ER_ROW_IS_REFERENCED_2
DELETE FROM referenced WHERE id1 = 10;
DROP TABLE referencing, referenced;

--echo # FR 1.4) Test foreign key support with UPDATE RESTRICT clause on the parent table
--echo # Create parent table referenced and child table referencing with foreign key constraints
--echo #  and ON UPDATE RESTRICT.
--echo # Insert data into the parent and child tables and verify that updating a row in the parent table
--echo #  that has a corresponding foreign key value in the child table reports ER_ROW_IS_REFERENCED_2.
--echo # operation on the parent table, if foreign key value exist in the
--echo # child table, an error ER_ROW_IS_REFERENCED_2 must be reported.
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1) ON UPDATE RESTRICT);
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (20, 20);
INSERT INTO referenced VALUES (30, 30);
INSERT INTO referencing VALUES (10, 10);
INSERT INTO referencing VALUES (20, 20);
UPDATE referenced set id2 = 40 WHERE id2 = 30;
UPDATE referenced set id1 = 40 WHERE id1 = 30;
--error ER_ROW_IS_REFERENCED_2
UPDATE referenced set id1 = 20 WHERE id1 = 10;
SELECT * from referenced;
SELECT * from referencing;
DROP TABLE referencing, referenced;

--echo # FR 1.5) Test foreign key support with DELETE CASCADE clause on the parent table.
--echo # Create parent table referenced and child table referencing with foreign key constraints
--echo #  and ON DELETE CASCADE.
--echo # Insert data into the parent and child tables and verify that deleting a row from the parent table
--echo #  cascades the deletion to the child table.
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1) ON DELETE CASCADE);
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (20, 20);
INSERT INTO referenced VALUES (30, 30);
INSERT INTO referencing VALUES (10, 10);
INSERT INTO referencing VALUES (10, 20);
INSERT INTO referencing VALUES (20, 10);
INSERT INTO referencing VALUES (20, 20);
DELETE FROM referenced WHERE id1 = 30;
DELETE FROM referenced WHERE id1 = 10;
SELECT * FROM referencing;
SELECT * FROM referenced;
SELECT table_name, rows_updated, rows_deleted FROM sys.schema_table_statistics
  WHERE table_schema='test' ORDER BY table_name;
SELECT table_name, rows_updated, rows_deleted FROM sys.schema_index_statistics
  WHERE table_schema='test' ORDER BY table_name;
DROP TABLE referencing, referenced;

--echo # FR 1.6) Test foreign key support with DELETE SET NULL clause on the parent table.
--echo # Create parent table referenced and child table referencing with foreign key constraints
--echo #  and ON DELETE SET NULL.
--echo # Insert data into the parent and child tables and verify that deleting a row from the
--echo #  parent table sets the corresponding foreign key value to NULL in the child table.
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1) ON DELETE SET NULL);
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (20, 20);
INSERT INTO referenced VALUES (30, 30);
INSERT INTO referencing VALUES (10, 10);
INSERT INTO referencing VALUES (10, 20);
INSERT INTO referencing VALUES (20, 10);
INSERT INTO referencing VALUES (20, 20);
DELETE FROM referenced WHERE id1 = 30;
DELETE FROM referenced WHERE id1 = 10;
SELECT * FROM referencing;
SELECT * FROM referenced;
SELECT table_name, rows_updated, rows_deleted FROM sys.schema_table_statistics
  WHERE table_schema='test' ORDER BY table_name;
SELECT table_name, rows_updated, rows_deleted FROM sys.schema_index_statistics
  WHERE table_schema='test' ORDER BY table_name;
DROP TABLE referencing, referenced;

--echo # FR 1.7) Test foreign key support with UPDATE CASCADE clause on the parent table
--echo # Create parent table referenced and child table referencing with foreign key constraints
--echo #  and ON UPDATE CASCADE.
--echo # Insert data into the parent and child tables and verify that updating a row in the parent table
--echo #  cascades the update to the child table.
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1) ON UPDATE CASCADE);
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (20, 20);
INSERT INTO referenced VALUES (30, 30);
INSERT INTO referencing VALUES (10, 10);
INSERT INTO referencing VALUES (20, 20);
INSERT INTO referencing VALUES (30, 30);
UPDATE referenced set id1 = 40 WHERE id1 = 30;
SELECT table_name, rows_updated, rows_deleted FROM sys.schema_table_statistics
  WHERE table_schema='test' ORDER BY table_name;
SELECT table_name, rows_updated, rows_deleted FROM sys.schema_index_statistics
  WHERE table_schema='test' ORDER BY table_name;
--error ER_DUP_ENTRY
UPDATE referenced set id1 = 20 WHERE id1 = 10;
SELECT * from referenced;
SELECT * from referencing;
DROP TABLE referencing, referenced;

--echo # FR 1.8) Test foreign key support with UPDATE SET NULL clause on the parent table
--echo # Create parent table referenced and child table referencing with foreign key constraints
--echo #  and ON UPDATE SET NULL.
--echo # Insert data into the parent and child tables and verify that updating a row in the parent table
--echo #  sets the corresponding foreign key value to NULL in the child table.
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1) ON UPDATE SET NULL);
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (20, 20);
INSERT INTO referenced VALUES (30, 30);
INSERT INTO referencing VALUES (10, 10);
INSERT INTO referencing VALUES (20, 20);
INSERT INTO referencing VALUES (30, 30);
UPDATE referenced set id1 = 40 WHERE id1 = 30;
--error ER_DUP_ENTRY
UPDATE referenced set id1 = 20 WHERE id1 = 10;
SELECT * from referenced;
SELECT * from referencing;
DROP TABLE referencing, referenced;

--echo # FR 2.3) Foreign key cascade must be supported for stored generated columns.
--echo # FR 2.3.1) Test foreign key cascade for stored generated columns
--echo # Create parent table parent with a stored generated column and child table child
--echo #  with a foreign key constraint referencing the generated column.
--echo # Insert data into the parent and child tables and verify that UPDATE CASCADE works correctly
CREATE TABLE parent(fname CHAR(64), lname CHAR(64), fullname VARCHAR(128)
GENERATED ALWAYS AS (CONCAT(fname, ' ', lname))STORED);
CREATE UNIQUE INDEX parentidx_fullname ON parent(fullname);
CREATE TABLE child(fullname CHAR(128) REFERENCES parent(fullname) ON UPDATE CASCADE);
INSERT INTO parent (fname, lname) VALUES ('fname', 'lname');
INSERT INTO child (fullname) VALUES ('fname lname');
#InnoDB FK does not cascade
UPDATE parent SET fname='newname' WHERE fname='fname';
SELECT * FROM parent;
SELECT * FROM child;
DROP TABLE parent, child;

--echo # FR 2.3.2) Test foreign key cascade referring to base column of stored generated column
CREATE TABLE parent(fname CHAR(64), lname CHAR(64), fullname VARCHAR(128)
GENERATED ALWAYS AS (CONCAT(fname, ' ', lname)) STORED, UNIQUE(fname));
CREATE TABLE child(fname CHAR(128) REFERENCES parent(fname) ON UPDATE CASCADE);
INSERT INTO parent (fname, lname) VALUES ('fname', 'lname');
INSERT INTO child (fname) VALUES ('fname');
UPDATE parent SET fname='newname' WHERE fname='fname';
SELECT * FROM parent;
SELECT * FROM child;
DROP TABLE parent, child;

--echo # FR 2.3.3) Test foreign key cascade on base column of virtual generated column
CREATE TABLE parent(fname VARCHAR(64) PRIMARY KEY);
CREATE TABLE child (fname VARCHAR(64) REFERENCES parent(fname) ON UPDATE CASCADE, lname CHAR(64),
fullname VARCHAR(128) GENERATED ALWAYS AS (CONCAT(fname, ' ', lname)) VIRTUAL);
INSERT INTO parent (fname) VALUES ('fname');
INSERT INTO child (fname, lname) VALUES ('fname', 'lname');
UPDATE parent SET fname='newname' WHERE fname='fname';
SELECT * FROM parent;
SELECT * FROM child;
DROP TABLE parent, child;

--echo # Bug#38691473 WL#11249: Foreign key constraint on generated column with ON UPDATE CASCADE does not work
CREATE TABLE author (author_id INT, author_gcol INT AS (author_id*2) STORED, PRIMARY KEY(author_gcol),
first_name VARCHAR(50) NOT NULL, last_name VARCHAR(50) NOT NULL);
INSERT INTO author(author_id, first_name, last_name) VALUES('1', 'Jules', 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', 'Sidney', 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT,
author_id INT, FOREIGN KEY (author_id) REFERENCES author(author_gcol) ON UPDATE CASCADE);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 6);

UPDATE author SET author_id = 4 WHERE author_id = 3;

SELECT * FROM author;
SELECT * FROM book;
DROP TABLE author, book;

--echo # FR 2.4) Test foreign key on composite columns
--echo # FR 2.4.1) Test insert on composite foreign key
--echo # Create parent table referenced with a composite unique key and child table
--echo #  referencing with a foreign key constraint referencing the composite key.
--echo # Insert data into the parent and child tables and verify that foreign key
--echo #  checks work correctly.
CREATE TABLE referenced(id1 INT, id2 INT, id3 INT, UNIQUE KEY(id1, id2), UNIQUE KEY(id2, id3), UNIQUE KEY(id1, id3));
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1, idd2) REFERENCES referenced(id1, id2));
INSERT INTO referenced VALUES (10, 20, 30);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (10, 10);
INSERT INTO referencing VALUES (10, 20);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (10, 30);
DROP TABLE referencing, referenced;

--echo # FR 2.4.2) Test foreign key with multi key for UPDATE and DELETE CASCADE
CREATE TABLE t1 (f1 INT NOT NULL, f2 VARCHAR(10), f3 CHAR(4), f4 BINARY(3),
f5 VARBINARY(2), f6 INT, CONSTRAINT PRIMARY KEY (f1,f2,f3,f4,f5,f6))
ENGINE=InnoDB;

CREATE TABLE t2 (f1 int not null, f2 varchar(10), f3 char(4), f4 BINARY(3),
f5 VARBINARY(2), f6 INT,CONSTRAINT t2_t1_fk FOREIGN KEY (f1,f2,f3,f4,f5,f6)
REFERENCES t1(f1,f2,f3,f4,f5,f6) ON UPDATE CASCADE ON DELETE CASCADE)
ENGINE=InnoDB;

INSERT INTO t1 VALUES (1, 'aa', 'bb', 0x123456, 0x55, -1),
(2, 'cc', 'dd  ', 0x123400, 0x55, -2),(3, 'ee', 'f', 0x123456, 0x55, -2);
INSERT INTO t2 VALUES (1, 'aa', 'bb', 0x123456, 0x55, -1),
(2, 'cc', 'dd', 0x1234, 0x55, -2),(3, 'ee', 'f   ', 0x123456, 0x55, -2);

SELECT * FROM t1;
SELECT * FROM t2 order by f1,f2,f3,f4,f5,f6;

UPDATE t1 SET f2='modified' WHERE f6=-2;
SELECT * FROM t1;
SELECT * FROM t2 order by f1,f2,f3,f4,f5,f6;

DELETE FROM t1 WHERE f1 = 1;
SELECT * FROM t1;
SELECT * FROM t2 order by f1,f2,f3,f4,f5,f6;
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t2 VALUES (2, 'cc', 'dd', 0x1234, 0x55, -2);

ALTER TABLE t2 DROP CONSTRAINT t2_t1_fk;
ALTER TABLE t2 ADD CONSTRAINT t2_t1_fk FOREIGN KEY (f1,f2,f3,f4,f5,f6)
REFERENCES t1(f1,f2,f3,f4,f5,f6);

--error ER_ROW_IS_REFERENCED_2
DELETE FROM t1 WHERE f1=2;
SELECT * FROM t1;
SELECT * FROM t2 order by f1,f2,f3,f4,f5,f6;

DROP TABLE t2,t1;

--echo # FR 2.5) Foreign key on unique key columns in parent and child must be
--echo # supported.
--echo # FR 2.5.1) Test foreign key support on unique key columns in parent tables.
--echo # Create parent table referenced with a unique key and child table referencing with
--echo #  a foreign key constraint referencing the unique key.
--echo # Insert data into the parent and child tables and verify that foreign key checks
--echo #  work correctly.
CREATE TABLE referenced(id1 INT UNIQUE KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1));
INSERT INTO referenced VALUES (10, 10);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (1, 10);
INSERT INTO referencing VALUES (10, 1);
INSERT INTO referencing VALUES (10, 10);
DROP TABLE referencing, referenced;

--echo # FR 2.5.2) Test foreign key support on unique key columns in child tables.
CREATE TABLE referenced(id1 INT UNIQUE KEY, id2 INT);
CREATE TABLE referencing(idd1 INT UNIQUE, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1));
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referencing VALUES (10, 10);
--error ER_ROW_IS_REFERENCED_2
UPDATE referenced SET id1 = 100 WHERE id1 = 10;
DROP TABLE referencing, referenced;

--echo # FR 2.6) Foreign key must be supported on non-unique key columns in parent
--echo # and child must be supported.
--echo # FR 2.6.1) Test foreign key support on non-unique key columns in parent tables
--echo # Create parent table referenced with a non-unique key and child table referencing with a
--echo #  foreign key constraint referencing the non-unique key.
--echo # Insert data into the parent and child tables and verify that foreign key checks work correctly
CREATE TABLE referenced(id1 INT, id2 INT, KEY(id1));
SET restrict_fk_on_non_standard_key = OFF;
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1));
SET restrict_fk_on_non_standard_key = ON;
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (40, 40);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (1, 10);
INSERT INTO referencing VALUES (10, 1);
INSERT INTO referencing VALUES (10, 10);
UPDATE referencing SET idd1 = 40 where idd2 = 1;
SELECT * FROM referencing;
DROP TABLE referencing, referenced;

--echo # FR 2.6.2) Test foreign key support on non-unique key column in parent and unique
--echo # column in child table
CREATE TABLE referenced(id1 INT, id2 INT, KEY(id1));
SET restrict_fk_on_non_standard_key = OFF;
CREATE TABLE referencing(idd1 INT UNIQUE, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1));
SET restrict_fk_on_non_standard_key = ON;
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (40, 40);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (1, 10);
INSERT INTO referencing VALUES (10, 1);
UPDATE referencing SET idd1 = 40 where idd2 = 1;
SELECT * FROM referencing;
DROP TABLE referencing, referenced;

--echo # FR 2.7) Foreign key must be supported on partial columns of the key in
--echo # parent and child.
--echo # FR 2.7.1) Test foreign key support on partial columns of the key in parent table
CREATE TABLE referenced(id1 INT, id2 INT, KEY(id1, id2));
SET restrict_fk_on_non_standard_key = OFF;
CREATE TABLE referencing(idd1 INT, idd2 INT, FOREIGN KEY (idd1) REFERENCES referenced(id1));
SET restrict_fk_on_non_standard_key = ON;
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (40, 40);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (1, 10);
INSERT INTO referencing VALUES (10, 1);
INSERT INTO referencing VALUES (10, 10);
UPDATE referencing SET idd1 = 40 where idd1 = 10;
SELECT * FROM referencing;
DROP TABLE referencing, referenced;

--echo # FR 2.7.2) Test foreign key support on partial columns of the key in child table
CREATE TABLE referenced(id1 INT PRIMARY KEY, id2 INT);
SET restrict_fk_on_non_standard_key = OFF;
CREATE TABLE referencing(idd1 INT, idd2 INT, UNIQUE(idd1, idd2),
FOREIGN KEY (idd1) REFERENCES referenced(id1));
SET restrict_fk_on_non_standard_key = ON;
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (40, 40);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (1, 10);
INSERT INTO referencing VALUES (10, 1);
INSERT INTO referencing VALUES (10, 10);
UPDATE referencing SET idd1 = 40 where idd1 = 10;
SELECT * FROM referencing;
DROP TABLE referencing, referenced;

--echo # FR 2.8) If any one of the column value in composite foreign key is NULL,
--echo # foreign key check must not be performed for other columns.
--echo # FR 2.8.1) Test MATCH SIMPLE
--echo # Create parent table referenced with a composite unique key and child table
--echo #  referencing with a foreign key constraint referencing the composite key.
--echo # Insert data into the parent and child tables and verify that foreign key
--echo #  checks are not performed when one of the column values is NULL.
CREATE TABLE referenced(id1 INT, id2 INT, UNIQUE KEY(id1, id2));
CREATE TABLE referencing(idd1 INT, idd2 INT,
  FOREIGN KEY (idd1, idd2) REFERENCES referenced(id1, id2));
INSERT INTO referenced VALUES (10, 10);
INSERT INTO referenced VALUES (15, 15);
INSERT INTO referenced VALUES (30, 30);
INSERT INTO referenced VALUES (30, 35);
INSERT INTO referenced VALUES (40, 40);
INSERT INTO referencing VALUES (10, 10);
INSERT INTO referencing VALUES (15, 15);
INSERT INTO referencing VALUES (30, 30);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (10, 20);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO referencing VALUES (20, 20);
INSERT INTO referencing VALUES (NULL, 20);
INSERT INTO referencing VALUES (20, NULL);
UPDATE referencing SET idd1 = NULL, idd2=50  where idd1 = 10;
UPDATE referencing SET idd1 = 150, idd2=NULL  where idd1 = 15;
--error ER_NO_REFERENCED_ROW_2
UPDATE referencing SET idd1 = 40,  idd2=50  where idd1 = 30;
SELECT * FROM referencing ORDER BY idd1, idd2;
INSERT INTO referencing VALUES (30, 35);
DELETE FROM referenced where id1=30 and id2=NULL;
SELECT * FROM referencing ORDER BY idd1, idd2;
DROP TABLE referencing, referenced;

--echo Bug#38718457 - Composite foreign key with one column containing NULL is allowed
--echo When composite unique key contains NULL values, RESTRICT as well as CASCADE
--echo updates the parent table and leaves the child table in inconsistent state
--echo InnoDB neglects NULL values for both DELETE and UPDATE but SQL FK neglets
--echo only for UPDATE. Fixed by neglecting FK check/cascade for DELETE to mimic
--echo InnoDB FK behavior.

--echo CASE 1: UPDATE RESTRICT
CREATE TABLE author (author_id INT, first_name VARCHAR(50), last_name VARCHAR(50), UNIQUE KEY(author_id, first_name));
INSERT INTO author(author_id, first_name, last_name) VALUES('1', NULL, 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', NULL, 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT, author_id INT, first_name VARCHAR(50),
FOREIGN KEY (author_id, first_name) REFERENCES author(author_id, first_name) ON DELETE RESTRICT ON UPDATE RESTRICT);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3, NULL);

UPDATE author SET author_id = 4 where author_id = 3;

SELECT * FROM author;
SELECT * FROM book;

DROP TABLE author, book;

--echo CASE 2: DELETE RESTRICT
CREATE TABLE author (author_id INT, first_name VARCHAR(50), last_name VARCHAR(50), UNIQUE KEY(author_id, first_name));
INSERT INTO author(author_id, first_name, last_name) VALUES('1', NULL, 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', NULL, 'Sheldon');

CREATE TABLE book ( book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT, author_id INT, first_name VARCHAR(50),
FOREIGN KEY (author_id, first_name) REFERENCES author(author_id, first_name) ON DELETE RESTRICT ON UPDATE RESTRICT);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3, NULL);

DELETE FROM author WHERE author_id = 3;

SELECT * FROM author;
SELECT * FROM book;

DROP TABLE author, book;

--echo CASE 3: UPDATE CASCADE
CREATE TABLE author (author_id INT, first_name VARCHAR(50), last_name VARCHAR(50), UNIQUE KEY(author_id, first_name));
INSERT INTO author(author_id, first_name, last_name) VALUES('1', NULL, 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', NULL, 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT, author_id INT, first_name VARCHAR(50),
FOREIGN KEY (author_id, first_name) REFERENCES author(author_id, first_name) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3, NULL);

UPDATE author SET author_id = 4 where author_id = 3;

SELECT * FROM author;
SELECT * FROM book;

DROP TABLE author, book;

--echo CASE 4: DELETE CASCADE
CREATE TABLE author (author_id INT, first_name VARCHAR(50), last_name VARCHAR(50), UNIQUE KEY(author_id, first_name));
INSERT INTO author(author_id, first_name, last_name) VALUES('1', NULL, 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', NULL, 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT, author_id INT, first_name VARCHAR(50),
FOREIGN KEY (author_id, first_name) REFERENCES author(author_id, first_name) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3, NULL);

DELETE FROM author WHERE author_id = 3;

SELECT * FROM author;
SELECT * FROM book;

DROP TABLE author, book;

--echo CASE 5: UPDATE SET NULL
CREATE TABLE author (author_id INT, first_name VARCHAR(50), last_name VARCHAR(50), UNIQUE KEY(author_id, first_name));
INSERT INTO author(author_id, first_name, last_name) VALUES('1', NULL, 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', NULL, 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT, author_id INT, first_name VARCHAR(50),
FOREIGN KEY (author_id, first_name) REFERENCES author(author_id, first_name) ON DELETE SET NULL ON UPDATE SET NULL);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3, NULL);

UPDATE author SET author_id = 4 where author_id = 3;

SELECT * FROM author;
SELECT * FROM book;

DROP TABLE author, book;

--echo CASE 6: DELETE SET NULL
CREATE TABLE author (author_id INT, first_name VARCHAR(50), last_name VARCHAR(50), UNIQUE KEY(author_id, first_name));
INSERT INTO author(author_id, first_name, last_name) VALUES('1', NULL, 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', NULL, 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT, author_id INT, first_name VARCHAR(50),
FOREIGN KEY (author_id, first_name) REFERENCES author(author_id, first_name) ON DELETE SET NULL ON UPDATE SET NULL);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3, NULL);

DELETE FROM author WHERE author_id = 3;

SELECT * FROM author;
SELECT * FROM book;

DROP TABLE author, book;

--echo # FR 2.9) REPLACE statement must be supported on the tables with FK constraint.
--echo # FR 2.9.1) Test foreign key support with REPLACE statement
--echo # Create parent table t1 and child table t2 with foreign key constraints.
--echo # Insert data into the parent and child tables and verify that REPLACE statement
--echo #  works correctly.
CREATE TABLE t1(f1 INT PRIMARY KEY, f2 INT);
CREATE TABLE t2(f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON UPDATE CASCADE ON DELETE CASCADE);
REPLACE INTO t1 VALUES (1, 1);
REPLACE INTO t1 VALUES (2, 2);
REPLACE INTO t2 VALUES (1, 1);
--error ER_NO_REFERENCED_ROW_2
REPLACE INTO t2 VALUES (1, 5);
REPLACE INTO t2 VALUES (1, 2);
REPLACE INTO t1 VALUES (1, 10);
SELECT * FROM t1 order by f1;
SELECT * FROM t2 order by f1;
DROP TABLE t1, t2;

--echo # FR 2.9.2) Test REPLACE ON parent table FK column performs UPDATE CASCADE
CREATE TABLE t1(f1 INT PRIMARY KEY, f2 INT, UNIQUE(f2));
CREATE TABLE t2(f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f2) ON DELETE CASCADE ON UPDATE CASCADE);

REPLACE INTO t1 VALUES (1, 1);
REPLACE INTO t1 VALUES (2, 2);
REPLACE INTO t2 VALUES (1, 1);
REPLACE INTO t1 VALUES (1, 5);
SELECT * FROM t1 order by f1;
SELECT * FROM t2 order by f1;
DROP TABLE t1, t2;


--echo # FR 2.10) INSERT ... ON DUPLICATE KEY UPDATE must be supported on the
--echo # tables with FK constraint
--echo # FR 2.10.1) Test INSERT ON DUPLICATE KEY UPDATE with FK column update on child
--echo # Create parent table t1 and child table t2 with foreign key constraints.
--echo # Insert data into the parent and child tables and verify that
--echo #  INSERT ... ON DUPLICATE KEY UPDATE statement works correctly
CREATE TABLE t1(f1 INT PRIMARY KEY, f2 INT);
CREATE TABLE t2(f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1, 1);
INSERT INTO t1 VALUES (2, 2);
INSERT INTO t2 VALUES (1, 1);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t2 values (1, 1) ON DUPLICATE KEY UPDATE f2=5;
INSERT INTO t2 values (1, 1) ON DUPLICATE KEY UPDATE f2=2;
SELECT * FROM t1 order by f1;
SELECT * FROM t2 order by f1;
DROP TABLE t1, t2;

--echo # FR 2.10.2) Test INSERT ON DUPLICATE KEY UPDATE with FK column update on parent
CREATE TABLE t1(f1 INT PRIMARY KEY, f2 INT, UNIQUE(f2));
CREATE TABLE t2(f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f2) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1, 1);
INSERT INTO t1 VALUES (2, 2);
INSERT INTO t2 VALUES (1, 1);
INSERT INTO t1 values (1, 1) ON DUPLICATE KEY UPDATE f2=5;
SELECT * FROM t1 order by f1;
SELECT * FROM t2 order by f1;
DROP TABLE t1, t2;

--echo # FR 2.11) CREATE TABLE..AS SELECT must be supported on the tables with FK
--echo # constraint
--echo # Create parent table parent and child table child with foreign key constraints.
--echo # Insert data into the parent and child tables and verify that CREATE TABLE..AS SELECT
--echo #  statement works correctly
CREATE TABLE parent (id INT PRIMARY KEY, val INT);
CREATE TABLE child (id INT PRIMARY KEY, pid INT,
FOREIGN KEY (pid) REFERENCES parent(id) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO parent VALUES (1, 100), (2, 200), (3, 300);
INSERT INTO child  VALUES (10, 1), (11, 2);

CREATE TABLE backup_parent AS SELECT * FROM parent;
CREATE TABLE backup_child  AS SELECT * FROM child;
SELECT * FROM backup_parent ORDER BY id;
SELECT * FROM backup_child  ORDER BY id;

DELETE FROM backup_parent WHERE id=2;
DROP TABLE backup_child;
CREATE TABLE backup_child  AS SELECT * FROM child;
SELECT * FROM backup_parent ORDER BY id;
SELECT * FROM backup_child  ORDER BY id;

--echo # FR 2.12) INSERT INTO AS SELECT must be supported on the tables with FK
--echo # constraint
--echo # Insert legal rows from parent into child via SELECT
INSERT INTO child (id, pid) SELECT 12, 3;
SELECT * FROM child ORDER BY id;

--echo # Attempt to insert illegal FK via SELECT -> ER_NO_REFERENCED_ROW_2
--error ER_NO_REFERENCED_ROW_2
INSERT INTO child (id, pid) SELECT 13, 99;

SELECT * from parent;
--echo # Additional legal multi-row INSERT ... SELECT
INSERT INTO child (id, pid) SELECT val, id FROM parent WHERE id IN (1,2);
SELECT * FROM child ORDER BY id;

--echo # FR 2.13) Test foreign key support with LOAD DATA statement.
--echo # Prepare a temp data file with mixed valid and invalid rows
let $load_file1= $MYSQLTEST_VARDIR/tmp/fk_load1.csv;
--write_file $load_file1
15,1
16,2
17,99
EOF

--echo # Load with an invalid FK row -> ER_NO_REFERENCED_ROW_2
--replace_result $load_file1 <path>
--error ER_NO_REFERENCED_ROW_2
eval LOAD DATA INFILE '$load_file1' INTO TABLE child
FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n' (id, pid);

--echo # Verify no partial rows from failing batch were loaded
SELECT * FROM child ORDER BY id;

--echo # Prepare a valid load file and load it successfully
let $load_file2= $MYSQLTEST_VARDIR/tmp/fk_load2.csv;
--write_file $load_file2
15,1
16,2
17,3
EOF

--replace_result $load_file2 <path>
eval LOAD DATA INFILE '$load_file2' INTO TABLE child
FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n' (id, pid);

SELECT * FROM child ORDER BY id;


--remove_file $load_file1
--remove_file $load_file2

--echo # Clean up
DROP TABLE backup_child;
DROP TABLE backup_parent;
DROP TABLE child;
DROP TABLE parent;

--echo # FR 2.14) With a foreign key CASCADE clause, during DELETE and UPDATE
--echo # operation on the parent table, schema_table_statistics must be
--echo # updated for both parent and child table
CREATE TABLE referenced(id1 INT UNIQUE KEY, id2 INT);
CREATE TABLE referencing(idd1 INT, idd2 INT,
FOREIGN KEY (idd1) REFERENCES referenced(id1) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO referenced VALUES (10, 10),(20, 20), (30, 30);
INSERT INTO referencing VALUES (10, 10), (10, 20), (20, 10), (20,20);

SELECT table_name, rows_inserted, rows_updated, rows_deleted FROM sys.schema_table_statistics
  WHERE table_schema='test' ORDER BY table_name;
SELECT table_name, rows_inserted, rows_updated, rows_deleted FROM sys.schema_index_statistics
  WHERE table_schema='test' ORDER BY table_name;

UPDATE referenced SET id1 = 50 WHERE id1 = 30;

DELETE FROM referenced WHERE id1 = 20;

SELECT table_name, rows_updated, rows_deleted FROM sys.schema_table_statistics
  WHERE table_schema='test' ORDER BY table_name;
SELECT table_name, rows_updated, rows_deleted FROM sys.schema_index_statistics
  WHERE table_schema='test' ORDER BY table_name;

DROP TABLE referencing, referenced;

--echo # Bug#38668861 WL#11249: ON DELETE/UPDATE SET DEFAULT does not work as expected
--echo # ON DELETE/UPDATE SET DEFAULT is not supported. It should behave like RESTRICT
CREATE TABLE author (author_id INT PRIMARY KEY,
first_name VARCHAR(50) NOT NULL, last_name VARCHAR(50) NOT NULL);

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT,
title VARCHAR(200) NOT NULL, pub_year INT, author_id INT DEFAULT 1,
FOREIGN KEY (author_id) REFERENCES author(author_id) ON UPDATE SET DEFAULT ON DELETE SET DEFAULT);

INSERT INTO author VALUES(1, 'Jules', 'Verne');
INSERT INTO author VALUES(3, 'Sidney', 'Sheldon');
INSERT INTO book VALUES(1, 'Master of the Game', 1982, 3);

--error ER_ROW_IS_REFERENCED_2
UPDATE author SET author_id = 2 WHERE author_id = 3;

--error ER_ROW_IS_REFERENCED_2
DELETE FROM author WHERE author_id = 3;

DROP TABLE author, book;

--echo # LOCK TABLE on parent with ON DELETE RESTRICT
--echo # LOCK TABLE must take corresponding lock on parent/child
CREATE TABLE t1 (p INT PRIMARY KEY);
CREATE TABLE t2 (c INT, INDEX(c), CONSTRAINT fk_t2_c FOREIGN KEY (c) REFERENCES t1(p) ON DELETE RESTRICT);

INSERT INTO t1 VALUES (1),(2);
INSERT INTO t2 VALUES (1);

LOCK TABLES t1 WRITE;
--error ER_ROW_IS_REFERENCED_2
DELETE FROM t1 WHERE p=1;
UNLOCK TABLES;

LOCK TABLES t2 WRITE;
INSERT INTO t2 VALUES (2);
UNLOCK TABLES;

SELECT * FROM t1;
SELECT * FROM t2;
DROP TABLE t2, t1;

--echo # LOAD DATA Test for FK
CREATE TABLE t1(f1 INT PRIMARY KEY);
INSERT INTO t1 VALUES (1), (2);
CREATE TABLE t2(f1 INT REFERENCES t1(f1));
INSERT INTO t2 VALUES(1);
SET FOREIGN_KEY_CHECKS=0;
INSERT INTO t2 VALUES(5);
SET FOREIGN_KEY_CHECKS=1;
INSERT INTO t2 VALUES(2);
SELECT * FROM t2 INTO OUTFILE 'tmp1.txt';
DELETE FROM t2;
--error ER_NO_REFERENCED_ROW_2
LOAD DATA INFILE 'tmp1.txt' INTO TABLE t2;
SELECT * FROM t2;
LOAD DATA INFILE 'tmp1.txt' IGNORE INTO TABLE t2;
SELECT * FROM t2;
DROP TABLE t1, t2;

let $MYSQLD_DATADIR= `select @@datadir`;
remove_file $MYSQLD_DATADIR/test/tmp1.txt;
