eval CREATE TABLE t (a INT, b INT, c INT, d INT, e INT, KEY b(a, b, c, d)) ENGINE=$engine;

--disable_query_log
CREATE TEMPORARY TABLE a (a INT);
let $i=2;
while ($i)
{
  --eval INSERT INTO a VALUES ($i)
  dec $i;
}

CREATE TEMPORARY TABLE b (a INT);
let $i=10;
while ($i)
{
  --eval INSERT INTO b VALUES ($i)
  dec $i;
}

--echo # Since ANALYZE TABLE only reads a subset of the data, the statistics for
--echo # table t depends on the row order. And since the INSERT INTO ... SELECT
--echo # may be executed using different execution plans, we've added ORDER BY
--echo # to ensure that we rows has the same order every time. If not, the
--echo # estimated number of rows in EXPLAIN may change on different platforms.
INSERT INTO t SELECT a.a, b.a, a.a + b.a, 2 * b.a, 2 * a.a FROM a, b
  ORDER BY a.a, b.a;
INSERT INTO t SELECT a, b, b+10, b+20, b FROM t;
INSERT INTO t SELECT a, b, b+20, b+40, b FROM t;
INSERT INTO t SELECT a, b, b+40, b+80, b FROM t;

DROP TEMPORARY TABLE a, b;

# The cost estimates vary slightly between runs. Add extra data to the
# table to increase the difference in cost between a table scan and an
# index scan, so that the choice between those scans is less likely to
# change because of small variations in statistics.
ALTER TABLE t ADD COLUMN data VARCHAR(256) DEFAULT (REPEAT('x', 256));

--enable_query_log

ANALYZE TABLE t;
