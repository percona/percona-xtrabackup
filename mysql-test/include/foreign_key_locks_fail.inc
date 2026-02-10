connection default;
CREATE TABLE parent(f1 INT PRIMARY KEY, f2 INT);
CREATE TABLE child(f2 INT PRIMARY KEY, f1 INT,
                   FOREIGN KEY(f1) REFERENCES parent(f1));
INSERT INTO parent VALUES(10, 10);
INSERT INTO child VALUES(10, 10);

eval SET SESSION TRANSACTION ISOLATION LEVEL $trx_isolation_level;
START TRANSACTION;
--error ER_NO_REFERENCED_ROW_2
INSERT INTO child VALUES(20, 20);
SELECT object_schema, object_name, lock_type, lock_duration,
    lock_status FROM performance_schema.metadata_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5;
SELECT object_schema, object_name, index_name, lock_type, lock_mode,
    lock_status,
    lock_data FROM performance_schema.data_locks WHERE object_schema =
        'test' order by 1, 2, 3, 4, 5, 6, 7;
COMMIT;

START TRANSACTION;
--error ER_NO_REFERENCED_ROW_2
UPDATE child SET f1 = 11 WHERE f1 = 10;
connection con1;
SET innodb_lock_wait_timeout = 2;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT f1 FROM child WHERE f1 = 10 FOR UPDATE;
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

START TRANSACTION;
--error ER_ROW_IS_REFERENCED_2
UPDATE parent SET f1 = 11 WHERE f1 = 10;
connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT f1 FROM parent WHERE f1 = 10 FOR UPDATE;
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

START TRANSACTION;
--error ER_ROW_IS_REFERENCED_2
DELETE FROM parent WHERE f1 = 10;
connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT f1 FROM parent WHERE f1 = 10 FOR UPDATE;
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
DROP TABLE child, parent;
