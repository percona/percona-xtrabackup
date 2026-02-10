--echo #
--echo # WL#15658: Make mysqldump dump logical ACL statements
--echo #

-- source include/have_log_bin.inc

--source include/no_valgrind_without_big.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo # Init: create a user and a role
CREATE USER wl15658_user@localhost;
CREATE ROLE wl15658_role@localhost;
CREATE USER wl15658_another@localhost;

GRANT SELECT ON *.* TO wl15658_user@localhost;
GRANT UPDATE ON mysql.* TO wl15658_user@localhost;
GRANT FLUSH_STATUS ON *.* TO wl15658_user@localhost;
GRANT wl15658_role@localhost TO wl15658_user@localhost;
CREATE TABLE wl15658_table(a INT);

--echo # Test FR1: the --users option should produce a "CREATE USER wl15658_user" line

--exec $MYSQL_DUMP --compact test wl15658_table --users > $MYSQLTEST_VARDIR/tmp/wl15658_fr1.sql

--let $assert_text=CREATE USER wl15658_user@localhost
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr1.sql
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=1
--source include/assert_grep.inc

--let $assert_text=FR1.2: CREATE USER wl15658_user@localhost before table data
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr1.sql
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_only_after=^CREATE TABLE
--let $assert_count=0
--source include/assert_grep.inc

--let $assert_text=FR1.3: GRANT .. TO wl15658_user@localhost after table data
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr1.sql
--let $assert_select=^GRANT.*FLUSH_STATUS.*wl15658_user
--let $assert_only_after=^CREATE TABLE
--let $assert_count=1
--source include/assert_grep.inc
--let $assert_only_after=

--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr1.sql

--echo # Test FR1.4: should not fail
--exec $MYSQL_DUMP --compact --users > $MYSQLTEST_VARDIR/tmp/wl15658_fr14.sql
--let $assert_text=FR1.4: have CREATE USER wl15658_user@localhost
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr14.sql
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=1
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr14.sql

