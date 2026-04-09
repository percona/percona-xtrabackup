
if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@auth_app_id
}

if (!$mrs_delete_auth_app)
{
  --die 'mrs_delete_auth_app' variable is required.
}

--disable_query_log
--disable_result_log
START TRANSACTION;
eval  SET $mrs_sql_id_variable=
     (SELECT id FROM mysql_rest_service_metadata.auth_app WHERE name=$mrs_delete_auth_app);

if ($mrs_schema_version > 2)
{
  eval DELETE FROM `mysql_rest_service_metadata`.`service_has_auth_app`
     WHERE `auth_app_id`=$mrs_sql_id_variable;
}

eval DELETE FROM `mysql_rest_service_metadata`.`auth_app` WHERE id=$mrs_sql_id_variable;

COMMIT;
--echo # Removed AUTH APP at path: $mrs_add_auth_service
  
--enable_query_log
--enable_result_log


--let $mrs_sql_id_variable=
--let $mrs_add_auth_app=
