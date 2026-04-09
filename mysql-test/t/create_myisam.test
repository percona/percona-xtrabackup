--source include/force_myisam_default.inc
--source include/have_myisam.inc

# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

#
# Bug #14155: Maximum value of MAX_ROWS handled incorrectly on 64-bit
# platforms
#
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

create table t1 (i int) engine=myisam max_rows=100000000000;
show create table t1;
alter table t1 max_rows=100;
show create table t1;
alter table t1 max_rows=100000000000;
show create table t1;
drop table t1;

--echo
--echo # --
--echo # -- Bug#18834: ALTER TABLE ADD INDEX on table with two timestamp fields
--echo # --
--echo

CREATE TABLE t3(c1 DATETIME NOT NULL) ENGINE=MYISAM;
INSERT INTO t3 VALUES (0);

--echo
SET sql_mode = TRADITIONAL;

--echo
--error ER_TRUNCATED_WRONG_VALUE
ALTER TABLE t3 ADD INDEX(c1);

--echo
--echo # -- Cleanup.

SET sql_mode = '';
DROP TABLE t3;
# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
