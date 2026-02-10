--source include/no_valgrind_without_big.inc
--source include/not_asan.inc

--echo #
--echo # Bug #37353662: Contribution: fix mysqlslap --ssl-mode=disable not working issue
--echo #

delimiter $;
CREATE PROCEDURE signal_if_ssl(is_present int)
BEGIN
  if (is_present =
      (SELECT count(*) FROM performance_schema.session_status
         WHERE VARIABLE_NAME='Ssl_version' AND VARIABLE_VALUE = '')
     ) THEN
    signal SQLSTATE '42000' SET MESSAGE_TEXT = 'SSL not as expected', MYSQL_ERRNO=1000;
  END IF;
END$
delimiter ;$

SET GLOBAL log_output="TABLE";
--echo # Try with SSL required: should pass
--exec $MYSQL_SLAP --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=require --create="CALL test.signal_if_ssl(1);" --query="SELECT 1" --silent
--echo # Try with SSL disabled: should pass
--exec $MYSQL_SLAP --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=disable --create="CALL test.signal_if_ssl(0);" --query="SELECT 2" --silent
SET GLOBAL log_output=default;
SELECT argument FROM mysql.general_log WHERE command_type = 'Connect' ORDER BY event_time;

DROP PROCEDURE signal_if_ssl;
TRUNCATE mysql.general_log;

--echo # End of 9.3 tests
