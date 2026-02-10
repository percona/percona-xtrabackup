# This test takes rather long time so let us run it only in --big-test mode
--source include/big_test.inc
# Costs and therefore plan choices depend on the block size.
--source include/have_innodb_16k.inc
--source include/have_hypergraph.inc
--source include/elide_costs.inc

# Tests for WL#16109 "Hypergraph cost model for large data (including IO cost)".

SET @old_cte_max_recursion_depth=@@cte_max_recursion_depth;
SET SESSION cte_max_recursion_depth=110000;

CREATE VIEW num_1e5 AS
WITH RECURSIVE qn(n) AS (SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<1e5)
SELECT n FROM qn;

CREATE TABLE t1(
  pk INT NOT NULL PRIMARY KEY,
  a INT NOT NULL,
  b INT NOT NULL,
  fill VARCHAR(500) NOT NULL,
  KEY k1(a)
) ENGINE=InnoDB, CHARSET=latin1;

INSERT INTO t1
SELECT n, FLOOR(RAND(0)*1e5), FLOOR(RAND(1)*1e5), REPEAT('A',500) FROM num_1e5;

SET SESSION cte_max_recursion_depth= @old_cte_max_recursion_depth;
DROP VIEW num_1e5;

ANALYZE TABLE t1;

# If the buffer pool has a different size, we will get other costs and
# possibly different plans.
SELECT @@innodb_buffer_pool_size=24*1024*1024;

# Range scan expected.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT b FROM t1 WHERE a<10;

# Range scan with MRR.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT b FROM t1 WHERE a<1000;

# Table scan.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT b FROM t1 WHERE a<3000;

# Nested loop join expected.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1, t1 x2 WHERE x1.pk<10 AND x1.b=x2.a;

# Hash join expected.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1, t1 x2 WHERE x1.pk<1000 AND x1.b=x2.a;

# Hash join expected.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1, t1 x2 WHERE x1.pk<3000 AND x1.b=x2.a;

DROP TABLE t1;


--source include/disable_hypergraph.inc
