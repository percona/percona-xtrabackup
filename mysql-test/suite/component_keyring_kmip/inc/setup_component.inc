# Setup test to use keyring component

# Used environment variables:
# KMIP_PORT
# KMIP_ADDR
# KMIP_CLIENT_CA
# KMIP_CLIENT_KEY
# KMIP_SERVER_CA

--echo # ----------------------------------------------------------------------
--echo # Setup

--let PLUGIN_DIR_OPT = $KEYRING_KMIP_COMPONENT_OPT

# Data directory location
--let CURRENT_DATADIR = `SELECT @@datadir`

--let COMPONENT_LIBRARY = `SELECT SUBSTRING_INDEX('$KEYRING_KMIP_COMPONENT_LOAD', '=', -1)`
--let COMPONENT_DIR = $KEYRING_KMIP_COMPONENT_DIR
--let COMPONENT_NAME = `SELECT SUBSTRING_INDEX('$COMPONENT_LIBRARY', '.', 1)`
--source include/keyring_tests/helper/global_keyring_create_config.inc

# Create local keyring config
--let KEYRING_KMIP_PATH = `SELECT CONCAT( '$MYSQLTEST_VARDIR', '/keyring_kmip')`
--let KEYRING_CONFIG_CONTENT = `SELECT CONCAT('{ \"path\": \"', '$KEYRING_KMIP_PATH', '\", \"server_addr\": \"', '$KMIP_ADDR', '\", \"server_port\": \"', '$KMIP_PORT', '\", \"client_ca\": \"', '$KMIP_CLIENT_CA', '\", \"client_key\": \"', '$KMIP_CLIENT_KEY', '\", \"server_ca\": \"', '$KMIP_SERVER_CA', '\" }')`
--source include/keyring_tests/helper/local_keyring_create_config.inc

# Create local manifest file for current server instance
--let LOCAL_MANIFEST_CONTENT = `SELECT CONCAT('{ \"components\": \"file://', '$COMPONENT_NAME', '\" }')`
--source include/keyring_tests/helper/instance_create_manifest.inc

# Restart server with manifest file 
--source include/keyring_tests/helper/start_server_with_manifest.inc
--echo # ----------------------------------------------------------------------

