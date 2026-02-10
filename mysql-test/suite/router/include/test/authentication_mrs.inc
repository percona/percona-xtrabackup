# Verify MRS authentication support per service where user session is identified
# by JWT token or cookie.
#
# The test assigns "authentication application", to a service and
# checks if default permissions allow to access all object under
# the service that require authentication.
#
# This test is parametrized, to execute it please execute following:
#
#  --let $test_session_type=[cookie|jwt]
#  --let $test_scram=[scram_get|scram_post]
#  --source ../include/test/authentication_mrs.inc
#
--source include/have_router.inc

if (!$test_session_type)
{
   --die The variable is required: test_session_type.
}

if (!$test_scram)
{
  --die The veriable is required: test_scram.
}

--let $_test_auth_app=
--let $extra_mrs_router_id=1
--source ../predefined_setup/configure_router_mrs_root.inc
--source ../schema/basic_schema.sql

## Test starts here
--echo
--echo
--echo ## I. Create three services that have mrs authentication:
--echo #
--echo # 1. under first service create a schema that requires authentication
--echo #    and create two objects that don't require authentication, still
--echo #    they inherit authentication requirement from schema.
--echo # 2. under second service create a schema that doest require authentication
--echo #    and create two objects one of which requires authentication.
--echo #       srv1(mrs auth)->schema1(requires-auth)->object1(doesn't require)
--echo #       srv1(mrs auth)->schema1(requires-auth)->object2(doesn't require)
--echo #       srv2(mrs auth)->schema2(doesn't require)->object3(requires)
--echo #       srv2(mrs auth)->schema2(doesn't require)->object4(doesn't require)
--echo # 3. create services with configured validation of onCompleteRedirection pattern
--echo #
--echo ## II. Verify that non authorized request
--echo #
--echo # 1. Have access to db-object (/srv2/schema2/object4)
--echo # 2. Doesn't have access to db-object (object1,object2,object3)
--echo #
--echo ## III. Authenticate user1 against service: /srv1, and verify it
--echo #
--echo # 1. Have access to db-object (object1,object2,object4)
--echo # 2. Doesn't have access to db-object (object3)
--echo #
--echo ## IV. Authenticate user2 against service: /srv2, and verify it
--echo #
--echo # 1. Have access to db-object (object3,object4)
--echo # 2. Doesn't have access to db-object (object1,object2)
--echo #
--echo ## V. Authenticate using
--echo #
--echo # 1. svc_local and empty redirection (unauthorized)
--echo # 2. svc_local and non matching redirection (unauthorized)
--echo # 3. svc_bad and any redirection (unauthorized)
--echo # 4. svc_aaa and any "aaa!" redirection (unauthorized)
--echo # 5. svc_local and matching redirection (ok)
--echo #
--echo #

--echo
--echo #
--echo # I.1

--source ../mrs/start_object_definition.inc

--let $mrs_add_service_path="/svc1"
--let $mrs_add_host_name=""
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/basic1
--let $mrs_add_schema_auth=TRUE
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/o1
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=table2
--let $mrs_add_db_object_path=/o2
--source ../mrs/db_object/add.inc

if ($test_second_handler)
{
  --let $mrs_add_auth_app=Auth2
  --let $mrs_add_auth_vendor=MRS
  --let $mrs_add_auth_service=/svc1
  --source ../mrs/auth_app/add.inc
  --let $_test_auth_app=--auth-app=Auth1
}


--let $mrs_add_auth_app=Auth1
--let $mrs_add_auth_vendor=MRS
--let $mrs_add_auth_service=/svc1
--source ../mrs/auth_app/add.inc


--let $mrs_add_user_name="svc1_usr"
--let $mrs_add_user_ext_uid=1000
# auth_string: $A$005$VxmHRs/knOXHlsrB0onsYl8YmX0=$zBMKnT9FCep0IayH6aF5RI41z9EIGZoYalzD2MR971o=
# encoded as base64:
--let $mrs_add_user_auth_string="JEEkMDA1JFZ4bUhScy9rbk9YSGxzckIwb25zWWw4WW1YMD0kekJNS25UOUZDZXAwSWF5SDZhRjVSSTQxejlFSUdab1lhbHpEMk1SOTcxbz0="
# Auth-string generated for "test" password
--source ../mrs/user/add.inc

