# Add a role
#
# ALLOWED PARAMETERS:
#  * $mrs_add_role_caption - unique name of the role
#  * $mrs_add_role_description - description for the role
#  * $mrs_add_role_options - JSON object with options
#  * $mrs_add_role_derived_from_role_id
#  * $mrs_add_role_specific_to_service_id

# USAGE:
#
#    --let $mrs_add_role_caption="Editor"
#    --source ../include/mrs/role/add.inc
#    

if (!$mrs_add_role_caption)
{
  --die 'mrs_add_role_caption' variable is required.
}

if (!$mrs_add_role_description)
{
  --let $mrs_add_role_description=NULL
}

if (!$mrs_add_role_options)
{
  --let $mrs_add_role_options='{}'
}

if (!$mrs_add_role_derived_from_role_id)
{
  --let $mrs_add_role_derived_from_role_id=NULL
}

if (!$mrs_add_role_specific_to_service_id)
{
  --let $mrs_add_role_specific_to_service_id=NULL
}

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@role_id
}

--disable_query_log
--disable_result_log
eval  SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();

eval INSERT INTO `mysql_rest_service_metadata`.`mrs_role` (
  `id`,
  `derived_from_role_id`,
  `specific_to_service_id`,
  `caption`,
  `description`,
  `options`)
  VALUES(
    $mrs_sql_id_variable,
    $mrs_add_role_derived_from_role_id,
    $mrs_add_role_specific_to_service_id,
    "$mrs_add_role_caption",
    "$mrs_add_role_description",
    $mrs_add_role_options);


--echo # Registered MRS ROLE: $mrs_add_role_caption
  
--enable_query_log
--enable_result_log

--let $mrs_sql_id_variable=
--let $mrs_add_role_derived_from_role_id=
--let $mrs_add_role_specific_to_service_id=
--let $mrs_add_role_caption=
--let $mrs_add_role_description=
--let $mrs_add_role_options=

