#
# PXB-1793: Full backup fails when PS is initialized and
#           started with encryption options
#

require_xtradb

MYSQLD_EXTRA_MY_CNF_OPTS="
binlog-encryption
innodb-undo-log-encrypt
innodb-redo-log-encrypt
innodb_encrypt_tables=ON
innodb_encrypt_online_alter_logs=ON
innodb_temp_tablespace_encrypt=ON
encrypt-tmp-files
"

. inc/keyring_file.sh

start_server

# backup fails if bug is present
xtrabackup --backup --target-dir=$topdir/backup
