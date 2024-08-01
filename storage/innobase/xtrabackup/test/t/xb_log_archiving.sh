#
# Basic test for using redo log archiving for backups
#

require_server_version_higher_than 8.0.16

. inc/keyring_file.sh

# initialize db without keyring and extra config. In case we have options like
# encrypt_redo_log we cannot run --initialize with local component config
# present at datadir.
start_server
stop_server
#enable keyring component
configure_keyring_file_component
start_server
innodb_wait_for_flush_all

. inc/xb_log_archiving.sh
