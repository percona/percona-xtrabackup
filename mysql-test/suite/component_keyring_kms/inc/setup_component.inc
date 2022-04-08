# Setup test to use keyring component

# Used environment variables:
# KMS_REGION
# KMS_KEY
# KMS_AUTH_KEY
# KMS_SECURE_ACCESS_KEY

--echo # ----------------------------------------------------------------------
--echo # Setup

--let PLUGIN_DIR_OPT = $KEYRING_KMS_COMPONENT_OPT

# Data directory location
--let CURRENT_DATADIR = `SELECT @@datadir`

--let COMPONENT_LIBRARY = `SELECT SUBSTRING_INDEX('$KEYRING_KMS_COMPONENT_LOAD', '=', -1)`
--let COMPONENT_DIR = $KEYRING_KMS_COMPONENT_DIR
--let COMPONENT_NAME = `SELECT SUBSTRING_INDEX('$COMPONENT_LIBRARY', '.', 1)`
--source include/keyring_tests/helper/global_keyring_create_config.inc

# Create local keyring config
--let KEYRING_KMS_PATH = `SELECT CONCAT( '$MYSQLTEST_VARDIR', '/keyring_kms')`
--let KEYRING_CONFIG_CONTENT = `SELECT CONCAT('{ \"path\": \"', '$KEYRING_KMS_PATH', '\", \"region\": \"', '$KMS_REGION', '\", \"kms_key\": \"', '$KMS_KEY', '\", \"auth_key\": \"', '$KMS_AUTH_KEY', '\", \"secret_access_key\": \"', '$KMS_SECRET_ACCESS_KEY', '\", \"read_only\": false }')`
--source include/keyring_tests/helper/local_keyring_create_config.inc

# Create local manifest file for current server instance
--let LOCAL_MANIFEST_CONTENT = `SELECT CONCAT('{ \"components\": \"file://', '$COMPONENT_NAME', '\" }')`
--source include/keyring_tests/helper/instance_create_manifest.inc

# Restart server with manifest file 
--source include/keyring_tests/helper/start_server_with_manifest.inc
--echo # ----------------------------------------------------------------------

