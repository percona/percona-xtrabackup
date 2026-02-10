
#
# Bug #30126: semicolon before closing */ in /*!... CREATE DATABASE ;*/
#

# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

--let $file = $MYSQLTEST_VARDIR/tmp/bug30126.sql

CREATE DATABASE mysqldump_30126;
USE mysqldump_30126;
CREATE TABLE t1 (c1 int);
--exec $MYSQL_DUMP --add-drop-database mysqldump_30126 > $file
--exec $MYSQL mysqldump_30126 < $file
DROP DATABASE mysqldump_30126;

--remove_file $file
# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
