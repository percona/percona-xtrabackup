#
# Test checking whether server cleanly exits after being provided an invalid
# combination of command line arguments in windows when --skip-grant-tables
# is used without enabling networking.
#

--source include/have_debug.inc
--source include/windows.inc

--let $MYSQLD_LOG= $MYSQL_TMP_DIR/server.log
--let $MYSQLD_DATADIR= `SELECT @@datadir`

--echo # Stop DB server which was created by MTR default
--source include/shutdown_mysqld.inc

# Make sure server exits with the right status and message after being provided
# incorrect startup options. In windows, when starting without checking user
# privileges TCP/IP, --shared-memory, or --named-pipe should be enabled to
# allow remote connections.
--error 1
--exec $MYSQLD_CMD --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --skip-grant-tables

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=TCP/IP, --shared-memory, or --named-pipe should be configured on NT OS
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--source include/start_mysqld.inc
