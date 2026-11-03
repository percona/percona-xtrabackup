# Setup test to use keyring component on replica server only

--echo # ----------------------------------------------------------------------
--echo # Setup

--source include/rpl/init_source_replica.inc

--let RPL_PLUGIN_DIR_OPT = $PERCONA_KEYRING_ENCRYPTED_FILE_COMPONENT_OPT

--let COMPONENT_LIBRARY = `SELECT SUBSTRING_INDEX('$PERCONA_KEYRING_ENCRYPTED_FILE_COMPONENT_LOAD', '=', -1)`
--let COMPONENT_DIR = $PERCONA_KEYRING_ENCRYPTED_FILE_COMPONENT_DIR
--let COMPONENT_NAME = `SELECT SUBSTRING_INDEX('$COMPONENT_LIBRARY', '.', 1)`

# Create keyring config for replica server

--source include/rpl/connection_replica.inc
--let CURRENT_DATADIR = `SELECT @@datadir`

# Create local keyring config
--let KEYRING_FILE_PATH = `SELECT CONCAT( '$MYSQLTEST_VARDIR', '/keyring_file_replica')`
--let KEYRING_CONFIG_CONTENT = `SELECT CONCAT('{ \"path\": \"', '$KEYRING_FILE_PATH','\", \"read_only\": false , \"password\": \"secret_password\", \"iterations\": 42 }')`
--source include/keyring_tests/helper/local_keyring_create_config.inc

# Create local manifest file for current server instance
--let LOCAL_MANIFEST_CONTENT = `SELECT CONCAT('{ \"components\": \"file://', '$COMPONENT_NAME', '\" }')`
--source include/keyring_tests/helper/instance_create_manifest.inc

--let REPLICA_DATADIR = $CURRENT_DATADIR
--let REPLICA_KEYRING_FILE_PATH = $KEYRING_FILE_PATH

--source include/rpl/connection_source.inc
--let CURRENT_DATADIR = `SELECT @@datadir`
--echo # ----------------------------------------------------------------------
