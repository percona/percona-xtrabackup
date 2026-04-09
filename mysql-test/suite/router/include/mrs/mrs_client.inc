if ($mrs_client_exe_args)
{
  --let $mrs_client_cmd=$mrs_client_exe_args  
}

if (!$mrs_client_exe_args)
{
  if (!$MRS_CLIENT_ARGS)
  {
    --die Either `MRS_CLIENT_ARGS` or `mrs_client_exe_args` variable is required.
  }
  --let $mrs_client_cmd=$MRS_CLIENT_ARGS  
}

if ($mrs_client_arg_path)
{
 --let $mrs_escape_query=$mrs_client_arg_path
 --let $mrs_include_character_and=1
 --source escape_url_query.inc

  --let $mrs_client_cmd=$mrs_client_cmd --path $mrs_escape_query
}

if ($mrs_client_arg_authentication)
{
	--let $mrs_client_cmd=$mrs_client_cmd --authentication $mrs_client_arg_authentication
}

if ($mrs_client_arg_user)
{
    --let $mrs_client_cmd=$mrs_client_cmd --user $mrs_client_arg_user
}

if ($mrs_client_arg_password)
{
    --let $mrs_client_cmd=$mrs_client_cmd --password $mrs_client_arg_password
}

if ($mrs_client_arg_payload)
{
  --let $mrs_escape_query=$mrs_client_arg_payload
  --source escape_url_query.inc

  --let $mrs_client_cmd=$mrs_client_cmd --encoded-payload $mrs_escape_query
}

if ($mrs_client_arg_expected_status)
{
  --let $mrs_client_cmd=$mrs_client_cmd --expected-status $mrs_client_arg_expected_status
}

if ($mrs_client_arg_json_pointer)
{
  --let $mrs_client_cmd=$mrs_client_cmd --json-pointer $mrs_client_arg_json_pointer
}

if ($mrs_client_arg_exclude_json_pointer)
{
  --let $mrs_client_cmd=$mrs_client_cmd --exclude-json-pointer $mrs_client_arg_exclude_json_pointer
}

if ($mrs_client_arg_request_type)
{
  --let $mrs_client_cmd=$mrs_client_cmd -t $mrs_client_arg_request_type
}

if ($mrs_client_arg_display)
{
  --let $mrs_client_cmd=$mrs_client_cmd --display $mrs_client_arg_display
}

if ($mrs_client_arg_write_format)
{
  --let $mrs_client_cmd=$mrs_client_cmd --write-format $mrs_client_arg_write_format
}

if ($mrs_client_arg_write_to_file)
{
  --let $mrs_client_cmd=$mrs_client_cmd --write-to-file $mrs_client_arg_write_to_file
}

if ($mrs_client_arg_session_file)
{
  --let $mrs_client_cmd=$mrs_client_cmd --session-file $mrs_client_arg_session_file
}

if ($mrs_client_arg_wait_until_found)
{
  --let $mrs_client_cmd=$mrs_client_cmd --wait-until-status=$mrs_client_arg_wait_until_found
}

if ($mrs_client_arg_wait_until_status)
{
  --let $mrs_client_cmd=$mrs_client_cmd --wait-until-status=$mrs_client_arg_wait_until_status
}

if ($mrs_client_arg_response_type)
{
  --let $mrs_client_cmd=$mrs_client_cmd --response-type=$mrs_client_arg_response_type
}

if ($mrs_client_arg_json_schema_file)
{
  --let $mrs_client_cmd=$mrs_client_cmd --json-schema-file=$mrs_client_arg_json_schema_file
}

if ($mrs_client_arg_json_schema)
{
  --let $mrs_escape_query=$mrs_client_arg_json_schema
  --source escape_url_query.inc
  --let $mrs_client_cmd=$mrs_client_cmd --encoded-json-schema=$mrs_escape_query
}

if ($mrs_client_arg_expect_header)
{
  if (!$mrs_client_arg_expect_header_value)
  {
    --die "mrs_client_arg_expect_header" requires mrs_client_arg_expect_header_value parameter.
  }
  --let $mrs_escape_query=$mrs_client_arg_expect_header_value
  --let $mrs_include_character_and=1
  --source escape_url_query.inc
  --let $mrs_client_cmd=$mrs_client_cmd --encoded-expected-header=$mrs_client_arg_expect_header=$mrs_escape_query
}

if ($mrs_client_arg_request_header_host)
{
  --let $mrs_escape_query=$mrs_client_arg_request_header_host
  --source escape_url_query.inc
  --let $mrs_client_cmd=$mrs_client_cmd --encoded-request-header-host=$mrs_escape_query
}

if ($mrs_client_arg_request_header_authorization)
{
  --let $mrs_escape_query=$mrs_client_arg_request_header_authorization
  --source escape_url_query.inc
  --let $mrs_client_cmd=$mrs_client_cmd --encoded-request-header-authorization=$mrs_escape_query
}

if ($mrs_client_arg_protocol)
{
  --let $mrs_client_cmd=$mrs_client_cmd --protocol=$mrs_client_arg_protocol
}

if ($mrs_client_arg_expected_fatal_error)
{
  --let $mrs_client_cmd=$mrs_client_cmd --expected-fatal-error=$mrs_client_arg_expected_fatal_error
}

if ($mrs_client_arg_header_upgrade)
{
  --let $mrs_client_cmd=$mrs_client_cmd --request-header-upgrade=$mrs_client_arg_header_upgrade
}

if ($mrs_client_arg_header_http2_settings)
{
  --let $mrs_client_cmd=$mrs_client_cmd --request-header-http2-settings=$mrs_client_arg_header_http2_settings
}

if ($mrs_client_replace_regexp)
{
  --replace_regex $mrs_client_replace_regexp
}

exec $mrs_client_cmd 2>&1;

--let $mrs_client_arg_header_upgrade=
--let $mrs_client_arg_header_http2_settings=
--let $mrs_client_arg_expected_fatal_error=
--let $mrs_client_arg_protocol=
--let $mrs_client_arg_request_header_authorization=
--let $mrs_client_arg_request_header_host=
--let $mrs_client_exe_args=
--let $mrs_client_arg_path=
--let $mrs_client_arg_payload=
--let $mrs_client_arg_expected_status=
--let $mrs_client_arg_json_pointer=
--let $mrs_client_arg_exclude_json_pointer=
--let $mrs_client_arg_request_type=
--let $mrs_client_arg_display=
--let $mrs_client_arg_write_format=
--let $mrs_client_arg_write_to_file=
--let $mrs_client_arg_session_file=
--let $mrs_client_arg_wait_until_found=
--let $mrs_client_arg_wait_until_status=
--let $mrs_client_arg_response_type=
--let $mrs_client_arg_json_schema_file=
--let $mrs_client_arg_json_schema=
--let $mrs_client_arg_expect_header=
--let $mrs_client_arg_expect_header_value=
--let $mrs_client_replace_regexp=
--let $mrs_client_arg_authentication=
--let $mrs_client_arg_user=
--let $mrs_client_arg_password=
