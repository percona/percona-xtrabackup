disable_query_log;
disable_result_log;
# Revoke proxy grants
REVOKE PROXY ON proxied@localhost FROM u1@localhost, u2@localhost, u3@localhost;

# Drop proxy users
DROP USER u1@localhost, u2@localhost, u3@localhost;

# Drop proxied user
DROP USER proxied@localhost;

# Uninstall test_plugin_server
UNINSTALL PLUGIN test_plugin_server;

# Remove component library
let $auth_plugin_path= `SELECT SUBSTR('$PLUGIN_AUTH_OPT/$PLUGIN_AUTH', 14)`;
let $component_connection_control_path= `SELECT SUBSTR('$COMPONENT_CONNECTION_CONTROL_OPT/$PLUGIN_AUTH', 14)`;

if ($auth_plugin_path != $component_connection_control_path)
{
  --remove_file $component_connection_control_path
}
enable_result_log;
enable_query_log;
