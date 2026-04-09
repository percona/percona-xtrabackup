#
# Bug#36808636 System accounts are not converted to non legacy auth plugin during upgrade
#

--echo #
--echo # Bug#36808636 System accounts are not converted to non legacy auth plugin during upgrade
--echo #

# prep a'la 5.7 system accounts
--echo
--echo # prep a'la 5.7 system accounts
UPDATE mysql.user SET plugin='mysql_native_password', authentication_string='*THISISNOTAVALIDPASSWORDTHATCANBEUSEDHERE' WHERE user='mysql.sys';
UPDATE mysql.user SET plugin='mysql_native_password', authentication_string='*THISISNOTAVALIDPASSWORDTHATCANBEUSEDHERE' WHERE user='mysql.session';

# check sys-accounts use mysql_native_password
--echo
--echo # check sys-accounts use mysql_native_password
--echo # expected: mysql_native_password   *THISISNOTAVALIDPASSWORDTHATCANBEUSEDHERE  Y
SELECT user,plugin,authentication_string,account_locked FROM mysql.user WHERE USER='mysql.sys';
SELECT user,plugin,authentication_string,account_locked FROM mysql.user WHERE USER='mysql.session';

--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysql_upgrade_test.log

# restart the server and enforce the upgrade
--echo
--echo # restart the server and enforce the upgrade
--replace_result $MYSQLD_LOG MYSQLD_LOG
--let $restart_parameters = restart:--upgrade=FORCE --log-error=$MYSQLD_LOG
--let $wait_counter= 10000
--source include/restart_mysqld.inc

# check for [ERROR] pattern in server log
--echo
--echo # check for ERROR pattern in server log
--echo # expected: pattern not found
--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= \[ERROR\]
--source include/search_pattern.inc

# clean log
--remove_file $MYSQLD_LOG

# check sys-accounts are upgraded to cahcing_sha2_password
--echo
--echo # check sys-accounts are upgraded to caching_sha2_password
--echo # expected: caching_sha2_password   \$A\$005\$THISISACOMBINATIONOFINVALIDSALTANDPASSWORDTHATMUSTNEVERBRBEUSED  Y
SELECT user,plugin,authentication_string,account_locked FROM mysql.user WHERE USER='mysql.sys';
SELECT user,plugin,authentication_string,account_locked FROM mysql.user WHERE USER='mysql.session';

# end of test, no cleanup needed
--echo
--echo # End of tests
# this comment prevents EOL junk pb2 error, do not remove it !
