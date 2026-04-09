# Teardown

--echo # ----------------------------------------------------------------------
--echo # Teardown
# Remove keyring file
--source include/keyring_tests/helper/local_keyring_file_remove.inc

# Drop global keyring config
--source include/keyring_tests/helper/global_keyring_remove_config.inc

# Remove manifest file for mysqld binary
--source include/keyring_tests/helper/binary_remove_manifest.inc

# restore global files (needs COMPONENT_DIR and COMPONENT_NAME)
--source include/keyring_tests/helper/restore_global_files_once.inc

# Restart server without manifest file
--source include/keyring_tests/helper/cleanup_server_with_manifest.inc
--echo # ----------------------------------------------------------------------