--echo # Test FR1.5: excluded tables
--exec $MYSQL_DUMP --compact --users mysql > $MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.user
--let $assert_select=^CREATE TABLE.*user
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.global_grants
--let $assert_select=^CREATE TABLE.*global_grants
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.db
--let $assert_select=^CREATE TABLE.*db['` ]
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.tables_priv
--let $assert_select=^CREATE TABLE.*tables_priv['` ]
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.columns_priv
--let $assert_select=^CREATE TABLE.*columns_priv['` ]
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.procs_priv
--let $assert_select=^CREATE TABLE.*procs_priv['` ]
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.proxies_priv
--let $assert_select=^CREATE TABLE.*proxies_priv['` ]
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.default_roles
--let $assert_select=^CREATE TABLE.*default_roles['` ]
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.role_edges
--let $assert_select=^CREATE TABLE.*role_edges['` ]
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR1.5: no mysql.password_history
--let $assert_select=^CREATE TABLE.*password_history['` ]
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql
--let $assert_count=0
--let $assert_text=FR2: no DROP USER without --add-drop-user
--let $assert_select=^DROP USER
--source include/assert_grep.inc

--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr15.sql

--echo # FR1.6: --users and --flush-privileges causes error
--error 1
--exec $MYSQL_DUMP --compact --users --flush-privileges 2>&1


--exec $MYSQL_DUMP --compact --users --add-drop-user > $MYSQLTEST_VARDIR/tmp/wl15658_fr2.sql

--let $assert_text=FR2: have DROP USER
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr2.sql
--let $assert_select=^DROP USER.*wl15658_user
--let $assert_count=1
--source include/assert_grep.inc

--let $assert_text=FR2: DROP USER before CREATE USER
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr2.sql
--let $assert_only_after=^CREATE USER.*wl15658_user
--let $assert_select=^DROP USER.*wl15658_user
--let $assert_count=0
--source include/assert_grep.inc
--let $assert_only_after=

--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr2.sql

--exec $MYSQL_DUMP --compact test --add-drop-user > $MYSQLTEST_VARDIR/tmp/wl15658_fr21.sql 2>&1
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr21.sql
--let $assert_text=FR2.1: --add-drop-user a no-op if --users is missing
--let $assert_select=^DROP USER.*wl15658_user
--let $assert_count=0
--source include/assert_grep.inc

--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr21.sql
--let $assert_text=FR3.1: --add-drop-user sans --users warning present
--let $assert_select=The --add-drop-user option is a no-op without --users
--let $assert_count=1
--source include/assert_grep.inc

--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr21.sql

--exec $MYSQL_DUMP --compact --users > $MYSQLTEST_VARDIR/tmp/wl15658_fr31.sql
--let $assert_text=FR3.1: All accounts included if no --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr31.sql
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=1
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr31.sql

--echo # FR3.2: invalid data for --include-user: must fail
--error 1
--exec $MYSQL_DUMP --compact --users --include-user=wl15658_user 2>&1

--exec $MYSQL_DUMP --compact --users --include-user=wl15658_user@localhost --include-user=wl15658_role@localhost > $MYSQLTEST_VARDIR/tmp/wl15658_fr33.sql
--let $assert_text=FR3.3: Multiple --include-user specified, wl15658_user present
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr33.sql
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=1
--source include/assert_grep.inc

--let $assert_text=FR3.3: Multiple --include-user specified, wl15658_role present
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr33.sql
--let $assert_select=^CREATE USER.*wl15658_role
--let $assert_count=1
--source include/assert_grep.inc

--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr33.sql

--exec $MYSQL_DUMP --compact --include-user=wl15658_user@localhost test wl15658_table > $MYSQLTEST_VARDIR/tmp/wl15658_fr34.sql 2>&1
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr34.sql
--let $assert_text=FR3.4: --include-user is a no-op if no --users present
--let $assert_select=^CREATE USER
--let $assert_count=0
--source include/assert_grep.inc

--let $assert_text=FR3.4: --include-user sans --users warning present
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr34.sql
--let $assert_select=The --include-user option is a no-op without --users
--let $assert_count=1
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr34.sql

--exec $MYSQL_DUMP --compact --users > $MYSQLTEST_VARDIR/tmp/wl15658_fr41.sql
--let $assert_text=FR4.1: All accounts included if no --exclude-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr41.sql
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=1
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr41.sql

--echo # FR4.2: invalid data for --exclude-user: must fail
--error 1
--exec $MYSQL_DUMP --compact --users --exclude-user=wl15658_user 2>&1

--exec $MYSQL_DUMP --compact --users --exclude-user=wl15658_user@localhost --exclude-user=wl15658_role@localhost > $MYSQLTEST_VARDIR/tmp/wl15658_fr43.sql
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr43.sql
--let $assert_text=FR4.3: Multiple --exclude-user specified, wl15658_user not present
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=0
--source include/assert_grep.inc

--let $assert_text=FR4.3: Multiple --exclude-user specified, wl15658_role not present
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr43.sql
--let $assert_select=^CREATE USER.*wl15658_role
--let $assert_count=0
--source include/assert_grep.inc

--let $assert_text=FR4.3: Multiple --exclude-user specified, CREATE USER wl15658_another present
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr43.sql
--let $assert_select=^CREATE USER.*wl15658_another
--let $assert_count=1
--source include/assert_grep.inc

--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr43.sql

--exec $MYSQL_DUMP --compact --users --include-user=wl15658_user@localhost --exclude-user=wl15658_user@localhost > $MYSQLTEST_VARDIR/tmp/wl15658_fr44.sql
--let $assert_text=FR4.4: --include-user --exclude-user = no user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr44.sql
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=0
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr44.sql

--exec $MYSQL_DUMP --compact --users --exclude-user=wl15658_user@localhost --include-user=wl15658_user@localhost > $MYSQLTEST_VARDIR/tmp/wl15658_fr44.sql
--let $assert_text=FR4.4: --exclude-user --include-user = no user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr44.sql
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=0
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr44.sql

--exec $MYSQL_DUMP --compact --exclude-user=wl15658_user@localhost test wl15658_table > $MYSQLTEST_VARDIR/tmp/wl15658_fr45.sql 2>&1
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr45.sql
--let $assert_text=FR4.5: --exclude-user is a no-op if no --users present
--let $assert_select=^CREATE USER.*wl15658_user
--let $assert_count=0
--source include/assert_grep.inc

--let $assert_text=FR4.5: --exclude-user sans --users warning present
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr45.sql
--let $assert_select=The --exclude-user option is a no-op without --users
--let $assert_count=1
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr45.sql

--echo FR5: --users doesn't work if --xml is on
--error 1
--exec $MYSQL_DUMP --compact --users --xml test wl15658_table 2>&1

--echo # Cleanup
DROP USER wl15658_another@localhost;
DROP USER wl15658_user@localhost;
DROP ROLE wl15658_role@localhost;
DROP TABLE wl15658_table;

--echo FR 3.5 and 4.6: account name formatting for user accounts

CREATE USER 'wl15658 space'@localhost;
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658 space'@'localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr351.sql 2>&1
--let $assert_text=FR3.5: space in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr351.sql
--let $assert_select=wl15658.*space
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr351.sql

CREATE USER 'wl15658-minus'@localhost;
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658-minus'@'localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr352.sql 2>&1
--let $assert_text=FR3.5: minus in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr352.sql
--let $assert_select=wl15658.*minus
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr352.sql

CREATE USER 'wl15658%percent'@localhost;
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658%percent'@'localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr353.sql 2>&1
--let $assert_text=FR3.5: percent in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr353.sql
--let $assert_select=wl15658.*percent
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr353.sql

CREATE USER 'wl15658\\backslash'@localhost;
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658\\\\backslash'@'localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr354.sql 2>&1
--let $assert_text=FR3.5: backslash in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr354.sql
--let $assert_select=wl15658.*backslash
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr354.sql

CREATE USER 'wl15658\'quote'@localhost;
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658\'quote'@'localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr355.sql 2>&1
--let $assert_text=FR3.5: quote in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr355.sql
--let $assert_select=wl15658.*quote
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr355.sql

CREATE USER 'wl15658_hst'@'space localhost';
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658_hst'@'space localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr356.sql 2>&1
--let $assert_text=FR3.5: space in host in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr356.sql
--let $assert_select=wl15658_hst.*space
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr356.sql

CREATE USER 'wl15658_hst'@'minus-localhost';
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658_hst'@'minus-localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr357.sql 2>&1
--let $assert_text=FR3.5: minus in host in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr357.sql
--let $assert_select=wl15658_hst.*minus
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr357.sql

CREATE USER 'wl15658_hst'@'percent%localhost';
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658_hst'@'percent%localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr358.sql 2>&1
--let $assert_text=FR3.5: percent in host in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr358.sql
--let $assert_select=wl15658_hst.*percent
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr358.sql

CREATE USER 'wl15658_hst'@'backslash\\localhost';
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658_hst'@'backslash\\\\localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr359.sql 2>&1
--let $assert_text=FR3.5: backslash in host in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr359.sql
--let $assert_select=wl15658_hst.*backslash
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr359.sql

CREATE USER 'wl15658_hst'@'quote\'localhost';
--exec $MYSQL_DUMP --compact --users --include-user="'wl15658_hst'@'quote\'localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr35a.sql 2>&1
--let $assert_text=FR3.5: quote in host in --include-user
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr35a.sql
--let $assert_select=wl15658_hst.*quote
--let $assert_count=2
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr35a.sql

--exec $MYSQL_DUMP --compact --users --exclude-user="'wl15658_hst'@'percent%localhost'" > $MYSQLTEST_VARDIR/tmp/wl15658_fr35b.sql 2>&1
--let $assert_text=FR3.5: --exclude-user percent
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr35b.sql
--let $assert_select=wl15658_hst.*percent
--let $assert_count=0
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr35b.sql

DROP USER 'wl15658 space'@localhost;
DROP USER 'wl15658-minus'@localhost;
DROP USER 'wl15658%percent'@localhost;
DROP USER 'wl15658\\backslash'@localhost;
DROP USER 'wl15658\'quote'@localhost;
DROP USER 'wl15658_hst'@'space localhost';
DROP USER 'wl15658_hst'@'minus-localhost';
DROP USER 'wl15658_hst'@'percent%localhost';
DROP USER 'wl15658_hst'@'backslash\\localhost';
DROP USER 'wl15658_hst'@'quote\'localhost';

--exec $MYSQL_DUMP --compact --users --exclude-user=nonexistent@nosuchhost > $MYSQLTEST_VARDIR/tmp/wl15658_fr47.sql 2>&1
--let $assert_text=FR4.7: --exclude-user with a non-matched account results in a warning
--let $assert_file=$MYSQLTEST_VARDIR/tmp/wl15658_fr47.sql
--let $assert_select=^Warning: --exclude-user=.*didn't match any included account
--let $assert_count=1
--source include/assert_grep.inc
--remove_file $MYSQLTEST_VARDIR/tmp/wl15658_fr47.sql

--echo # Test FR3.6: --include-user=nonexistent@nosuchhost must fail

--let $executed_gtid_set= `SELECT @@GLOBAL.GTID_EXECUTED`
--replace_result "$executed_gtid_set" GTID_SET

--error 2
--exec $MYSQL_DUMP --compact --users --include-user=nonexistent@nosuchhost 2>&1


--echo # End if 9.3 tests
