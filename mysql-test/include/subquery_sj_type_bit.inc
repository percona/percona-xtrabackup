#
# BIT type checks
#
CREATE TABLE t1 (a BIT(7)) ENGINE=InnoDB;
INSERT INTO t1 VALUES (64),(65),(65),(NULL),(66);
CREATE TABLE t2 (a INT)  ENGINE=InnoDB;
INSERT INTO t2 VALUES (61),(64),(65);
ANALYZE TABLE t1, t2;
let $query= SELECT * FROM t2 WHERE a IN (SELECT a FROM t1);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
let $query= SELECT * FROM t1 WHERE a IN (SELECT a FROM t2);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
let $query= SELECT * FROM t1 WHERE a IN (SELECT a FROM t1);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
DROP TABLE t1, t2;

CREATE TABLE t1 (a BIT(7)) ENGINE=MyISAM;
INSERT INTO t1 VALUES (64),(65),(65),(NULL),(66);
CREATE TABLE t2 (a INT)  ENGINE=MyISAM;
INSERT INTO t2 VALUES (61),(64),(65);
ANALYZE TABLE t1, t2;
let $query= SELECT * FROM t2 WHERE a IN (SELECT a FROM t1);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
let $query= SELECT * FROM t1 WHERE a IN (SELECT a FROM t2);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
let $query= SELECT * FROM t1 WHERE a IN (SELECT a FROM t1);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
DROP TABLE t1, t2;

CREATE TABLE t1 (a BIT(7)) ENGINE=HEAP;
INSERT INTO t1 VALUES (64),(65),(65),(NULL),(66);
CREATE TABLE t2 (a INT)  ENGINE=HEAP;
INSERT INTO t2 VALUES (61),(64),(65);
ANALYZE TABLE t1, t2;
let $query= SELECT * FROM t2 WHERE a IN (SELECT a FROM t1);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
let $query= SELECT * FROM t1 WHERE a IN (SELECT a FROM t2);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
let $query= SELECT * FROM t1 WHERE a IN (SELECT a FROM t1);
eval $query;
eval EXPLAIN $query;
--skip_if_hypergraph
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
DROP TABLE t1, t2;
