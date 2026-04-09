--echo # FR 7: When foreign_key_checks=0, do not perform FK check, RESTRICT, or CASCADE

CREATE TABLE parent(id INT PRIMARY KEY, value INT);
CREATE TABLE child(id INT PRIMARY KEY, pid INT,
FOREIGN KEY (pid) REFERENCES parent(id) ON UPDATE CASCADE ON DELETE CASCADE);

INSERT INTO parent VALUES (1, 100), (2, 200), (3, 300);
INSERT INTO child  VALUES (10, 1), (11, 2);

--echo # FK enforced by default value of FKC
--error ER_NO_REFERENCED_ROW_2
INSERT INTO child VALUES (12, 999);

--echo # Disable FKC
SET SESSION foreign_key_checks = 0;

--echo # With FKC off: allow orphan insert
INSERT INTO child VALUES (12, 999);
SELECT * FROM child ORDER BY id;

--echo # With FKC off: allow UPDATE to non-existent parent
UPDATE child SET pid = 888 WHERE id = 12;
SELECT * FROM child WHERE id=12;

--echo # With FKC off: parent DELETE should NOT cascade; children referencing it remain
DELETE FROM parent WHERE id = 1;
SELECT * FROM parent ORDER BY id;
SELECT * FROM child ORDER BY id;

--echo # With FKC off: parent UPDATE should NOT cascade to children
UPDATE parent SET id = 200 WHERE id = 2;
SELECT * FROM parent ORDER BY id;
--echo # Child row referencing former id=2 should still point to 2 (no cascade happened)
SELECT * FROM child WHERE id=11;

--echo # Re-enable FKC to on
SET SESSION foreign_key_checks = 1;

--echo # With FKC on: illegal orphan insert/update must fail
--error ER_NO_REFERENCED_ROW_2
INSERT INTO child VALUES (13, 777);

--error ER_NO_REFERENCED_ROW_2
UPDATE child SET pid = 777 WHERE id = 12;

--echo # With FKC on: DELETE should cascade now (id=3)
INSERT INTO child VALUES (20, 3);
DELETE FROM parent WHERE id = 3;
SELECT * FROM parent ORDER BY id;
SELECT * FROM child ORDER BY id;

--echo # Cleanup
DROP TABLE child;
DROP TABLE parent;
