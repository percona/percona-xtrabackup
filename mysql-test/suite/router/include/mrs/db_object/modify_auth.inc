

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@db_object_id
}

if (!$mrs_db_object_requires_auth)
{
  --die `mrs_db_object_requires_auth` variable is required.
}

--disable_query_log
--disable_result_log

eval UPDATE `mysql_rest_service_metadata`.`db_object` SET requires_auth = "$mrs_db_object_requires_auth" WHERE `id`=$mrs_sql_id_variable;

--enable_query_log
--enable_result_log


--let $mrs_db_object_requires_auth=
--let $mrs_sql_id_variable=
