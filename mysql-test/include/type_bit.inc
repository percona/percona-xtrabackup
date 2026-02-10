--echo #
--echo # Bug#36846564 Window function applied to BIT column produces non-BIT type
--echo #              Same issue with materialized derived table
--echo #

--echo # Verify DISTINCT of type BIT in different contexts as we no
--echo # longer treat BIT as integer in tmp tables; it now retains its
--echo # BIT type identity throughout.
CREATE TABLE t1 (a INT, b BIT(2));
INSERT INTO t1 VALUES (3, 2), (2, 3), (2, 0), (3, 2), (3, 1);
SELECT COUNT(DISTINCT b) FROM t1 GROUP BY a;
SELECT HEX(GROUP_CONCAT(DISTINCT b)) FROM t1 GROUP BY a;
CREATE TABLE t2 AS SELECT SQL_SMALL_RESULT DISTINCT b FROM t1;
SELECT HEX(t2.b) FROM t2;
SELECT COUNT(a) AS cnt FROM t1 GROUP BY b ORDER BY cnt;
SELECT a, HEX(b) FROM t1 ORDER BY b;
SELECT HEX(b), COUNT(a) OVER (PARTITION BY b) FROM t1;
DROP TABLE t1, t2;

