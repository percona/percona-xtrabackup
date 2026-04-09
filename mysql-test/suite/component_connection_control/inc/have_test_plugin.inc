disable_query_log;
#
# Check if server has support for loading plugin
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip component_connection_control requires dynamic loading
}

#
# Check if the variable PLUGIN_AUTH is set
#
if (!$PLUGIN_AUTH) {
  --skip component_connection_control requires the environment variable \$PLUGIN_AUTH to be set (normally done by mtr)
}

enable_query_log;
