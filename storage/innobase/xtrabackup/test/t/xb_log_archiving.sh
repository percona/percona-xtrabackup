#
# Basic test for using redo log archiving for backups
#

require_server_version_higher_than 8.0.16

if [ ${ASAN_OPTIONS:-undefined} != "undefined" ]
then
    skip_test "Incompatible with AddressSanitizer"
fi

. inc/keyring_file.sh

start_server

. inc/xb_log_archiving.sh
