#
# Basic test for using redo log archiving for backups (encrypted)
#

require_server_version_higher_than 8.0.16


MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt
"

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

. inc/xb_log_archiving.sh
