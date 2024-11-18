#
# PXB-1793: Full backup fails when PS is initialized and
#           started with encryption options
#

require_xtradb

MYSQLD_EXTRA_MY_CNF_OPTS="
binlog-encryption
innodb-undo-log-encrypt
innodb-redo-log-encrypt
default_table_encryption=on
innodb_encrypt_online_alter_logs=ON
innodb_temp_tablespace_encrypt=ON
encrypt-tmp-files
"

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

# backup fails if bug is present
xtrabackup --backup --target-dir=$topdir/backup
