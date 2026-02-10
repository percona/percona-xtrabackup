connection default;
CREATE TABLE parent(f1 INT PRIMARY KEY, f2 INT);
CREATE TABLE child(f2 INT PRIMARY KEY, f1 INT,
                   FOREIGN KEY(f1) REFERENCES parent(f1)
                       ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO parent VALUES(10, 10), (20, 20);
INSERT INTO child VALUES(10, 10), (20, 20);

eval SET SESSION TRANSACTION ISOLATION LEVEL $trx_isolation_level;
START TRANSACTION;
UPDATE parent SET f1 = 11 WHERE f1 = 10;

connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT f1 FROM child WHERE f1 = 10 FOR UPDATE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT f1 FROM child WHERE f1 = 11 FOR UPDATE;
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
DELETE FROM parent WHERE f1 = 11;

connection con1;
START TRANSACTION;
--error ER_LOCK_WAIT_TIMEOUT
SELECT f1 FROM child WHERE f1 = 11 FOR UPDATE;
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
