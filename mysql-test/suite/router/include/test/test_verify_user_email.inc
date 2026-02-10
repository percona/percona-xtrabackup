
if (!$test_user_idx)
{
  --die Variable test_user_idx is required.
}


--let $mrs_client_arg_path='/a/authentication/login'
--let $mrs_client_arg_authentication=basic
--let $mrs_client_arg_user=user$test_user_idx
--let $mrs_client_arg_password=secret$test_user_idx
--let $mrs_client_arg_session_file='$MYSQL_TMP_DIR/user_session.dat'
--let $mrs_client_arg_display=none
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/a/authentication/user'
--let $mrs_client_arg_session_file='$MYSQL_TMP_DIR/user_session.dat'
--let $mrs_client_arg_json_pointer=/name,/email
--source ../mrs/mrs_client.inc


--remove_file $MYSQL_TMP_DIR/user_session.dat

--let $test_user_idx=
