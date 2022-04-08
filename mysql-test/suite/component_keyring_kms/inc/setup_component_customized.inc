# Setup test to use keyring component

--echo # ----------------------------------------------------------------------
--echo # Setup

--let PLUGIN_DIR_OPT = $KEYRING_KMS_COMPONENT_OPT

--let COMPONENT_LIBRARY = `SELECT SUBSTRING_INDEX('$KEYRING_KMS_COMPONENT_LOAD', '=', -1)`
--let COMPONENT_DIR = $KEYRING_KMS_COMPONENT_DIR
--let COMPONENT_NAME = `SELECT SUBSTRING_INDEX('$COMPONENT_LIBRARY', '.', 1)`
--let GLOBAL_MANIFEST_CONTENT = `SELECT CONCAT('{ \"components\": \"file://', '$COMPONENT_NAME', '\" }')`
# Create manifest file for mysqld binary
--source include/keyring_tests/helper/binary_create_customized_manifest.inc

# Create global keyring config
--let KEYRING_KMS_PATH = `SELECT CONCAT( '$MYSQLTEST_VARDIR', '/keyring_kms')`
--let KEYRING_CONFIG_CONTENT = `SELECT CONCAT('{ \"path\": \"', '$KEYRING_KMS_PATH', '\", \"region\": \"', '$KMS_REGION', '\", \"kms_key\": \"', '$KMS_KEY', '\", \"auth_key\": \"', '$KMS_AUTH_KEY', '\", \"secret_access_key\": \"', '$KMS_SECRET_ACCESS_KEY', '\", \"read_only\": false }')`
--source include/keyring_tests/helper/global_keyring_create_customized_config.inc

# Restart server with manifest file 
--source include/keyring_tests/helper/start_server_with_manifest.inc
--echo # ----------------------------------------------------------------------

