
if (!$mrs_add_content_file_path) {
  --die 'mrs_add_content_file_path' variable is required.
}

if (!$mrs_content_set_id_sql_variable) {
  --die '$mrs_content_set_id_sql_variable' variable is required.
}

--disable_query_log
--disable_result_log
eval DELETE FROM `mysql_rest_service_metadata`.`content_file` WHERE `request_path`='$mrs_add_content_file_path' AND `content_set_id`=$mrs_content_set_id_sql_variable;
--enable_query_log
--enable_result_log

--let $mrs_add_content_file_path=
--let $mrs_content_set_id_sql_variable=

