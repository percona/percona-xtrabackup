connection default;
CREATE TABLE self(pk INT PRIMARY KEY, fk1 INT,
                  FOREIGN KEY(fk1) REFERENCES self(pk));
INSERT INTO self VALUES(1, NULL), (2, 1);

eval SET SESSION TRANSACTION ISOLATION LEVEL $trx_isolation_level;
START TRANSACTION;
INSERT INTO self VALUES(3, 2);

connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT fk1 FROM self WHERE fk1 = 2 FOR UPDATE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT pk FROM self WHERE pk = 3 FOR UPDATE;
rollback;

connection default;
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;

DROP TABLE self;
CREATE TABLE self(pk INT PRIMARY KEY, fk1 INT,
                  FOREIGN KEY(fk1) REFERENCES self(pk));
INSERT INTO self VALUES(1, NULL), (2, 1), (3, 2);
START TRANSACTION;
UPDATE self SET fk1 = 1 WHERE fk1 = 2;
connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT fk1 FROM self WHERE fk1 = 1 FOR UPDATE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT fk1 FROM self WHERE fk1 = 2 FOR UPDATE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT fk1 FROM self WHERE pk = 1 FOR UPDATE;
rollback;

connection default;
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;

DROP TABLE self;
CREATE TABLE self(pk INT PRIMARY KEY, fk1 INT,
                  FOREIGN KEY(fk1) REFERENCES self(pk));
INSERT INTO self VALUES(1, NULL), (2, 1), (3, 2);
START TRANSACTION;
UPDATE self SET pk = 4 WHERE pk = 3;
connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT fk1 FROM self WHERE pk = 3 FOR UPDATE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT fk1 FROM self WHERE pk = 4 FOR UPDATE;
rollback;

connection default;
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;

DROP TABLE self;
CREATE TABLE self(pk INT PRIMARY KEY, fk1 INT,
                  FOREIGN KEY(fk1) REFERENCES self(pk));
INSERT INTO self VALUES(1, NULL), (2, 1), (3, 2);
START TRANSACTION;
DELETE FROM self WHERE pk = 3;
connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT fk1 FROM self WHERE fk1 = 2 FOR UPDATE;
rollback;

connection default;
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;

connection default;
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;

DROP TABLE self;
CREATE TABLE self(pk INT PRIMARY KEY, fk1 INT,
                  FOREIGN KEY(fk1) REFERENCES self(pk)
                      ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO self VALUES(1, NULL), (2, 1), (3, 2), (4, 3);
START TRANSACTION;
DELETE FROM self WHERE pk = 2;
connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT fk1 FROM self WHERE fk1 = 2 FOR UPDATE;
rollback;

connection default;
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;

DROP TABLE self;

SET FOREIGN_KEY_CHECKS = 0;
CREATE TABLE t1(cid INT PRIMARY KEY, eid INT,
                FOREIGN KEY(eid) REFERENCES t2(eid)
                    ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t2(eid INT PRIMARY KEY, cid INT,
                FOREIGN KEY(cid) REFERENCES t1(cid)
                    ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES(1, 10);
INSERT INTO t1 VALUES(2, 20);
INSERT INTO t2 VALUES(10, 1);
INSERT INTO t2 VALUES(20, 2);
SET FOREIGN_KEY_CHECKS = 1;
--source include/wait_innodb_all_purged.inc
SELECT * FROM t1;
SELECT * FROM t2;
START TRANSACTION;
DELETE FROM t1 WHERE cid = 1;
connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT cid FROM t2 WHERE cid = 1 FOR UPDATE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT eid FROM t1 WHERE eid = 10 FOR UPDATE;
rollback;

connection default;
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;

--source include/wait_innodb_all_purged.inc
SELECT * FROM t1;
SELECT * FROM t2;
START TRANSACTION;
UPDATE t1 SET cid = 3 WHERE cid = 2;
connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT cid FROM t2 WHERE cid = 2 FOR UPDATE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT cid FROM t2 WHERE cid = 3 FOR UPDATE;
rollback;

connection default;
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;
DROP TABLE t1, t2;
