disable_query_log;
#
# Check if server has support for loading component
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip component_connection_control requires dynamic loading
}

#
# Check if the variable COMPONENT_CONNECTION_CONTROL is set
#
if (!$COMPONENT_CONNECTION_CONTROL) {
  --skip component_connection_control requires the environment variable \$COMPONENT_CONNECTION_CONTROL to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for null_audit_db
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$COMPONENT_CONNECTION_CONTROL_OPT/'`) {
  --skip component_connection_control requires that --plugin-dir is set to the component_connection_control dir (either the .opt file does not contain \$COMPONENT_CONNECTION_CONTROL_OPT or another component is in use)
}
enable_query_log;
