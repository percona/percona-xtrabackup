# Grant a privilege to a role
#
# ALLOWED PARAMETERS:
#  * $mrs_grant_privilege_service_path - host/service name pattern
#  * $mrs_grant_privilege_schema_path - schema name pattern
#  * $mrs_grant_privilege_object_path - db object name pattern
#  * $mrs_grant_privilege_crud_operations - allowed CRUD ops (READ,CREATE,UPDATE,DELETE)
#  * $mrs_grant_privilege_options - JSON options (quoted)

# USAGE:
#
#    --let $mrs_grant_privilege_service_path=/myService
#    --let $mrs_grant_privilege_schema_path=/schema
#    --let $mrs_grant_privilege_object_path=/object
#    --let $mrs_grant_privilege_crud_operations=READ,CREATE,UPDATE,DELETE
#    --source ../include/mrs/role/grant_privilege.inc
#    

if (!$mrs_grant_privilege_service_path)
{
  --die 'mrs_grant_privilege_service_path' variable is required.
}

if (!$mrs_grant_privilege_crud_operations)
{
  --die 'mrs_grant_privilege_crud_operations' variable is required.
}

if (!$mrs_grant_privilege_schema_path)
{
    --die 'mrs_grant_privilege_schema_path' variable is required.
}

if (!$mrs_grant_privilege_object_path)
{
    --die 'mrs_grant_privilege_object_path' variable is required.
}

if (!$mrs_grant_privilege_options)
{
  --let $mrs_grant_privilege_options=NULL
}

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@role_privilege_id
}

if (!$mrs_sql_role_id_variable)
{
  --let $mrs_sql_role_id_variable=@role_id
}

--disable_query_log
--disable_result_log
eval  SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();

eval INSERT INTO `mysql_rest_service_metadata`.`mrs_privilege` (
  `id`,
  `role_id`,
  `crud_operations`,
  `service_path`,
  `schema_path`,
  `object_path`,
  `options`)
  VALUES(
    $mrs_sql_id_variable,
    $mrs_sql_role_id_variable,
    '$mrs_grant_privilege_crud_operations',
    '$mrs_grant_privilege_service_path',
    '$mrs_grant_privilege_schema_path',
    '$mrs_grant_privilege_object_path',
    $mrs_grant_privilege_options);


--echo # Registered MRS Role Grant: $mrs_grant_privilege_service_path $mrs_grant_privilege_schema_path $mrs_grant_privilege_object_path => $mrs_grant_privilege_crud_operations
  
--enable_query_log
--enable_result_log

--let $mrs_sql_id_variable=
--let $mrs_sql_role_id_variable=
--let $mrs_grant_privilege_crud_operations=
--let $mrs_grant_privilege_service_path=
--let $mrs_grant_privilege_schema_path=
--let $mrs_grant_privilege_object_path=
--let $mrs_grant_privilege_options=
