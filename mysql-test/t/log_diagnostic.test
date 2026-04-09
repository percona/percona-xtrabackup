--source include/have_debug.inc
--source include/have_log_diagnostic.inc

--let $MYSQLD_ERROR_LOG= $MYSQLTEST_VARDIR/log/diagnostic_error.log
--let $MYSQLD_DIAGNOSTIC_LOG= $MYSQL_TMP_DIR/diagnostic.log

--echo # Disable diagnostics.
--let $restart_parameters= "restart: --log-error-verbosity=3 --log-error=$MYSQLD_ERROR_LOG --log-diagnostic-enable=0 --log-diagnostic=$MYSQLD_DIAGNOSTIC_LOG --debug='+d,emit_diagnostic_message_upon_start'"
--replace_result $MYSQLD_ERROR_LOG MYSQLD_ERROR_LOG $MYSQLD_DIAGNOSTIC_LOG MYSQLD_DIAGNOSTIC_LOG

--source include/restart_mysqld.inc
--source include/wait_until_connected_again.inc

--let $assert_file= $MYSQLD_ERROR_LOG
--let $assert_select= Message to diagnostic log
--let $assert_count= 0
--let $assert_text= No diagnostic output to the error log
--source include/assert_grep.inc

--let $assert_file= $MYSQLD_ERROR_LOG
--let $assert_select= Message to stdout
--let $assert_count= 1
--let $assert_text= Output to stdout goes to the error log
--source include/assert_grep.inc

--echo # The diagnostic log file should not exist.
--error 1
--remove_file $MYSQLD_DIAGNOSTIC_LOG

--echo # Enable diagnostics.
--let $restart_parameters = "restart: --log-error-verbosity=3 --log-error=$MYSQLD_ERROR_LOG --log-diagnostic-enable=1 --log-diagnostic=$MYSQLD_DIAGNOSTIC_LOG --debug='+d,emit_diagnostic_message_upon_start'"
--replace_result $MYSQLD_ERROR_LOG MYSQLD_ERROR_LOG $MYSQLD_DIAGNOSTIC_LOG MYSQLD_DIAGNOSTIC_LOG

--source include/restart_mysqld.inc
--source include/wait_until_connected_again.inc

--let $assert_file= $MYSQLD_DIAGNOSTIC_LOG
--let $assert_select= Message to diagnostic log
--let $assert_count= 1
--let $assert_text= Diagnostic output goes to the diagnostic log.
--source include/assert_grep.inc

--let $assert_file= $MYSQLD_DIAGNOSTIC_LOG
--let $assert_select= Message to stdout
--let $assert_count= 1
--let $assert_text= Output to stdout goes to the diagnostic log.
--source include/assert_grep.inc

--remove_file $MYSQLD_DIAGNOSTIC_LOG

