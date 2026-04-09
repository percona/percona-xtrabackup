# Test checks differrent operators used on different time types
#
# Example of use:
#
# --let $test_encode_date_begin=
# --let $test_encode_date_end=
# --let $test_time_type=
# --source ../include/test/test_table_type_datetime.inc
#

--source ../predefined_setup/configure_router_mrs_root.inc

eval CREATE TABLE `test`.`datetime_values` (
             `id` int NOT NULL AUTO_INCREMENT, `dt` $test_time_type(1) DEFAULT NULL, PRIMARY KEY (`id`));
INSERT INTO `test`.`datetime_values` (id, dt) VALUES 
           (1, '2024-10-01 14:34:00.1234'),
           (2, '2025-10-01 18:34:00.1234'),
           (3, NULL),
           (4, '2026-10-01'),
           (5, '1970-10-01 06:34:20.2345');

--echo
--echo
--echo ## I. Verify JSON encoding of $test_time_type type, returned in FEED response
--echo #
--echo # 1. Verify JSON response containing datetime values encoded as string generated from column of type $test_time_type
--echo #    returning single object.
--echo # 2. Verify JSON response containing datetime values encoded as string generated from column of type $test_time_type
--echo #    returning multiple objects.
--echo # 3. Verify that object can be updated with different datimetime-values encoded as string and response
--echo #    contains same data but truncated to precision of datatype in that column ($test_time_type(1)).
--echo # 4. Verify error message, in case of invalid $test_time_type value.
--echo #
--echo ## II. Verify JSON encoding of $test_time_type in filter object with eq operator.
--echo # 1. Verify that objects can be filtered using $test_time_type column using datetime-value encoded as string after truncation to $test_time_type(1).
--echo # 2. Verify that objects can't be found using $test_time_type column using accurate datetime-value encoded as string (same precision as in insert).
--echo # 3. Verify that objects can be filtered with null value.
--echo #
--echo ## III. Verify JSON encoding of $test_time_type in filter object with gt operator.
--echo # 1. Verify that objects can be filtered using $test_time_type column using datetime-value encoded as string
--echo #

--source ../mrs/start_object_definition.inc

# The test uses HOST to have only paths in HREF fields.
--let $mrs_add_service_path="/svc"
--let $mrs_add_host_name=""
--source ../mrs/service/add.inc

--let $mrs_add_schema=test
--let $mrs_add_schema_path=/test
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=datetime_values
--let $mrs_add_db_object_path=/datetime
--source ../mrs/db_object/add.inc

--source ../mrs/end_object_definition.inc

--exec $MRS_CLIENT_ARGS --path /svc/test/datetime/1 --wait-until-status 60 --display none

--let $MRS_CLIENT_ARGS_LIST=$MRS_CLIENT_ARGS --json-pointer /items/*/dt
--let $MRS_CLIENT_ARGS_OBJ=$MRS_CLIENT_ARGS --json-pointer /dt

--echo
--echo #
--echo # I.1
--exec $MRS_CLIENT_ARGS_OBJ --path /svc/test/datetime/2

--echo
--echo #
--echo # I.2
--exec $MRS_CLIENT_ARGS_LIST --path /svc/test/datetime

--echo
--echo #
--echo # I.3
--let $mrs_client_replace_regexp=/(.*)"gtid": ".*"/\1"gtid": "GTID"/
--let $mrs_client_arg_path='/svc/test/datetime/2'
--let $mrs_client_arg_payload='{"id":2,"dt":"2002-02-02 02:02:02.1234"}'
--let $mrs_client_arg_request_type='PUT'
--source ../mrs/mrs_client.inc

--let $mrs_client_replace_regexp=/(.*)"gtid": ".*"/\1"gtid": "GTID"/
--let $mrs_client_arg_path='/svc/test/datetime/2'
--let $mrs_client_arg_payload='{"id":2,"dt":"2000-01-01 00:00:00.1234"}'
--let $mrs_client_arg_request_type='PUT'
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # I.4
--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":$test_encode_date_begin"14:34:00.1000"$test_encode_date_end}'
--let $mrs_client_arg_expected_status=BadRequest
let $mrs_client_arg_json_schema='
{
  "type": "object",
  "required": ["message", "status"],
  "properties": {
     "message":{"type":"string", "pattern":"$test_time_type"}
  }
}';
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":$test_encode_date_begin"TEST STRING"$test_encode_date_end}'
--let $mrs_client_arg_expected_status=BadRequest
let $mrs_client_arg_json_schema='
{
  "type": "object",
  "required": ["message", "status"],
  "properties": {
     "message":{"type":"string", "pattern":"$test_time_type"}
  }
}';
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":$test_encode_date_begin false$test_encode_date_end}'
--let $mrs_client_arg_expected_status=BadRequest
let $mrs_client_arg_json_schema='
{
  "type": "object",
  "required": ["message", "status"],
  "properties": {
     "message":{"type":"string", "pattern":"FilterObject"}
  }
}';
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # II.1

--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":$test_encode_date_begin"2024-10-01 14:34:00.1000"$test_encode_date_end}'
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/db_object_table_items_non_empty.sch
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":$test_encode_date_begin"2024-10-01 14:34:00.1"$test_encode_date_end}'
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/db_object_table_items_non_empty.sch
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":$test_encode_date_begin"2000-01-01 00:00:00.1"$test_encode_date_end}'
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/db_object_table_items_non_empty.sch
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":{"\\\$eq":$test_encode_date_begin"2024-10-01 14:34:00.1000"$test_encode_date_end}}'
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/db_object_table_items_non_empty.sch
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # II.2
--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":$test_encode_date_begin"2024-10-01 14:34:00.1234"$test_encode_date_end}'
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/db_object_table_items_empty.sch
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":$test_encode_date_begin"2000-01-01 00:00:00.1234"$test_encode_date_end}'
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/db_object_table_items_empty.sch
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # II.3
--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":{"\\\$null":null}}'
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/db_object_table_items_non_empty.sch
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # III.1
--let $mrs_client_arg_path='/svc/test/datetime/?q={"dt":{"\\\$gt":$test_encode_date_begin"2024-10-01 14:30:00.1234"$test_encode_date_end}}'
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/db_object_table_items_non_empty.sch
--source ../mrs/mrs_client.inc


# Cleanup
DROP TABLE `test`.`datetime_values`;
--source ../mrs/cleanup.inc

--let $test_encode_date_begin=
--let $test_encode_date_end=
--let $test_time_type=
