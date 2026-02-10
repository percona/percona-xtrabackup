# ==== Purpose ====
#
# Change services comment, the script by default takes
# last created service.
#
# * $mrs_sql_id_variable - variable that allows to overwritte which service
#                          should be changed
# * $mrs_modify_comments - new value for service comment
#
# ==== Usage ====
#
# [--let $mrs_sql_id_variable=@service_id1]
# --let $mrs_modify_comments=new comment
# --source ../include/mrs/service/modify_comments.inc
#

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@service_id
}

if (!$mrs_modify_comments)
{
  --die `mrs_modify_comments` variable is required.
}

--disable_query_log
--disable_result_log

eval UPDATE `mysql_rest_service_metadata`.`service` SET comments = "$mrs_modify_comments" WHERE `id`=$mrs_sql_id_variable;

--enable_query_log
--enable_result_log


--let $mrs_modify_comments=
--let $mrs_sql_id_variable=
