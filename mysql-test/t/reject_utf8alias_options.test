################################################################
# WL#16779 User controlled aliasing for 'utf8'
# Verify that "utf8" and "utf8_xxx" are rejected as command line option
# arguments for --character-set-server and --collation-server.
# This happens during bootstrap, and we we have no way of knowing whether
# "utf8" means utf8mb3 or utf8mb4.
# Actually: --collation-server=utf8_xxx is un-ambiguous, so we *could*
# accept it, with a warning.
################################################################

--let $MYSQLD_LOG=$MYSQL_TMP_DIR/server.log
--let $MYSQLD_DATADIR=`SELECT @@datadir`

--echo # stop server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down

--echo # reject utf8
--error 1
--exec $MYSQLD --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR -Cutf8
--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Character set 'utf8' rejected as command line option
--source include/search_pattern.inc
--remove_file $MYSQLD_LOG

--echo # reject UTF8
--error 1
--exec $MYSQLD --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR -CUTF8
--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Character set 'utf8' rejected as command line option
--source include/search_pattern.inc
--remove_file $MYSQLD_LOG

--echo # reject utf8_bin
--error 1
--exec $MYSQLD --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --character-set-server=utf8mb3 --collation-server=utf8_bin
--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Collation 'utf8_bin' rejected as command line option
--source include/search_pattern.inc
--remove_file $MYSQLD_LOG

--echo # reject UTF8_BIN
--error 1
--exec $MYSQLD --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --character-set-server=utf8mb3 --collation-server=UTF8_BIN
--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Collation 'UTF8_BIN' rejected as command line option
--source include/search_pattern.inc
--remove_file $MYSQLD_LOG

--echo # reject utf8 for --character-set-filesystem
--error 1
--exec $MYSQLD --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --character-set-filesystem=utf8
--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Character set 'utf8' rejected as command line option
--source include/search_pattern.inc
--remove_file $MYSQLD_LOG

--echo # Log an error for unknown character set
--error 1
--exec $MYSQLD --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR -Cutf8nosuchname
--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Unknown character set: 'utf8nosuchname'
--source include/search_pattern.inc
--remove_file $MYSQLD_LOG

--echo # Log an error for unknown character set
--error 1
--exec $MYSQLD --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --character-set-filesystem=utf8nosuchname
--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Unknown character set: 'utf8nosuchname'
--source include/search_pattern.inc
--remove_file $MYSQLD_LOG

--echo # Restarting the server
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc
