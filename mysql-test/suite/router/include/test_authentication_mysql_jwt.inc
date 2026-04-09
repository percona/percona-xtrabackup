# Verify authentication support per service where user session is identified
# by JWT token.
#
# The test assigns "authentication application", to a service and
# checks if default permissions allow to access all object under
# the service that require authentication.
#
# This test is parametrized, to execute it please execute following:
#
#    --let $test_authentication_method=BASIC|BASIC_JSON
#    --let $test_authentication_failure_status=Unauthorized|TemporaryRedirect
#    [--let $test_is_get_request=1]
#    [--let $test_authentication_failure_extra_ops=--expected-header=WWW-Authenticate=basic]
#    --source ../include/test_authentication_mysql_jwt.inc
#
--source include/have_router.inc

--let $extra_mrs_router_id=1
--source ../include/predefined_setup/configure_router_mrs_root.inc
--source ../include/schema/basic_schema.sql

CREATE USER user_valid@'%' IDENTIFIED BY 'pwd';
CREATE USER user_not_reg@'%' IDENTIFIED BY 'pwd';
CREATE USER user_blocked@'%' IDENTIFIED BY 'pwd';

## Test starts here
--echo
--echo
--echo ## I. Create three services that have basic-mysql authentication:
--echo #
--echo # 1. under first service create a schema that requires authentication
--echo #    and create two objects that don't require authentication, still
--echo #    they inherit authentication requirement from schema.
--echo # 2. under second service create a schema that doest require authentication
--echo #    and create two objects one of which requires authentication.
--echo #       srv1(basic auth)->schema1(requires-auth)->object1(doesn't require)
--echo #       srv1(basic auth)->schema1(requires-auth)->object2(doesn't require)
--echo #       srv2(basic auth)->schema2(doesn't require)->object3(requires)
--echo #       srv2(basic auth)->schema2(doesn't require)->object4(doesn't require)
--echo # 3. under thrid service create a schema that doest require authentication
--echo #    and create on objects. The authentication-application must be limited to
--echo #    registered users.
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
--echo ## V. Authenticate against service: /srv3, and verify it
--echo #
--echo # 1. Verify that registered user can login
--echo # 2. Verify that not-registered user can't login
--echo # 3. Verify that blocked user can't login
--echo #
--echo #


--echo
--echo #
--echo # I.1

--source ../include/mrs/start_object_definition.inc

--let $mrs_add_service_path="/svc1"
--let $mrs_add_host_name=""

--source ../include/mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/basic1
--let $mrs_add_schema_auth=TRUE
--source ../include/mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/o1
--source ../include/mrs/db_object/add.inc

--let $mrs_add_db_object=table2
--let $mrs_add_db_object_path=/o2
--source ../include/mrs/db_object/add.inc

--let $mrs_add_auth_app=default authentication-1
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/svc1
--source ../include/mrs/auth_app/add.inc


--echo
--echo #
--echo # I.2
--let $mrs_add_service_path="/svc2"
--let $mrs_add_host_name=""
--source ../include/mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/basic2
--source ../include/mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/o3
--let $mrs_add_db_object_auth=TRUE
--source ../include/mrs/db_object/add.inc

--let $mrs_add_db_object=table2
--let $mrs_add_db_object_path=/o4
--source ../include/mrs/db_object/add.inc

--let $mrs_add_auth_app=default authentication-2
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/svc2
--source ../include/mrs/auth_app/add.inc


--echo
--echo #
--echo # I.3
--let $mrs_add_service_path="/svc3"
--let $mrs_add_host_name=""
--source ../include/mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/basic
--source ../include/mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/o
--let $mrs_add_db_object_auth=TRUE
--source ../include/mrs/db_object/add.inc

--let $mrs_add_db_object=table2
--let $mrs_add_db_object_path=/o4
--source ../include/mrs/db_object/add.inc

--let $mrs_add_auth_app=app3
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/svc3
--let $mrs_add_auth_registered_users_only=1
--source ../include/mrs/auth_app/add.inc

--let $mrs_add_user_ext_uid="user_blocked@%"
--let $mrs_add_user_name='user_blocked'
--let $mrs_add_user_login_permitted='0'
--let $mrs_add_user_auth_string=' '
--source ../include/mrs/user/add.inc

--let $mrs_add_user_ext_uid="user_valid@%"
--let $mrs_add_user_name='user_valid'
--let $mrs_add_user_login_permitted='1'
--let $mrs_add_user_auth_string=' '
--source ../include/mrs/user/add.inc

--source ../include/mrs/end_object_definition.inc

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
if ($test_is_get_request)
{
exec $MRS_CLIENT_ARGS
   --path /svc1/authentication/login
   -r raw
   --expected-status $test_authentication_failure_status
   $test_authentication_failure_extra_ops;
}

if (!$test_is_get_request)
{
  exec $MRS_CLIENT_ARGS
    -a $test_authentication_method
    --path /svc1/authentication/login
    -u root_not_existing
    --expected-status $test_authentication_failure_status
    $test_authentication_failure_extra_ops
    --session-type $test_type;
}

exec $MRS_CLIENT_ARGS
  -a $test_authentication_method
  --path /svc1/authentication/login
  -u user_valid
  -p pwd
  --session-type $test_type
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

exec $MRS_CLIENT_ARGS
  -a $test_authentication_method
  --path /svc2/authentication/login
  -u user_valid
  -p pwd
  --session-type $test_type
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

--echo
--echo #
--echo # V.1
exec $MRS_CLIENT_ARGS
  -a $test_authentication_method
  --path /svc3/authentication/login
  -u user_valid
  -p pwd
  --session-type $test_type
  --session-file $MYSQL_TMP_DIR/user3_session.dat;

--echo
--echo #
--echo # V.2
exec $MRS_CLIENT_ARGS
  -a $test_authentication_method
  --path /svc3/authentication/login
  -u user_not_reg
  -p pwd
  --session-type $test_type
  --expected-status $test_authentication_failure_status;

--echo
--echo #
--echo # V.3
exec $MRS_CLIENT_ARGS
  -a $test_authentication_method
  --path /svc3/authentication/login
  -u user_blocked
  -p pwd
  --session-type $test_type
  --expected-status $test_authentication_failure_status;

# Cleanup
DROP USER user_valid@'%';
DROP USER user_not_reg@'%';
DROP USER user_blocked@'%';
remove_file $MYSQL_TMP_DIR/user1_session.dat;
remove_file $MYSQL_TMP_DIR/user2_session.dat;
remove_file $MYSQL_TMP_DIR/user3_session.dat;
--source ../include/mrs/cleanup.inc
--let $test_is_get_request=
