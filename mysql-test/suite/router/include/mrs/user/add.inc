# Add user to authentication-application that was defined in last call to `add_auth_app.inc`.
# User must provide user-name, external user-id, auth-string
#
# ALLOWED PARAMETERS:
#  * $mrs_add_user_email - users email (optional)
#  * $mrs_add_user_login_permitted - enabled/disable the user (optional, default true)
#  * $mrs_add_user_ext_uid - id of the user in external system (required)
#  * $mrs_add_user_name - name of the user/account
#  * $mrs_add_user_auth_string= authentication application specific string

# USAGE:
#
#    [--let $mrs_add_user_email=[some@email.com]]
#    [--let $mrs_add_user_login_permitted=[1|0]]
#    --let $mrs_add_user_ext_uid=EXT_USER_ID
#    --let $mrs_add_user_name="Me"
#    --let $mrs_add_user_auth_string=APP_SPECIFIC_AUTH_STRING
#    --source ../include/mrs/user/add.inc
#

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@user_id
}

if (!$mrs_sql_app_id_variable)
{
  --let $mrs_sql_app_id_variable=@auth_app_id
}

if (!$mrs_add_user_name)
{
  --die 'mrs_add_user_name' variable is required.
}

if (!$mrs_add_user_ext_uid)
{
  --die 'mrs_add_user_ext_uid' variable is required.
}

if (`SELECT LENGTH($mrs_add_user_auth_string)=0`)
{
  --echo mrs_add_user_auth_string:$mrs_add_user_auth_string
  --die 'mrs_add_user_auth_string' variable is required.
}


if (!$mrs_add_user_email)
{
  --let $mrs_add_user_email=NULL
}

if (!$mrs_add_user_login_permitted)
{
  --let $mrs_add_user_login_permitted=1
}


--disable_query_log
--disable_result_log
eval  SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();

eval INSERT INTO `mysql_rest_service_metadata`.`mrs_user` (
  `id`,
  `auth_app_id`,
  `name`,
  `email`,
  `vendor_user_id`,
  `login_permitted`,
  `mapped_user_id`,
  `app_options`,
  `auth_string`)
  VALUES(
    $mrs_sql_id_variable,
    $mrs_sql_app_id_variable,
    $mrs_add_user_name,
    $mrs_add_user_email,
    $mrs_add_user_ext_uid,
    $mrs_add_user_login_permitted,
    NULL,
    NULL,
    FROM_BASE64($mrs_add_user_auth_string));

--let $mrs_add_user_role_id=`SELECT IFNULL(default_role_id,"") FROM mysql_rest_service_metadata.auth_app WHERE id=$mrs_sql_app_id_variable`

if (`SELECT LENGTH("$mrs_add_user_role_id") and LENGTH("$mrs_add_user_role")=0`)
{
  eval INSERT INTO mysql_rest_service_metadata.mrs_user_has_role(user_id,role_id, comments)
           VALUES($mrs_sql_id_variable, "$mrs_add_user_role_id","Default role");
}

if (`SELECT LENGTH("$mrs_add_user_role")`)
{
  eval INSERT INTO mysql_rest_service_metadata.mrs_user_has_role(user_id,role_id, comments)
           VALUES($mrs_sql_id_variable, (select id from mysql_rest_service_metadata.mrs_role where caption rlike "$mrs_add_user_role"),"Assigned role");
}

--echo # Registred MRS USER: $mrs_add_user_name
  
--enable_query_log
--enable_result_log

--let $mrs_sql_id_variable=
--let $mrs_sql_app_id_variable=
--let $mrs_add_user_name=
--let $mrs_add_user_email=
--let $mrs_add_user_ext_uid=
--let $mrs_add_user_login_permitted=
--let $mrs_add_user_auth_string=
