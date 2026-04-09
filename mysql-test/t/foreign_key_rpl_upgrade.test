--source include/have_debug.inc
--source include/have_log_bin.inc
--source include/rpl/set_gtid_mode_off.inc
--echo # FR 12) When replicating from a source running an older MySQL version that does
--echo # not support SQL foreign key handling, the replica must perform foreign key
--echo # CASCADE actions using InnoDB FK handling. Since the USE_SQL_FOREIGN_KEY_F flag
--echo # is not set in row-based binlogs from older versions, the applier must use
--echo # InnoDB FK handling.

--echo # This test simulates replication upgrade by using sql binlog dump taken 
--echo # from old MySQL Version 9.0 and replay them on the latest MySQL version

--echo # USE_SQL_FOREIGN_KEY_F should not be present in fk_both_binlog.sql as it is
--echo # created with older MySQL version 9.0 which does not support SQL FK handling.
--let SEARCH_FILE=$MYSQL_TEST_DIR/std_data/fk_both_binlog.sql
--let SEARCH_PATTERN=USE_SQL_FOREIGN_KEY_F
--source include/search_pattern.inc

exec $MYSQL < $MYSQL_TEST_DIR/std_data/fk_both_binlog.sql;

--echo #log event handler adds "SQL FK Handling = OFF" in error log with debug build
--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let SEARCH_PATTERN=SQL FK Handling = OFF
--source include/search_pattern.inc

SELECT * FROM t1 ORDER BY f1;
SELECT * FROM t2 ORDER BY f1;
SELECT * FROM t3 ORDER BY f1;

--echo # rows_deleted should not account child table record
--echo # InnoDB FK handling does not account this.
SELECT table_name, rows_updated FROM sys.schema_table_statistics
  WHERE table_schema='test' ORDER BY table_name;

DROP TABLE t3, t2, t1;
--source include/rpl/set_gtid_mode_on.inc
