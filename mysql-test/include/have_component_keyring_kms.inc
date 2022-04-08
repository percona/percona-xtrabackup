disable_query_log;

#
# Check if the variable KEYRING_KMS_COMPONENT is set
#
if (!$KEYRING_KMS_COMPONENT) {
   --skip test requires the environment variable \$KEYRING_KMS_COMPONENT to be set (normally done by mtr), see the file plugin.defs
}

enable_query_log;
