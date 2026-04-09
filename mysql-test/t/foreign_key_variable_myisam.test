--source include/have_myisam.inc

--echo # FR 8.3) innodb_native_foreign_keys must not affect FK handling for other storage
--echo # engine tables

--echo # Phase A: OFF (default)
SHOW VARIABLES LIKE 'innodb_native_foreign_keys';

CREATE TABLE t1(f1 INT PRIMARY KEY) ENGINE=MyISAM;
CREATE TABLE t2(f1 INT, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON UPDATE CASCADE ON DELETE CASCADE) ENGINE=MyISAM;

--echo # Insert child pointing to non-existent parent -> should succeed
INSERT INTO t2 VALUES (1, 999);

--echo # Update parent -> no cascade to child
INSERT INTO t1 VALUES (10);
INSERT INTO t2 VALUES (10, 10);
UPDATE t1 SET f1 = 20 WHERE f1 = 10;

--echo # Delete parent -> no cascade
DELETE FROM t1 WHERE f1 = 20;

SELECT * FROM t1;
SELECT * FROM t2;

--echo # MyISAM ignores FK metadata
SHOW CREATE TABLE t2;

--echo # Phase B: ON
--let $restart_parameters=restart: --innodb_native_foreign_keys=ON
--source include/restart_mysqld.inc
SHOW VARIABLES LIKE 'innodb_native_foreign_keys';

DROP TABLE IF EXISTS t1, t2;
CREATE TABLE t1(f1 INT PRIMARY KEY) ENGINE=MyISAM;
CREATE TABLE t2(f1 INT, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON UPDATE CASCADE ON DELETE CASCADE) ENGINE=MyISAM;

--echo # Insert child pointing to non-existent parent -> should succeed
INSERT INTO t2 VALUES (1, 999);

--echo # Update parent -> no cascade to child
INSERT INTO t1 VALUES (10);
INSERT INTO t2 VALUES (10, 10);
UPDATE t1 SET f1 = 20 WHERE f1 = 10;

--echo # Delete parent -> no cascade
DELETE FROM t1 WHERE f1 = 20;

SELECT * FROM t1;
SELECT * FROM t2;
--echo # MyISAM ignores FK metadata
SHOW CREATE TABLE t2;

DROP TABLE t1, t2;
