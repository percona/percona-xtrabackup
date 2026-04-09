--let $_mrs_max_log_id=`select IFNULL(max(id), 0) from mysql_rest_service_metadata.router_general_log`
--let $wait_condition=SELECT id FROM mysql_rest_service_metadata.router_general_log WHERE router_id=$extra_mrs_router_id and id > $_mrs_max_log_id;
--source include/wait_condition_or_abort.inc

--let $_mrs_max_log_id=
