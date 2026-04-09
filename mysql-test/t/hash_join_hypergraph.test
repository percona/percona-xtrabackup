--source include/have_hypergraph.inc
--source include/hash_join.inc
--source include/elide_costs.inc

--echo #
--echo # Bug#34940000 Hash join execution may be ineficcient if probe input is empty
--echo #

CREATE TABLE t1(
  a INT NOT NULL PRIMARY KEY,
  b INT NOT NULL,
  c INT NOT NULL
);

INSERT INTO t1
WITH RECURSIVE qn(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM qn WHERE n<20)
SELECT n, n%7, n%5 FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b,c;
ANALYZE TABLE t1;

# We should check if the probe (outer) input is empty first if it is an innerjoin \ or semijoin
# where reading the first row from the probe is cheaper than from the build (inner) input.
# If the probe is empty, we can skip reading the build input.
# Note: Plan change under new cost model, but still shows (never) executed from the inner input.

# Inner join.
 --replace_regex $elide_costs_and_time
 EXPLAIN ANALYZE SELECT 1 FROM t1 JOIN
  (SELECT SUM(x1.b) s FROM t1 x1, t1 x2 WHERE x1.b<x2.b GROUP BY x1.c) d1
  ON c=d1.s AND c*2<0;

# Semijoin.
--replace_regex $elide_costs_and_time
 EXPLAIN ANALYZE SELECT 1 FROM t1 x1 WHERE b IN
   (SELECT c FROM t1 x2 WHERE b>0) AND x1.c*2<0;

DROP TABLE t1;

--echo #
--echo # Bug#34155137 STRAIGHT_JOIN GIVES DIFFERENT ORDER FOR HASH JOIN WITH HYPERGRAPH
--echo #

CREATE TABLE small_tab(x INT);
INSERT INTO small_tab VALUES (1), (2), (3), (4), (5), (6);

CREATE TABLE large_tab(x INT);
INSERT INTO large_tab SELECT x FROM small_tab;
INSERT INTO large_tab SELECT x FROM large_tab;
INSERT INTO large_tab SELECT x FROM large_tab;
INSERT INTO large_tab SELECT x FROM large_tab;
INSERT INTO large_tab SELECT x FROM large_tab;

SELECT COUNT(*) FROM small_tab;
SELECT COUNT(*) FROM large_tab;

ANALYZE TABLE large_tab;
ANALYZE TABLE small_tab;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT x FROM large_tab STRAIGHT_JOIN small_tab USING (x);
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT x FROM small_tab STRAIGHT_JOIN large_tab USING (x);

DROP TABLE large_tab;
DROP TABLE small_tab;

--echo #
--echo # Bug#38385568	Hypergraph: inefficient join order for hash join
--echo #

CREATE TABLE narrow (a INT);
CREATE TABLE wide (a INT, c VARCHAR(2048));

INSERT INTO narrow
  WITH RECURSIVE qn(n) AS (
    SELECT 1 UNION ALL SELECT n+1 FROM qn WHERE n < 256
  )
  SELECT n FROM qn;

INSERT INTO wide SELECT a, REPEAT('X', 2048) FROM narrow LIMIT 240;

ANALYZE TABLE narrow,wide;

--echo # Use 'narrow' as the build-relation, since that is smaller.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT wide.c FROM narrow,wide WHERE narrow.a=wide.a;

--echo # Use 'wide' as the build-relation, since that is smaller.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT 1 FROM narrow,wide WHERE narrow.a=wide.a;

DROP TABLE narrow,wide;

eval INSTALL PLUGIN mock SONAME '$MOCK_PLUGIN';

SET @old_threshold = @@session.secondary_engine_cost_threshold;
SET secondary_engine_cost_threshold = 0;


CREATE TABLE t1 (x INT) SECONDARY_ENGINE MOCK;
ALTER TABLE t1 SECONDARY_LOAD;
ANALYZE TABLE t1;

SET SESSION OPTIMIZER_TRACE="enabled=on";

--echo # We should only propose one join order to a secondary engine.
SELECT 1 FROM t1 x1 JOIN t1 x2 ON x1.x=x2.x;

# Propose exactly one hash join.
SELECT REGEXP_SUBSTR(trace, "- [{]HASH_JOIN",1,1,'n') IS NULL first,
REGEXP_SUBSTR(trace, "- [{]HASH_JOIN",1,2,'n') IS NULL second
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

SET SESSION OPTIMIZER_TRACE="enabled=off";

DROP TABLE t1;

UNINSTALL PLUGIN mock;

SET @@session.secondary_engine_cost_threshold = @old_threshold;

--source include/disable_hypergraph.inc
