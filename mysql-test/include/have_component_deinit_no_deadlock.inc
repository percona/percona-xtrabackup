disable_query_log;

#
# Check if the variable $COMPONENT_TEST_COMPONENT_DEINIT_NO_DEADLOCK is set
#
if (!$COMPONENT_TEST_COMPONENT_DEINIT_NO_DEADLOCK) {
   --skip component requires the environment variable \$COMPONENT_TEST_COMPONENT_DEINIT_NO_DEADLOCK to be set (normally done by mtr), see the file plugin.defs
}

#
## Check if --plugin-dir was setup for the component
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$COMPONENT_TEST_COMPONENT_DEINIT_NO_DEADLOCK_OPT/'`) {
    --skip component_test requires that --plugin-dir is set to the component_test dir (the .opt file does not contain \$COMPONENT_TEST_COMPONENT_DEINIT_NO_DEADLOCK_OPT)
}

enable_query_log;
