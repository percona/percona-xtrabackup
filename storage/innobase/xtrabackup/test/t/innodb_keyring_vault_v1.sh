#
# Basic test of InnoDB encryption support using vault version v1 -> v1
#
. inc/keyring_common.sh
. inc/keyring_vault.sh

keyring_vault_ping || skip_test "Keyring vault server is not avaliable"

keyring_vault_mount "1" "1"

function cleanup_keyring() {
	keyring_vault_remove_all_keys
}

trap "keyring_vault_unmount" EXIT

test_do "ENCRYPTION='y'" "" "top-secret"
test_do "ENCRYPTION='y'" "ROW_FORMAT=COMPRESSED" "none"
test_do "ENCRYPTION='y'" "ROW_FORMAT=COMPRESSED" "top-secret"
test_do "ENCRYPTION='y'" "COMPRESSION='zlib'" "generate"

keyring_vault_unmount
trap "" EXIT

# cleanup environment variables
MYSQLD_EXTRA_MY_CNF_OPTS=
XB_EXTRA_MY_CNF_OPTS=
