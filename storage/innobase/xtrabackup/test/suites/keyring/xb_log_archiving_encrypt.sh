#
# Basic test for using redo log archiving for backups (encrypted)
#

require_server_version_higher_than 8.0.16


MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt
"

. inc/keyring_file.sh

start_server

. inc/xb_log_archiving.sh
