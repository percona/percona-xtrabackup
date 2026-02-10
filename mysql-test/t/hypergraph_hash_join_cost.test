--source include/have_hypergraph.inc
--source include/elide_costs.inc

# Tests for WL#16719 "Hypergraph cost model for hash-join with spill-to-disk"

SET @old_histogram_generation_max_mem_size=@@histogram_generation_max_mem_size;
SET @old_join_buffer_size=@@join_buffer_size;
SET @old_cte_max_recursion_depth=@@cte_max_recursion_depth;

DELIMITER //;

# Fetch the net cost of the root operation from a JSON plan.
CREATE FUNCTION root_cost(expl JSON) RETURNS DOUBLE DETERMINISTIC
BEGIN
  RETURN JSON_EXTRACT(expl, "$.query_plan.estimated_total_cost") -
    (SELECT SUM(cost) FROM
    JSON_TABLE(expl, "$.query_plan.inputs[*]"
               COLUMNS (cost DOUBLE PATH '$.estimated_total_cost')) t);
END
//

DELIMITER ;//

CREATE TABLE t1(
  i1 INT NOT NULL,
  i2 INT NOT NULL,
  i3 INT NOT NULL,
  v1 VARCHAR(255),
  KEY k1 (i2)
) ENGINE=InnoDB, CHARSET=latin1, STATS_SAMPLE_PAGES=1000;


SET @t1_rows=10000;
SET cte_max_recursion_depth=@t1_rows;
SET histogram_generation_max_mem_size=@t1_rows*512;

INSERT INTO t1
WITH RECURSIVE qn(n) AS
(SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<@t1_rows-1)
SELECT n, CEIL(RAND(0) * @t1_rows / 2), n DIV 10, REPEAT('X', 255) FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON i1,i3;
ANALYZE TABLE t1;

# Verify that the cost of this hash join is bigger when we need to
# do spill-to-disk.
let $query =
SELECT * FROM t1 build STRAIGHT_JOIN t1 probe ON build.i1=probe.i2;

SET @@join_buffer_size=4*1024*1024;

--replace_regex $elide_costs_and_rows
--eval EXPLAIN FORMAT=TREE $query

--eval EXPLAIN FORMAT=JSON INTO @json $query
SELECT root_cost(@json);

SET @@join_buffer_size=256*1024;

--replace_regex $elide_costs_and_rows
--eval EXPLAIN FORMAT=TREE $query

--eval EXPLAIN FORMAT=JSON INTO @json $query
SELECT root_cost(@json);

let $query =
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1 LEFT JOIN t1 x2
ON x1.i1=x2.i2 WHERE x1.i3<150;

--echo # NLJ should be cheaper than hash join.
--replace_regex $elide_costs_and_rows
--eval $query

--echo # But hash join is cheaper if the projection of the build table is smaller.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT x2.i3 FROM t1 x1 LEFT JOIN t1 x2
ON x1.i1=x2.i2 WHERE x1.i3<150;

SET @@join_buffer_size=1024*1024;

--echo # NLJ should be cheaper than hash join.
--replace_regex $elide_costs_and_rows
--eval $query

SET @@join_buffer_size=4*1024*1024;

--echo # Hash join should be cheaper than NLJ, as there is no spill-to-disk.
--replace_regex $elide_costs_and_rows
--eval $query

DROP TABLE t1;

# For some DELETE statements the optimizer can choose between deleting rows
# immediately as they are scanned, or storing identifiers of rows to be deleted
# while processing the query, and instead deleting them towards the end of
# execution (see the comment on AccessPath::immediate_update_delete_table for
# more details).

# This replaces unit tests
# HypergraphOptimizerTest.DeletePreferImmediate and
# HypergraphOptimizerTest.UpdatePreferImmediate which would no longer
# work, as the query there would now give a hash join rather than NLJ.

CREATE TABLE t1(
  x INT NOT NULL PRIMARY KEY
);

CREATE TABLE t2(
  x INT NOT NULL
);

INSERT INTO t1
WITH RECURSIVE qn(n) AS (SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<99)
SELECT n FROM qn;

INSERT INTO t2 select * from t1 WHERE t1.x<10;

ANALYZE TABLE t1,t2;

# These statements do a NLJ with t1 is the inner table, and then delete
# rows from t1 (buffered) or t2 (directly).
EXPLAIN FORMAT=JSON INTO @buffered DELETE t1 FROM t1, t2 WHERE t1.x=t2.x;
EXPLAIN FORMAT=JSON INTO @direct DELETE t2 FROM t1, t2 WHERE t1.x=t2.x;

SELECT JSON_EXTRACT(@buffered, "$.query_plan.operation") buffered,
JSON_EXTRACT(@direct, "$.query_plan.operation") direct;

--echo # Direct delete should be cheaper than buffered.
SELECT root_cost(@direct) < 0.5 * root_cost(@buffered);

# These statements do a NLJ with t1 is the inner table, and then update
# rows from t1 (buffered) or t2 (directly).
EXPLAIN FORMAT=JSON INTO @buffered
UPDATE t1, t2 SET t1.x = t1.x + 1 WHERE t1.x = t2.x;

EXPLAIN FORMAT=JSON INTO @direct
UPDATE t1, t2 SET t2.x = t2.x + 1 WHERE t1.x = t2.x;

SELECT JSON_EXTRACT(@buffered, "$.query_plan.operation") buffered,
JSON_EXTRACT(@direct, "$.query_plan.operation") direct;

--echo # Direct update should be cheaper than buffered.
SELECT root_cost(@direct) < 0.5 * root_cost(@buffered);

DROP TABLE t1,t2;

DROP FUNCTION root_cost;

# Test of rewrite semijoin to inner join with deduplicated inner side.
CREATE VIEW num_1e4 AS
WITH RECURSIVE qn(n) AS (SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<9999) SELECT n FROM qn;

CREATE TABLE t1 (
  pk INT PRIMARY KEY,
  a INT,
  KEY ka(a)
);

CREATE TABLE t2 (
  pk INT PRIMARY KEY,
  a INT
);

INSERT INTO t1 SELECT n, n%10 FROM  num_1e4 LIMIT 400;
INSERT INTO t2 SELECT n, n%10 FROM  num_1e4 LIMIT 100;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON a;
ANALYZE TABLE t2 UPDATE HISTOGRAM ON a;
ANALYZE TABLE t1, t2;

--echo # Rewrite to inner join with deduplicated t1.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t2 WHERE a IN (SELECT a FROM t1);

--echo # No rewrite to inner join.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t2 WHERE a IN (SELECT a FROM t2);

DROP TABLE t1, t2;
DROP VIEW num_1e4;

SET @@join_buffer_size=@old_join_buffer_size;
SET @@cte_max_recursion_depth=@old_cte_max_recursion_depth;
SET @@histogram_generation_max_mem_size=@old_histogram_generation_max_mem_size;

--source include/disable_hypergraph.inc
