--source include/elide_costs.inc

--echo WL#13052: Transform table subqueries using derived tables

CREATE TABLE it
 (pk    INTEGER PRIMARY KEY,
  val   INTEGER NOT NULL,
  valn  INTEGER,
  nulls INTEGER
);

CREATE TABLE ot(
  v INTEGER NOT NULL,
  vn INTEGER);

CREATE TABLE itr
 (rf    INTEGER,
  pki   INTEGER,
  val   INTEGER NOT NULL,
  valn  INTEGER,
  nulls INTEGER,
  PRIMARY KEY(rf, pki)
);

CREATE TABLE otr(
  rf INTEGER,
  v INTEGER NOT NULL,
  vn INTEGER);

CREATE TABLE t0row(v INTEGER NOT NULL);

CREATE TABLE t1row(v INTEGER NOT NULL, n INTEGER);

CREATE TABLE t2row(v INTEGER NOT NULL, vn INTEGER, n INTEGER);

CREATE TABLE t3row(v INTEGER NOT NULL, vn INTEGER, n INTEGER);

CREATE TABLE t4row(v INTEGER NOT NULL, vn INTEGER, n INTEGER);

CREATE TABLE t_null(i INTEGER);

CREATE TABLE t_outer(i INTEGER NOT NULL);

CREATE TABLE t_empty(i INTEGER);

INSERT INTO it(pk, val, valn, nulls) VALUES
 (1, 1,    1, NULL),
 (2, 2,    2, NULL),
 (3, 3, NULL, NULL);

INSERT INTO ot VALUES (0, 0), (1, 1), (2, 2), (3, 3), (4, NULL);

INSERT INTO itr SELECT 1, pk, val, valn, nulls FROM it;
INSERT INTO itr SELECT 2, pk, val, valn, nulls FROM it;

INSERT INTO otr SELECT 1, v, vn FROM ot;
INSERT INTO otr SELECT 2, v, vn FROM ot;
INSERT INTO otr SELECT 3, v, vn FROM ot;

INSERT INTO t1row VALUES (2, NULL);

INSERT INTO t2row VALUES (2, 2, NULL), (2, NULL, NULL);

INSERT INTO t3row VALUES (2, 2, NULL), (2, 2, NULL), (2, NULL, NULL);

INSERT INTO t4row VALUES
    (2, 2, NULL), (2, 2, NULL), (2, NULL, NULL), (3, 3, NULL);

INSERT INTO t_null VALUES (NULL), (1), (1), (2);

INSERT INTO t_outer VALUES (1), (1), (2);

ANALYZE TABLE it, ot, itr, otr, t0row, t1row, t2row, t3row, t4row, t_null, t_outer, t_empty;

--echo
--echo # -------------------------------------------------------------------#
--echo # Test quantified comparison predicates with subquery_to_derived OFF #
--echo # -------------------------------------------------------------------#
--echo

--source include/subquery_allany.inc

set optimizer_switch='semijoin=off,subquery_to_derived=on';

--echo
--echo # -------------------------------------------------------------------#
--echo # Test quantified comparison predicates with subquery_to_derived ON  #
--echo # -------------------------------------------------------------------#
--echo

--source include/subquery_allany.inc

--echo
--echo # Bug#37533056: WL#13052: Query is transformed where inner query block
--echo #               contains outer references inside an inequality clause.
--echo

SELECT rf, v, v <>ALL (SELECT val FROM itr WHERE otr.rf < itr.rf) AS b FROM otr;

SELECT rf, v, v NOT IN (SELECT val FROM itr WHERE otr.rf < itr.rf) AS b
FROM otr;

--error ER_SUBQUERY_TRANSFORM_REJECTED
SELECT rf, v, v =ANY (SELECT val FROM itr WHERE otr.rf < itr.rf) AS b FROM otr;

--error ER_SUBQUERY_TRANSFORM_REJECTED
SELECT rf, v, v IN (SELECT val FROM itr WHERE otr.rf < itr.rf) AS b FROM otr;

--error ER_SUBQUERY_TRANSFORM_REJECTED
SELECT rf, v, v <ALL (SELECT val FROM itr WHERE otr.rf > itr.rf) AS b FROM otr;

--error ER_SUBQUERY_TRANSFORM_REJECTED
SELECT rf, v, v >ALL (SELECT val FROM itr WHERE otr.rf <> itr.rf) AS b FROM otr;

--error ER_SUBQUERY_TRANSFORM_REJECTED
SELECT rf, v, v <ANY (SELECT val FROM itr WHERE otr.rf < itr.rf) AS b FROM otr;

--error ER_SUBQUERY_TRANSFORM_REJECTED
SELECT rf, v, v >ANY (SELECT val FROM itr WHERE otr.rf <> itr.rf) AS b FROM otr;

--error ER_SUBQUERY_TRANSFORM_REJECTED
SELECT rf, v, v =ALL (SELECT val FROM itr WHERE otr.rf < itr.rf) AS b FROM otr;

--error ER_SUBQUERY_TRANSFORM_REJECTED
SELECT rf, v, v <>ANY (SELECT val FROM itr WHERE otr.rf > itr.rf) AS b FROM otr;

set optimizer_switch=default;

DROP TABLE it, ot, itr, otr, t0row, t1row, t2row, t3row, t4row;
DROP TABLE t_outer, t_null, t_empty;
