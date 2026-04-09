# Kills all sleeping connections and waits until it happens.

--let $kills = `SELECT GROUP_CONCAT('KILL ', ID SEPARATOR ';') FROM performance_schema.processlist WHERE COMMAND LIKE 'Sleep'`
--replace_regex /.+//
if ($kills)
{
  --eval $kills
}

let $wait_condition= SELECT count(*) = 0 FROM performance_schema.processlist WHERE command LIKE 'Sleep';
--source include/wait_condition.inc
