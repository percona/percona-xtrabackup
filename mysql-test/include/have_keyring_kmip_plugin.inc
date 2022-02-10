#
# Check if the variable KEYRING_KMIP_PLUGIN is set
#
if (!$KEYRING_KMIP_PLUGIN) {
  --skip keyring_kmip not available.
}