--let $mrs_add_user_name="svc1_usr_blocked"
--let $mrs_add_user_ext_uid=1001
# auth_string: $A$005$VxmHRs/knOXHlsrB0onsYl8YmX0=$zBMKnT9FCep0IayH6aF5RI41z9EIGZoYalzD2MR971o=
# encoded as base64:
--let $mrs_add_user_auth_string="JEEkMDA1JFZ4bUhScy9rbk9YSGxzckIwb25zWWw4WW1YMD0kekJNS25UOUZDZXAwSWF5SDZhRjVSSTQxejlFSUdab1lhbHpEMk1SOTcxbz0="
# Auth-string generated for "test" password
--let $mrs_add_user_login_permitted='0'
--source ../mrs/user/add.inc


--echo
--echo #
--echo # I.2
--let $mrs_add_service_path="/svc2"
--let $mrs_add_host_name=""
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/basic2
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/o3
--let $mrs_add_db_object_auth=TRUE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=table2
--let $mrs_add_db_object_path=/o4
--source ../mrs/db_object/add.inc

--let $mrs_add_auth_app=default authentication
--let $mrs_add_auth_vendor=MRS
--let $mrs_add_auth_service=/svc2
--source ../mrs/auth_app/add.inc

--let $mrs_add_user_name="svc2_usr"
--let $mrs_add_user_ext_uid=2000
# auth_string: $A$005$VxmHRs/knOXHlsrB0onsYl8YmX0=$zBMKnT9FCep0IayH6aF5RI41z9EIGZoYalzD2MR971o=
# encoded as base64:
--let $mrs_add_user_auth_string="JEEkMDA1JFZ4bUhScy9rbk9YSGxzckIwb25zWWw4WW1YMD0kekJNS25UOUZDZXAwSWF5SDZhRjVSSTQxejlFSUdab1lhbHpEMk1SOTcxbz0="
# Auth-string generated for "test" password
--source ../mrs/user/add.inc

# TODO: make sure to test on md-ver when restoring that part
#--echo
#--echo #
#--echo # I.3
#--let $mrs_add_service_path="/svc_local"
#--let $mrs_add_host_name=""
#--let $mrs_add_service_url_validation="^/local/.*/1$"
#--source ../mrs/service/add.inc

#--let $mrs_add_service_path="/svc_aaa"
#--let $mrs_add_host_name=""
#--let $mrs_add_service_url_validation="(a+)+"
#--source ../mrs/service/add.inc

#--let $mrs_add_service_path="/svc_bad"
#--let $mrs_add_host_name=""
#--let $mrs_add_service_url_validation="^/remote/([A-Z]+(\d{2,})$"
#--source ../mrs/service/add.inc

#--let $mrs_add_auth_app=default authentication3
#--let $mrs_add_auth_vendor=MRS
#--let $mrs_add_auth_services=('/svc_bad', '/svc_aaa', '/svc_local')
#--source ../mrs/auth_app/add.inc

--source ../mrs/end_object_definition.inc

--echo
--echo #
--echo # II.1
exec $MRS_CLIENT_ARGS
  --wait-until-status 60
  --exclude-json-pointer  "/*"
  --path /svc2/basic2/o4;


--echo
--echo #
--echo # II.2
exec $MRS_CLIENT_ARGS
  --expected-status Unauthorized
  --path /svc2/basic2/o3;

exec $MRS_CLIENT_ARGS
  --expected-status Unauthorized
  --path /svc1/basic1/o1;

exec $MRS_CLIENT_ARGS
  --expected-status Unauthorized
  --path /svc1/basic1/o2;


--echo
--echo #
--echo # III.1
--echo Check no access for non existing user
exec $MRS_CLIENT_ARGS
  -a $test_scram
  --path /svc1/authentication/login
  -u root_not_existing
  $_test_auth_app
  --expected-status Unauthorized
  --session-type $test_session_type;

--echo Check the username is case-sensitive
exec $MRS_CLIENT_ARGS
  -a $test_scram
  --path /svc1/authentication/login
  -u Svc1_usr
  -p test
  $_test_auth_app
  --expected-status Unauthorized
  --session-type $test_session_type;

--echo Check no access with bad password
exec $MRS_CLIENT_ARGS
  -a $test_scram
  --path /svc1/authentication/login
  -u svc1_usr
  -p invalid_password
  $_test_auth_app
  --expected-status Unauthorized
  --session-type $test_session_type;

