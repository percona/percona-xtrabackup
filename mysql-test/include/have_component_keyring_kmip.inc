disable_query_log;

#
# Check if the variable KEYRING_KMIP_COMPONENT is set
#
if (!$KEYRING_KMIP_COMPONENT) {
   --skip test requires the environment variable \$KEYRING_FILE_COMPONENT to be set (normally done by mtr), see the file plugin.defs
}

enable_query_log;
