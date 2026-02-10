if (!$mrs_log_expected_message)
{
  --die `mrs_log_expected_message` variable is required.
}

if (!$mrs_log_expected_level)
{
  --die `mrs_log_expected_level` variable is required.
}

--disable_query_log
--disable_result_log

let $wait_condition=SELECT id FROM mysql_rest_service_metadata.router_general_log 
WHERE router_id=$extra_mrs_router_id and log_type = $mrs_log_expected_level 
AND message LIKE $mrs_log_expected_message;
--source include/wait_condition_or_abort.inc

--enable_query_log
--enable_result_log

--let $mrs_log_expected_message=
--let $mrs_log_expected_level=
