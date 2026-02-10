--echo #
--echo # WL#15658: Make mysqldump dump logical ACL statements
--echo #

--source include/no_valgrind_without_big.inc
# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo # Init: create a user and a role
CREATE USER wl15658_user@localhost;
CREATE ROLE wl15658_role@localhost;
CREATE USER wl15658_anoter@localhost;

GRANT SELECT ON *.* TO wl15658_user@localhost;
GRANT UPDATE ON mysql.* TO wl15658_user@localhost;
GRANT FLUSH_STATUS ON *.* TO wl15658_user@localhost;
GRANT wl15658_role@localhost TO wl15658_user@localhost;
CREATE TABLE wl15658_table(a INT);

--echo # dump all data
--exec $MYSQL_DUMP --all-databases --users --add-drop-user --include-user=wl15658_user@localhost --include-user=wl15658_anoter@localhost --include-user=wl15658_role@localhost > $MYSQLTEST_VARDIR/tmp/wl15658_all_dbs.sql
--echo # restore all data
--exec $MYSQL < $MYSQLTEST_VARDIR/tmp/wl15658_all_dbs.sql
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_all_dbs.sql

--echo # Cleanup
DROP USER wl15658_anoter@localhost;
DROP USER wl15658_user@localhost;
DROP ROLE wl15658_role@localhost;
DROP TABLE wl15658_table;

--echo # End if 9.3 tests

# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
