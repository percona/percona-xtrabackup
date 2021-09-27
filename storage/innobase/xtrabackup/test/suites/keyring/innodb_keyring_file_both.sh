#
# Basic test of InnoDB encryption support
#
. inc/keyring_common.sh
require_server_version_higher_than 8.0.23
KEYRING_TYPE=("both")

. inc/keyring_file.sh
vlog "-- Starting main test with ${KEYRING_TYPE} --"
test_do "ENCRYPTION='y'" "top-secret" ${KEYRING_TYPE}
test_do "ENCRYPTION='y' ROW_FORMAT=COMPRESSED" "none"  ${KEYRING_TYPE}
test_do "ENCRYPTION='y' COMPRESSION='lz4'" "none"  ${KEYRING_TYPE}
keyring_extra_tests

vlog "-- Finished main test with ${KEYRING_TYPE} --"
