--source include/have_asan.inc

--echo #
--echo # Bug#35621833: mysqldump memory leaks
--echo #

--disable_query_log
CALL mtr.add_suppression("Out of sort memory");
--enable_query_log

SET GLOBAL SORT_BUFFER_SIZE = 32768;
let $dumpfile=$MYSQLTEST_VARDIR/tmp/bug35621833.sql;

--echo # Test: should not leak memory
--error 3
--exec $MYSQL_DUMP --force --order-by-primary mysql help_topic > $dumpfile

SET GLOBAL SORT_BUFFER_SIZE = DEFAULT;
--remove_file $dumpfile

--echo # End of 9.3 tests
