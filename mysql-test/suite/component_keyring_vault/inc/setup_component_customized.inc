# Setup test to use keyring component

--echo # ----------------------------------------------------------------------
--echo # Setup

--let PLUGIN_DIR_OPT = $KEYRING_VAULT_COMPONENT_OPT

--let COMPONENT_LIBRARY = `SELECT SUBSTRING_INDEX('$KEYRING_VAULT_COMPONENT_LOAD', '=', -1)`
--let COMPONENT_DIR = $KEYRING_VAULT_COMPONENT_DIR
--let COMPONENT_NAME = `SELECT SUBSTRING_INDEX('$COMPONENT_LIBRARY', '.', 1)`
--let GLOBAL_MANIFEST_CONTENT = `SELECT CONCAT('{ \"components\": \"file://', '$COMPONENT_NAME', '\" }')`
# Create manifest file for mysqld binary
--source include/keyring_tests/helper/binary_create_customized_manifest.inc

# Create global keyring config
--source generate_comp_config_content.inc
--let KEYRING_VAULT_PATH = `SELECT CONCAT( '$MYSQLTEST_VARDIR', '/keyring_VAULT')`
--let KEYRING_CONFIG_CONTENT = `SELECT CONCAT('{ ', '$config_content', ' }')`
--source include/keyring_tests/helper/global_keyring_create_customized_config.inc

# Restart server with manifest file 
--source include/keyring_tests/helper/start_server_with_manifest.inc
--echo # ----------------------------------------------------------------------

