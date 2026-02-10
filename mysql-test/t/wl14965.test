
--echo # save mysql tables
--exec $MYSQL_DUMP --add-drop-table --databases mysql > $MYSQL_TMP_DIR/mysql.sql
--echo # restore mysql tables from 8.0.27
--echo # But, add suppression because table mysql.procs_priv prior to version 9.2 had 'Routine_type' at position 4 of type enum('FUNCTION','PROCEDURE')
--echo # but since 9.2 it has type enum('FUNCTION','PROCEDURE','LIBRARY'). This is not an issue, since the new definition
--echo # is a superset of the prior and the old values (and the order) wont be affected.
--echo # Note: the warning is reported by Table_check_intact::check() & System_table_intact::report_error()
CALL mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* Cannot load from mysql.procs_priv");
--exec $MYSQL test < $MYSQLTEST_VARDIR/std_data/wl14965.dump

SHOW KEYS FROM mysql.db;
SHOW KEYS FROM mysql.tables_priv;
SHOW KEYS FROM mysql.columns_priv;
SHOW KEYS FROM mysql.procs_priv;

--echo # Before restart
SELECT * FROM mysql.procs_priv;

--echo # Restart server without upgrade option
--source include/restart_mysqld.inc

--echo # Before upgrade force
SELECT * FROM mysql.procs_priv;

--echo # Restart server with upgrade option
--let $restart_parameters=restart:--upgrade=force
--let $wait_counter= 10000
--source include/restart_mysqld.inc

--echo # After upgrade force
SELECT * FROM mysql.procs_priv;

SHOW KEYS FROM mysql.db;
SHOW KEYS FROM mysql.tables_priv;
SHOW KEYS FROM mysql.columns_priv;
SHOW KEYS FROM mysql.procs_priv;

CREATE USER wl14965_u2;
GRANT ALL ON test.* to wl14965_u2;
SHOW GRANTS FOR wl14965_u2;
DROP USER wl14965_u2;

--echo # ensure that key_len is 351 which includes length of host(255) + user(96)
--replace_column 10 # 11 #
EXPLAIN SELECT * FROM mysql.db WHERE host='' AND user='';
--replace_column 10 # 11 #
EXPLAIN SELECT * FROM mysql.tables_priv WHERE host='' AND user='';
--replace_column 10 # 11 #
EXPLAIN SELECT * FROM mysql.columns_priv WHERE host='' AND user='';
--replace_column 10 # 11 #
EXPLAIN SELECT * FROM mysql.procs_priv WHERE host='' AND user='';

#cleanup
DROP DATABASE wl14965;
--echo # restore original mysql tables
--exec $MYSQL --force < $MYSQL_TMP_DIR/mysql.sql
--remove_file $MYSQL_TMP_DIR/mysql.sql
