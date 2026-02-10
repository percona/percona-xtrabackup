# The include statement below is a temp one for tests that are yet to
# be ported to run with InnoDB,
# but needs to be kept for tests that would need MyISAM in future.
--source include/not_hypergraph.inc
--source include/force_myisam_default.inc
--source include/have_myisam.inc

# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

--source include/group_skip_scan_test.inc
--source include/elide_costs.inc

--echo #
--echo # Bug#47762: Incorrect result from MIN() when WHERE tests NOT NULL column
--echo #            for NULL
--echo #

--echo ## Test for NULLs allowed
CREATE TABLE t1 ( a INT, KEY (a) );
INSERT INTO t1 VALUES (1), (2), (3);
--source include/min_null_cond.inc
INSERT INTO t1 VALUES (NULL), (NULL);
--source include/min_null_cond.inc
DROP TABLE t1;

--echo ## Test for NOT NULLs
CREATE TABLE t1 ( a INT NOT NULL PRIMARY KEY);
INSERT INTO t1 VALUES (1), (2), (3);
--echo #
--echo # NULL-safe operator test disabled for non-NULL indexed columns.
--echo #
--echo # See bugs
--echo #
--echo # - Bug#52174: Sometimes wrong plan when reading a MAX value from
--echo #   non-NULL index
--echo #
--let $skip_null_safe_test= 1
--let $skip_null_safe_test= 0
--source include/min_null_cond.inc
DROP TABLE t1;

# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
