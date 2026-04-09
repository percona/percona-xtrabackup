--echo #
--echo # WL#13153: Don't dump SECONDARY TABLE if requested by the end user
--echo #

CREATE TABLE t1 (a INT) SECONDARY_ENGINE=gizmo;

--echo # test: should have SECONDARY_ENGINE
SHOW CREATE TABLE t1;

SET SESSION show_create_table_skip_secondary_engine=on;
SHOW CREATE TABLE t1;
SET SESSION show_create_table_skip_secondary_engine=default;

--let $executed_gtid_set= `SELECT @@GLOBAL.GTID_EXECUTED`
--replace_result "$executed_gtid_set" GTID_SET

--echo # test: t1 should have SECONDARY_ENGINE on

--let $executed_gtid_set= `SELECT @@GLOBAL.GTID_EXECUTED`
--replace_result "$executed_gtid_set" GTID_SET

--exec $MYSQL_DUMP --compact test

--echo # test: t1 should not have SECONDARY_ENGINE

--let $executed_gtid_set= `SELECT @@GLOBAL.GTID_EXECUTED`
--replace_result "$executed_gtid_set" GTID_SET

--exec $MYSQL_DUMP --show-create-table-skip-secondary-engine --compact test

DROP TABLE t1;


--echo # End of 8.0 tests
