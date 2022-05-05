#
# Component keyring file specific variables
#
. inc/keyring_common.sh
. inc/keyring_kmip.sh
ping_kmip || skip_test "Keyring kmip requires KMIP Server configured"
is_xtradb || skip_test "Keyring kmip requires Percona Server"
require_server_version_higher_than 8.0.26

KEYRING_TYPE="component"
configure_server_with_component

vlog "-- Starting main test with ${KEYRING_TYPE} --"
test_do "ENCRYPTION='y'" "top-secret" ${KEYRING_TYPE}
test_do "ENCRYPTION='y' ROW_FORMAT=COMPRESSED" "none"  ${KEYRING_TYPE}
test_do "ENCRYPTION='y' COMPRESSION='lz4'" "none"  ${KEYRING_TYPE}
keyring_extra_tests

vlog "-- Finished main test with ${KEYRING_TYPE} --"
