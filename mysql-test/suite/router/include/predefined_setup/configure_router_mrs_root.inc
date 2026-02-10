# ==== Purpose ====
#
# Reusable configuration of Router with MRS plugin,
# that uses root account to access both meta and user data.
#
# ==== Usage ====
#
# --source ../include/predefined_setup/configure_router_mrs_root.inc
#
--source ../mrs/initialize_schema.inc

if (!$mrs_user_meta)
{
  --let $mrs_user_meta=root@localhost
}
if (!$mrs_user_data) {
  --let $mrs_user_data=root@localhost
}
--source ../mrs/user_configure.inc

--let $mrs_host_and_port=127.0.0.1:$HTTP_SERVER_PORT
if ($HTTP_SERVER_SSL==1)
{
  --let MRS_URL=https://$mrs_host_and_port
  --let MRS_CLIENT_ARGS=$MRS_CLIENT --url https://$mrs_host_and_port
}

if (!$MRS_CLIENT_ARGS)
{
  --let MRS_URL=http://$mrs_host_and_port
  --let MRS_CLIENT_ARGS=$MRS_CLIENT --url http://$mrs_host_and_port
}

--source ../test_dir.inc
--let $SCHEMA_DIR=$TEST_DIR/../include/json_schema/

if ($_MRS_CONTROL_START)
{
  --exec $MRS_CLIENT_ARGS --path /debug?do=start --wait-until-status=60 --display none
  --let $_mrs_started=1
}