#
#TODO: getting ERROR: Challenge response doesn't contain 'r' field. here
#
#exec $MRS_CLIENT_ARGS
#  -a $test_scram
#  --path /svc1/authentication/login
#  -u svc1_usr_blocked
#  -p test
#  $_test_auth_app
#  --expected-status Unauthorized
#  --session-type $test_session_type;

exec $MRS_CLIENT_ARGS
  -a $test_scram
  --path /svc1/authentication/login
  -u svc1_usr
  -p test
  $_test_auth_app
  --session-type $test_session_type
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

exec $MRS_CLIENT_ARGS
  -a $test_scram
  --path /svc2/authentication/login
  -u svc2_usr
  -p test
  --session-type $test_session_type
  --session-file $MYSQL_TMP_DIR/user2_session.dat;

### User1 access
exec $MRS_CLIENT_ARGS
  --path /svc1/basic1/o1
  --json-schema-file $SCHEMA_DIR/items_first_less_than_limit.sch
  --exclude-json-pointer  "/*"
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

exec $MRS_CLIENT_ARGS
  --path /svc1/basic1/o2
  --json-schema-file $SCHEMA_DIR/items_first_less_than_limit.sch
  --exclude-json-pointer  "/*"
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

exec $MRS_CLIENT_ARGS
  --json-schema-file $SCHEMA_DIR/items_first_less_than_limit.sch
  --exclude-json-pointer  "/*"
  --path /svc2/basic2/o4
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

--echo
--echo #
--echo # III.2
exec $MRS_CLIENT_ARGS
  --expected-status Unauthorized
  --path /svc2/basic2/o3
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

--echo
--echo #
--echo # IV.1
exec $MRS_CLIENT_ARGS
  --json-schema-file $SCHEMA_DIR/items_first_less_than_limit.sch
  --exclude-json-pointer  "/*"
  --path /svc2/basic2/o3
  --session-file $MYSQL_TMP_DIR/user2_session.dat;

--echo
--echo #
--echo # IV.2
### User2 access
exec $MRS_CLIENT_ARGS
  --path /svc1/basic1/o1
  --expected-status Unauthorized
  --session-file $MYSQL_TMP_DIR/user2_session.dat;

exec $MRS_CLIENT_ARGS
  --path /svc1/basic1/o2
  --expected-status Unauthorized
  --session-file $MYSQL_TMP_DIR/user2_session.dat;

exec $MRS_CLIENT_ARGS
  --json-schema-file $SCHEMA_DIR/items_first_less_than_limit.sch
  --exclude-json-pointer  "/*"
  --path /svc2/basic2/o4
  --session-file $MYSQL_TMP_DIR/user2_session.dat;

#
#TODO: getting ERROR: Challenge response doesn't contain 'r' field. here
#
--echo
--echo #
--echo # V.1
#exec $MRS_CLIENT_ARGS
#  -a $test_scram
#  --expected-status Unauthorized
#  --path /svc_local/authentication/login?onCompletionRedirect=
#  -u svc2_usr
#  -p test
#  --session-type $test_session_type;
  
--echo
--echo #
--echo # V.2
#exec $MRS_CLIENT_ARGS
#  -a $test_scram
#  --expected-status Unauthorized
#  --path /svc_local/authentication/login?onCompletionRedirect=/remote/index.html
#  -u svc2_usr
#  -p test
#  --session-type $test_session_type;
  
--echo
--echo #
--echo # V.3
#exec $MRS_CLIENT_ARGS
#  -a $test_scram
#  --expected-status Unauthorized
#  --path /svc_bad/authentication/login?onCompletionRedirect=/something/here/file.txt
#  -u svc2_usr
#  -p test
#  --session-type $test_session_type;
  
--echo
--echo #
--echo # V.4
#exec $MRS_CLIENT_ARGS
#  -a $test_scram
#  --expected-status Unauthorized
# --path /svc_aaa/authentication/login?onCompletionRedirect=aaa!
#  -u svc2_usr
#  -p test
#  --session-type $test_session_type;
  
--echo
--echo #
--echo # V.5
#exec $MRS_CLIENT_ARGS
#  -a $test_scram
#  --expected-status Unauthorized
#  --path /svc_local/authentication/login?onCompletionRedirect=/local/something/1
#  -u svc2_usr
#  -p test
#  --session-type $test_session_type;

# Cleanup
remove_file $MYSQL_TMP_DIR/user1_session.dat;
remove_file $MYSQL_TMP_DIR/user2_session.dat;
--source ../mrs/cleanup.inc

--let $test_second_handler=
--let $test_session_type=
--let $test_scram=

