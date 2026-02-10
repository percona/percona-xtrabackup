
# Request to be checked

if (!$mrs_check_path)
{
    --die '$mrs_check_path' required
}
if (!$mrs_check_request_type)
{
    --let $mrs_check_request_type=GET
}
if (!$mrs_check_expected_status)
{
    --let $mrs_check_expected_status=200
}
if (!$mrs_check_expected_status2)
{
    --let $mrs_check_expected_status2=200
}

if (!$mrs_check_pattern)
{
    --die '$mrs_check_pattern' required
}
if (!$mrs_check_cached)
{
    --let $final_delta=2
}
if ($mrs_check_cached)
{
    --let $final_delta=1
}
if ($mrs_check_result == "")
{
    --let $mrs_check_result=1
}

--let $mrs_check_statements_history_sql_text=$mrs_check_pattern

# first execution should cache it, so logs 1 query
--let $mrs_client_arg_path=$mrs_check_path
--let $mrs_client_arg_payload=$mrs_check_payload
--let $mrs_client_arg_request_type=$mrs_check_request_type
--let $mrs_client_arg_write_format=mtr
--let $mrs_client_arg_write_to_file=$MYSQL_TMP_DIR/result1.json
--let $mrs_client_arg_expected_status=$mrs_check_expected_status
--let $mrs_client_arg_session_file=$mrs_check_session_file
--let $mrs_client_arg_display=$mrs_check_display
--let $mrs_client_arg_response_type=$mrs_check_response_type
# Masking off the gtid value
--let $mrs_client_replace_regexp=/(.*)"gtid": ".*"/\1"gtid": "GTID"/
--source mrs_client.inc


--let $mrs_check_statements_history_initial=1
--let $mrs_check_statements_history_delta=1
--source ../check_statements_history.inc

# second execution should load from cache, so no additional query executed
--replace_regex /"gtid":.*/"gtid":GTID/
--let $mrs_client_arg_path=$mrs_check_path
--let $mrs_client_arg_payload=$mrs_check_payload
--let $mrs_client_arg_request_type=$mrs_check_request_type
--let $mrs_client_arg_write_format=mtr
--let $mrs_client_arg_write_to_file=$MYSQL_TMP_DIR/result2.json
--let $mrs_client_arg_expected_status=$mrs_check_expected_status2
--let $mrs_client_arg_session_file=$mrs_check_session_file
--let $mrs_client_arg_display=$mrs_check_display
--let $mrs_client_arg_response_type=$mrs_check_response_type
--source mrs_client.inc

--let $mrs_check_statements_history_delta=$final_delta
--source ../check_statements_history.inc

--source $MYSQL_TMP_DIR/result1.json
--let $result1=$mrs_result

--source $MYSQL_TMP_DIR/result2.json
--let $result2=$mrs_result

if ($mrs_check_result==1)
{
    if ($result1 != $result2) 
    {
        --echo "RESULT1:" $result1
        --echo "RESULT2:" $result2
    }
    --assert($result1==$result2)
}

--let $mrs_check_result=
--let $mrs_check_cached=
--let $mrs_check_payload=
--let $mrs_check_request_type=
--let $mrs_check_expected_status=
--let $mrs_check_expected_status2=
--let $mrs_check_session_file=
--let $mrs_check_display=
--let $mrs_check_response_type=
