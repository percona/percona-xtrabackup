--echo #
--echo # BIT type checks
--echo #

--echo # InnoDB table

CREATE TABLE t1 (a BIT(7), b INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (64, 1), (65, 2), (65, 3), (NULL, NULL), (66, 4);
ANALYZE TABLE t1;

let $query= SELECT COUNT(a) FROM t1;
eval $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

let $query= SELECT SUM(b) FROM t1 GROUP BY a ORDER BY SUM(b);
eval $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
DROP TABLE t1;

--echo # MyISAM table

CREATE TABLE t1 (a BIT(7), b INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (64, 1), (65, 2), (65, 3), (NULL, NULL), (66, 4);
ANALYZE TABLE t1;

let $query= SELECT COUNT(a) FROM t1;
eval $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

let $query= SELECT SUM(b) FROM t1 GROUP BY a ORDER BY SUM(b);
eval $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
DROP TABLE t1;

--echo # HEAP table

CREATE TABLE t1 (a BIT(7), b INT) ENGINE=HEAP;
INSERT INTO t1 VALUES (64, 1), (65, 2), (65, 3), (NULL, NULL), (66, 4);
ANALYZE TABLE t1;

let $query= SELECT COUNT(a) FROM t1;
eval $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

let $query= SELECT SUM(b) FROM t1 GROUP BY a ORDER BY SUM(b);
eval $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
DROP TABLE t1;

