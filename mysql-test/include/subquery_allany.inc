--echo
--echo # quantified comparison predicate in SELECT list
--echo # non-nullable columns
--echo # no dependent subquery predicate
--echo

let $query=
SELECT v <>ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with NOT EXISTS:
let $query=
SELECT NOT EXISTS (SELECT * FROM it WHERE ot.v = it.val) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >=ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <=ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;


# The equivalent query with EXISTS:
let $query=
SELECT EXISTS (SELECT * FROM it WHERE ot.v = it.val) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >=ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <=ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ALL (SELECT v FROM t0row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ALL (SELECT v FROM t1row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ALL (SELECT v FROM t2row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ALL (SELECT v FROM t4row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <>ANY (SELECT v FROM t0row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <>ANY (SELECT v FROM t1row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <>ANY (SELECT v FROM t2row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <>ANY (SELECT v FROM t4row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo
--echo # quantified comparison predicate in SELECT list
--echo # nullable outer column
--echo # no dependent subquery predicate
--echo

let $query=
SELECT vn <>ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with NOT EXISTS (with NULL values filtered out):
let $query=
SELECT NOT EXISTS (SELECT * FROM it WHERE ot.vn = it.val) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn >ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn >=ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <=ALL (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with EXISTS (with NULL values filtered out):
let $query=
SELECT EXISTS (SELECT * FROM it WHERE ot.vn = it.val) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn >ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn >=ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <=ANY (SELECT val FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ALL (SELECT v FROM t0row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ALL (SELECT v FROM t1row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ALL (SELECT v FROM t2row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ALL (SELECT v FROM t4row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <>ANY (SELECT v FROM t0row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <>ANY (SELECT v FROM t1row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <>ANY (SELECT v FROM t2row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <>ANY (SELECT v FROM t4row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo
--echo # quantified comparison predicate in SELECT list
--echo # nullable column in subquery
--echo # no dependent subquery predicate
--echo

let $query=
SELECT v <>ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with NOT EXISTS (with NULL values filtered out):
let $query=
SELECT NOT EXISTS (SELECT * FROM it WHERE ot.v = it.valn) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >=ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <=ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with EXISTS (with NULL values filtered out):
let $query=
SELECT EXISTS (SELECT * FROM it WHERE ot.v = it.valn) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >=ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <=ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ALL (SELECT n FROM t1row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ALL (SELECT vn FROM t2row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ALL (SELECT vn FROM t3row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v =ALL (SELECT vn FROM t4row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <>ANY (SELECT n FROM t1row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <>ANY (SELECT vn FROM t2row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <>ANY (SELECT vn FROM t3row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v <>ANY (SELECT vn FROM t4row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo
--echo # quantified comparison predicate in SELECT list
--echo # nullable columns
--echo # no dependent subquery predicate
--echo

let $query=
SELECT vn <>ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with NOT EXISTS (with NULL values filtered out):
let $query=
SELECT NOT EXISTS (SELECT * FROM it WHERE ot.vn = it.valn) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn >ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn >=ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <=ALL (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with EXISTS (with NULL values filtered out):
let $query=
SELECT EXISTS (SELECT * FROM it WHERE ot.vn = it.valn) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn >ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn >=ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <=ANY (SELECT valn FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ALL (SELECT n FROM t1row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ALL (SELECT vn FROM t2row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ALL (SELECT vn FROM t3row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn =ALL (SELECT vn FROM t4row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <>ANY (SELECT n FROM t1row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <>ANY (SELECT vn FROM t2row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <>ANY (SELECT vn FROM t3row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT vn <>ANY (SELECT vn FROM t4row) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo
--echo # quantified comparison predicate in WHERE clause
--echo # non-nullable columns
--echo # no dependent subquery predicate
--echo

let $query=
SELECT *
FROM ot
WHERE v <>ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with NOT EXISTS:
let $query=
SELECT *
FROM ot
WHERE NOT EXISTS (SELECT * FROM it WHERE ot.v = it.val);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >=ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <=ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with EXISTS:
let $query=
SELECT *
FROM ot
WHERE EXISTS (SELECT * FROM it WHERE ot.v = it.val);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >=ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <=ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ALL (SELECT v FROM t0row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ALL (SELECT v FROM t1row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ALL (SELECT v FROM t2row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ALL (SELECT v FROM t4row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <>ANY (SELECT v FROM t0row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <>ANY (SELECT v FROM t1row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <>ANY (SELECT v FROM t2row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <>ANY (SELECT v FROM t4row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo
--echo # quantified comparison predicate in WHERE clause
--echo # nullable outer column
--echo # no dependent subquery predicate
--echo

let $query=
SELECT *
FROM ot
WHERE vn <>ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with NOT EXISTS (with NULL values filtered out):
let $query=
SELECT *
FROM ot
WHERE NOT EXISTS (SELECT * FROM it WHERE ot.vn = it.val);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn >ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn >=ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <=ALL (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with EXISTS:
let $query=
SELECT *
FROM ot
WHERE EXISTS (SELECT * FROM it WHERE ot.vn = it.val);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn >ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn >=ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <=ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ALL (SELECT v FROM t0row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ALL (SELECT v FROM t1row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ALL (SELECT v FROM t2row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ALL (SELECT v FROM t4row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <>ANY (SELECT v FROM t0row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <>ANY (SELECT v FROM t1row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <>ANY (SELECT v FROM t2row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <>ANY (SELECT v FROM t4row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo
--echo # quantified comparison predicate in WHERE clause
--echo # nullable column in subquery
--echo # no dependent subquery predicate
--echo

let $query=
SELECT *
FROM ot
WHERE v <>ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with NOT EXISTS (with NULL values filtered out):
let $query=
SELECT *
FROM ot
WHERE NOT EXISTS (SELECT * FROM it WHERE ot.v = it.valn);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >=ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <=ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with EXISTS:
let $query=
SELECT *
FROM ot
WHERE EXISTS (SELECT * FROM it WHERE ot.v = it.valn);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >=ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <=ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ALL (SELECT n FROM t1row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ALL (SELECT vn FROM t2row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ALL (SELECT vn FROM t3row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v =ALL (SELECT vn FROM t4row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <>ANY (SELECT n FROM t1row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <>ANY (SELECT vn FROM t2row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <>ANY (SELECT vn FROM t3row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v <>ANY (SELECT vn FROM t4row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo
--echo # quantified comparison predicate in WHERE clause
--echo # nullable columns
--echo # no dependent subquery predicate
--echo

let $query=
SELECT *
FROM ot
WHERE vn <>ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with NOT EXISTS (with NULL values filtered out):
let $query=
SELECT *
FROM ot
WHERE NOT EXISTS (SELECT * FROM it WHERE ot.vn = it.valn);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn >ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn >=ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <=ALL (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

# The equivalent query with EXISTS:
let $query=
SELECT *
FROM ot
WHERE EXISTS (SELECT * FROM it WHERE ot.vn = it.valn);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn >ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn >=ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <=ANY (SELECT valn FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ALL (SELECT n FROM t1row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ALL (SELECT vn FROM t2row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ALL (SELECT vn FROM t3row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn =ALL (SELECT vn FROM t4row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <>ANY (SELECT n FROM t1row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <>ANY (SELECT vn FROM t2row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <>ANY (SELECT vn FROM t3row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE vn <>ANY (SELECT vn FROM t4row);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo #
--echo # Tests with moderated outer query
--echo

--echo # First, a non-moderated outer query:

let $query=
SELECT v >=ALL (SELECT val FROM it) AS b
FROM ot;
--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # DISTINCT in SELECT list:

let $query=
SELECT DISTINCT v >=ALL (SELECT val FROM it) AS b
FROM ot;
--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # GROUP BY on quantified comparison predicate

let $query=
SELECT v >=ALL (SELECT val FROM it) AS b, COUNT(*)
FROM ot
GROUP BY b;
--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # GROUP BY and HAVING on quantified comparison predicate

let $query=
SELECT v >=ALL (SELECT val FROM it) AS b, COUNT(*)
FROM ot
GROUP BY b
HAVING b > 0;
--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # ORDER BY on quantified comparison predicate

let $query=
SELECT v >=ALL (SELECT val FROM it) AS b
FROM ot
ORDER BY b;
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # GROUP BY and ORDER BY on quantified comparison predicate

let $query=
SELECT v >=ALL (SELECT val FROM it) AS b, COUNT(*)
FROM ot
GROUP BY b
ORDER BY b DESC;
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # Quantified comparison predicate as WINDOW expression

let $query=
SELECT v >=ALL (SELECT val FROM it) AS b,
       SUM((SELECT b)) OVER () AS s
FROM ot;
--sorted_result
--error 0, ER_SUBQUERY_TRANSFORM_REJECTED
eval $query;
--replace_regex $elide_costs
--error 0, ER_SUBQUERY_TRANSFORM_REJECTED
eval explain format=tree $query;

let $query=
SELECT v >=ALL (SELECT val FROM it) AS b,
       SUM((SELECT b)) OVER (PARTITION BY (SELECT b)) AS s
FROM ot;
--sorted_result
--error 0, ER_SUBQUERY_TRANSFORM_REJECTED
eval $query;
--replace_regex $elide_costs
--error 0, ER_SUBQUERY_TRANSFORM_REJECTED
eval explain format=tree $query;

--echo #
--echo # Some negative testing
--echo #

--echo # No tables in outer query block

let $query=
SELECT 1, 1 >ANY (SELECT val FROM it) AS q1, 2 >ANY (SELECT val FROM it) AS q2;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT 1 WHERE 2 >ANY (SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # No tables in inner query block

let $query=
SELECT 1, 1 >ANY (SELECT 0) AS q1 FROM ot;

--sorted_result
eval $query;     
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT 1 FROM ot WHERE 1 >ANY (SELECT 0);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # Set operation in subquery

let $query=
SELECT v >ALL (SELECT val FROM it UNION SELECT val FROM it) AS b
FROM ot; 

--sorted_result
eval $query;     
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT val FROM it UNION SELECT val FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # Implicitly grouped subquery

let $query=
SELECT v >ALL (SELECT MAX(val) FROM it) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT MAX(val) FROM it);

--sorted_result    
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # Explicitly grouped subquery

let $query=
SELECT v >ALL (SELECT val FROM it GROUP BY val) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT val FROM it GROUP BY val);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >ALL (SELECT MAX(valn) FROM it GROUP BY val) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT MAX(valn) FROM it GROUP BY val);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # Subquery with WINDOW function

let $query=
SELECT v >ALL (SELECT MAX(val) OVER () FROM it) AS b
FROM ot;

--sorted_result
--error 0, ER_NOT_SUPPORTED_YET
eval $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT MAX(val) OVER () FROM it);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # Subquery with LIMIT (unsupported) and ORDER BY (supported)

let $query=
SELECT v >ALL (SELECT val FROM it LIMIT 3) AS b
FROM ot;

--sorted_result
--error ER_NOT_SUPPORTED_YET
eval $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT val FROM it LIMIT 3);

--sorted_result
--error ER_NOT_SUPPORTED_YET
eval $query;

let $query=
SELECT v >ALL (SELECT val FROM it ORDER BY val DESC) AS b
FROM ot;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT val FROM it ORDER BY val DESC);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT v >ALL (SELECT val FROM it ORDER BY val DESC LIMIT 3) AS b
FROM ot;

--sorted_result
--error ER_NOT_SUPPORTED_YET
eval $query;

let $query=
SELECT *
FROM ot
WHERE v >ALL (SELECT val FROM it ORDER BY val DESC LIMIT 3);

--sorted_result
--error ER_NOT_SUPPORTED_YET
eval $query;

--echo # Subquery with outer reference in SELECT list

let $query=
SELECT rf, v, v <>ALL (SELECT val FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, v, v =ALL (SELECT val FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, v, v >ALL (SELECT val FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, v, v <>ANY (SELECT val FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, v, v =ANY (SELECT val FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, v, v >ANY (SELECT val FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, vn, vn <>ALL (SELECT valn FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, vn, vn =ALL (SELECT valn FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, vn, vn >ALL (SELECT valn FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, vn, vn <>ANY (SELECT valn FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, vn, vn =ANY (SELECT valn FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT rf, vn, vn >ANY (SELECT valn FROM itr WHERE otr.rf = itr.rf) AS b
FROM otr;

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo # Subquery with outer reference in WHERE clause

let $query=
SELECT *
FROM otr
WHERE v <>ALL (SELECT val FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE v =ALL (SELECT val FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE v >ALL (SELECT val FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE v <>ANY (SELECT val FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE v =ANY (SELECT val FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE v >ANY (SELECT val FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE vn <>ALL (SELECT valn FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE vn =ALL (SELECT valn FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE vn >ALL (SELECT valn FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE vn <>ANY (SELECT valn FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE vn =ANY (SELECT valn FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT *
FROM otr
WHERE vn >ANY (SELECT valn FROM itr WHERE otr.rf = itr.rf);

--sorted_result
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT i, i >ANY (SELECT i FROM t_null) FROM t_outer;
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT NULL >ANY (SELECT i FROM t_empty) AS result FROM t_null;
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

let $query=
SELECT i <>ANY (SELECT NULL FROM t_null) FROM t_null;
eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

--echo
--echo Bug#37529060: WL#13052: !Item->hidden in Aggregator_distinct::setup
--echo

CREATE TABLE t1 (
  pk int NOT NULL,
  cv varchar(1) DEFAULT NULL,
  cv_key varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk)
);

let $query=
SELECT alias2.pk AS field1,
       alias1.pk AS field2
FROM t1 AS alias1 INNER JOIN t1 AS alias2 ON alias2.cv = alias1.cv_key
WHERE alias2.cv <> ANY
       (SELECT alias3.cv_key AS field3
        FROM t1 AS alias3
        WHERE alias3.cv_key >
                 (SELECT MAX(alias4.cv) AS field4
                  FROM t1 AS alias4
                 )
       );

eval $query;
--replace_regex $elide_costs
eval explain format=tree $query;

DROP TABLE t1;
