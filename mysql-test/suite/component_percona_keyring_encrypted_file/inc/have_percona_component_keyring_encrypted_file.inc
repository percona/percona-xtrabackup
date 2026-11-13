disable_query_log;

#
# Check if the variable PERCONA_KEYRING_ENCRYPTED_FILE_COMPONENT is set
#
if (!$PERCONA_KEYRING_ENCRYPTED_FILE_COMPONENT) {
  --skip test requires the environment variable \$PERCONA_KEYRING_ENCRYPTED_FILE_COMPONENT to be set (normally done by mtr if plugin exists), see the file plugin.defs
}

enable_query_log;
