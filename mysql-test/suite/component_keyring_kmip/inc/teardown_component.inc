# Teardown

--echo # ----------------------------------------------------------------------
--echo # Teardown
# Remove local manifest file for current server instance
--source include/keyring_tests/helper/instance_remove_manifest.inc

# Remove keyring kmip
--source include/keyring_tests/helper/local_keyring_kmip_remove.inc

# Remove local keyring config
--source include/keyring_tests/helper/local_keyring_remove_config.inc

# Remove global keyring config
--source include/keyring_tests/helper/global_keyring_remove_config.inc

# Restart server without manifest file
--source include/keyring_tests/helper/cleanup_server_with_manifest.inc
--echo # ----------------------------------------------------------------------

--exec python std_data/kmip_clear.py
