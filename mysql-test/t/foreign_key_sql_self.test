#WL 11249 SQL engine layer foreign key support
--echo # FR 3) SQL FK must support self-referencing foreign key
--echo # FR 3.1) Self-referencing foreign key must check for existence of primary key
--echo # value for the given foreign key value.
CREATE TABLE self (pk INT PRIMARY KEY, fk1 INT, FOREIGN KEY (fk1) REFERENCES self (pk) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO self VALUES (1, NULL), (2, 1), (3, 2);
DELETE FROM self WHERE pk = 1;
DROP TABLE self;


--echo # FR 3.2) Self referencing foreign key with UPDATE CASCADE | SET NULL
--echo # clause must give error during UPDATE operation, an error
--echo # ER_ROW_IS_REFERENCED_2 must be reported.
CREATE TABLE self (pk INT PRIMARY KEY, fk1 INT, FOREIGN KEY (fk1) REFERENCES self (pk) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO self VALUES (1, NULL), (2, 1), (3, 2);

--echo # update which does not lead to cascade should work
UPDATE self SET pk=4 WHERE pk=3;

--echo # Verify that updating the primary key value to a value that is referenced
--echo #  by a foreign key in the same table reports ER_ROW_IS_REFERENCED_2
--error ER_ROW_IS_REFERENCED_2
UPDATE self SET pk=5 WHERE pk=1;

SELECT * FROM self ORDER BY pk; 
DROP TABLE self;

--echo # FR 3.3) Self referencing foreign key must CASCADE changes from
--echo # primary key to foreign key for DELETE CASCADE and DELETE SET NULL.
--echo # FR 3.3.1) Test Multiple Self Referencing
CREATE TABLE self (pk INT, fk1 INT, fk2 int, UNIQUE KEY(pk),
FOREIGN KEY (fk1) REFERENCES self (pk) ON UPDATE CASCADE ON DELETE CASCADE,
FOREIGN KEY (fk2) REFERENCES self (pk) ON UPDATE CASCADE ON DELETE CASCADE);
   
INSERT INTO self VALUES (1, NULL, NULL), (2, 1, NULL), (3, 2, 1);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO self VALUES (6, 1, 5); 
--error ER_NO_REFERENCED_ROW_2 
INSERT INTO self VALUES (7, 5, 1); 
   
DELETE FROM self WHERE pk=1; 

DROP TABLE self;

--echo # FR 3.3.2) Test Multi Level Self Referencing cascade
CREATE TABLE self (pk INT, pfk1 INT, fk2 int, UNIQUE KEY(pk), UNIQUE KEY(pfk1),
FOREIGN KEY (pfk1) REFERENCES self (pk) ON UPDATE CASCADE ON DELETE CASCADE,
FOREIGN KEY (fk2) REFERENCES self (pfk1) ON UPDATE CASCADE ON DELETE CASCADE);

INSERT INTO self VALUES (1, NULL, NULL), (2, 1, NULL), (3, 2, 1);
DELETE FROM self WHERE pk=1; 
SELECT * FROM self;
DROP TABLE self;

--echo # Test Self Referencing with same primary key and foreign key value
CREATE TABLE self (pk INT, fk int, UNIQUE KEY(pk),
FOREIGN KEY (fk) REFERENCES self (pk) ON UPDATE CASCADE ON DELETE CASCADE);

INSERT INTO self VALUES (1, 1), (2, 2);
DELETE FROM self where pk = 1;
SELECT * FROM self;
DROP TABLE self;

--echo # FR 3.4) Self-referencing foreign key DELETE CASCADE | SET NULL must be
--echo # supported in LOCK TABLE mode.
--echo # FR 3.4.1) DELETE CASCADE leads to multi level delete
CREATE TABLE self (pk INT PRIMARY KEY, fk1 INT, FOREIGN KEY (fk1) REFERENCES self (pk) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO self VALUES (1, NULL), (2, 1), (3, 2);
LOCK TABLES self WRITE;
DELETE FROM self WHERE pk = 1;
SELECT * FROM self;
DROP TABLE self;

--echo # FR 3.4.2) DELETE SET NULL leads to next level update
CREATE TABLE self (pk INT PRIMARY KEY, fk1 INT, FOREIGN KEY (fk1) REFERENCES self (pk) ON DELETE SET NULL);
INSERT INTO self VALUES (1, NULL), (2, 1), (3, 2);
LOCK TABLES self WRITE;
DELETE FROM self WHERE pk = 1;
SELECT * FROM self;
DROP TABLE self;

--echo # FR 3.4.3) Self-referencing foreign key UPDATE CASCADE returns error
--echo # in LOCK TABLE mode.
CREATE TABLE self (pk INT PRIMARY KEY, fk1 INT, FOREIGN KEY (fk1) REFERENCES self (pk) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO self VALUES (1, NULL), (2, 1), (3, 2);
LOCK TABLES self WRITE;
--error ER_ROW_IS_REFERENCED_2
UPDATE self SET pk=5 WHERE pk=1;
SELECT * FROM self ORDER BY pk;
DROP TABLE self;

--echo # FR 4) SQL FK must support circular foreign key.
--echo # FR 4.1) Circular foreign key must be detected and CASCADE operation
--echo # must succeed before it encounters a circular foreign key.
--echo # FR 4.1.1) Circular referencing involving two tables with different keys
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE employees (eid INT PRIMARY KEY, name VARCHAR(64), dept_id INT,
    FOREIGN KEY (dept_id) REFERENCES dept(dept_id) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE dept (dept_id INT PRIMARY KEY, name VARCHAR(64), mgr_id INT,
    FOREIGN KEY (mgr_id) REFERENCES employees(eid) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO employees VALUES (1, 'name1', 10);
INSERT INTO employees VALUES (11, 'name11', 10);
INSERT INTO employees VALUES (2, 'name2', 20);
INSERT INTO employees VALUES (3, 'name3', 30);
INSERT INTO dept VALUES (10, 'dept10', 1);
INSERT INTO dept VALUES (20, 'dept20', 2);
INSERT INTO dept VALUES (30, 'dept30', 3);
SET FOREIGN_KEY_CHECKS = 1;
DELETE FROM dept WHERE dept_id = 10;
UPDATE employees SET eid = 6 WHERE eid = 2;
SELECT * FROM dept;
SELECT * FROM employees;
DROP TABLE dept, employees;

--echo # FR 4.1.2) Circular referencing involving two tables with same key
--echo # t1(f1)->t2(f1)
--echo # t2(f1)->t1(f1)
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE t1(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t2(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t2(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1), (2);
SET FOREIGN_KEY_CHECKS=1;
DELETE FROM t1 WHERE f1=1;
SELECT * FROM t1;
SELECT * FROM t2;
UPDATE t1 SET f1=3 WHERE f1=2;
SELECT * FROM t1;
SELECT * FROM t2;
DROP TABLE t1, t2;

--echo # FR 4.1.3) Circular referencing involving three tables with same key
--echo # t1(f1)->t2(f1)
--echo # t2(f1)->t3(f1)
--echo # t3(f1)->t1(f1)
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE t1(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t3(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t2(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t3(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t2(f1) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1), (2);
INSERT INTO t3 VALUES (1), (2);
SET FOREIGN_KEY_CHECKS=1;
DELETE FROM t1 WHERE f1=1;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;
UPDATE t1 SET f1=3 WHERE f1=2;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;
DROP TABLE t1, t2, t3;

--echo # FR 4.1.4) Multipath UPDATE CASCADE from parent to grandchild for same column
--echo # t1(f1)->t21(f1)->t3(f1)
--echo #       ->t22(f1)->t3(f1)
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE t1(f1 INT, UNIQUE KEY(f1));
CREATE TABLE t21(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t22(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t3(f1 INT, UNIQUE KEY(f1),
FOREIGN KEY (f1) REFERENCES t21(f1) ON DELETE CASCADE ON UPDATE CASCADE,
FOREIGN KEY (f1) REFERENCES t22(f1) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t21 VALUES (1), (2);
INSERT INTO t22 VALUES (1), (2);
INSERT INTO t3 VALUES (1), (2);
SET FOREIGN_KEY_CHECKS=1;
DELETE FROM t1 WHERE f1=1;
SELECT * FROM t1;
SELECT * FROM t21;
SELECT * FROM t22;
SELECT * FROM t3;
--error ER_NO_REFERENCED_ROW_2  #InnoDB FK Fails
UPDATE t1 SET f1=3 WHERE f1=2;
SELECT * FROM t1;
SELECT * FROM t21;
SELECT * FROM t22;
SELECT * FROM t3;
DROP TABLE t1, t21, t22, t3;

--echo # FR 4.1.5) Multipath UPDATE CASCADE from parent to grandchild for two different columns
--echo # t1(f1)->t21(f1)->t3(f1)
--echo #       ->t22(f1)->t3(f2)
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE t1(f1 INT, UNIQUE KEY(f1));
CREATE TABLE t21(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t22(f1 INT, UNIQUE KEY(f1), FOREIGN KEY (f1) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t3(f1 INT, f2 INT, UNIQUE KEY(f1), UNIQUE KEY(f2),
FOREIGN KEY (f1) REFERENCES t21(f1) ON DELETE CASCADE ON UPDATE CASCADE,
FOREIGN KEY (f2) REFERENCES t22(f1) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t21 VALUES (1), (2);
INSERT INTO t22 VALUES (1), (2);
INSERT INTO t3 VALUES (1, 1), (2, 2);
SET FOREIGN_KEY_CHECKS=1;
DELETE FROM t1 WHERE f1=1;
SELECT * FROM t1;
SELECT * FROM t21;
SELECT * FROM t22;
SELECT * FROM t3;
UPDATE t1 SET f1=3 WHERE f1=2;
SELECT * FROM t1;
SELECT * FROM t21;
SELECT * FROM t22;
SELECT * FROM t3;
DROP TABLE t1, t21, t22, t3;

--echo # FR 4.1.6) UPDATE CASCADE circular referencing on different parent column
--echo # t1(f1)->t2(f1)->t1(f2)
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE t1(f1 int NOT NULL, f2 int NOT NULL, PRIMARY KEY (f1),
  FOREIGN KEY (f2) REFERENCES t2(f1) ON UPDATE CASCADE);
CREATE TABLE t2 (
  f1 int NOT NULL, f2 int NOT NULL, PRIMARY KEY (f1),
  FOREIGN KEY (f1) REFERENCES t1 (f1) ON UPDATE CASCADE);
INSERT INTO t1 values (1, 1);
INSERT INTO t2 values (1, 2);
SET FOREIGN_KEY_CHECKS=1;
--error ER_ROW_IS_REFERENCED_2 #InnoDB FK Fails
UPDATE t1 SET f1=100 WHERE f1=1;
SELECT * FROM t1;
SELECT * FROM t2;
DROP TABLE t1, t2;

--echo # FR 4.1.7) UPDATE CASCADE circular referencing with partial key
--echo # on different parent column
--echo # t1(f1,f2)->t2(f1, f2)->t1(f3)
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE t1(f1 int NOT NULL, f2 int NOT NULL, f3 int DEFAULT NULL,
  PRIMARY KEY (f1, f2), UNIQUE KEY (f3),
  FOREIGN KEY (f3) REFERENCES t2(f2) ON UPDATE CASCADE);
CREATE TABLE t2 (
  f1 int NOT NULL, f2 int NOT NULL, f3 int DEFAULT NULL,
  PRIMARY KEY (f1, f2), UNIQUE KEY f2 (f2),
  FOREIGN KEY (f1, f2) REFERENCES t1 (f1, f2) ON UPDATE CASCADE);
INSERT INTO t1 values (1, 2, 2);
INSERT INTO t2 values (1, 2, 3);
SET FOREIGN_KEY_CHECKS=1;
--error ER_ROW_IS_REFERENCED_2 #InnoDB FK Fails
UPDATE t1 SET f1=100, f2=101 WHERE f1=1 and f2=2;
SELECT * FROM t1;
SELECT * FROM t2;
DROP TABLE t1, t2;
