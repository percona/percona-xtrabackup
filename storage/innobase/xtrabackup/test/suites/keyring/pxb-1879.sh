#
# PXB-1879: xtrabackup does not take FS block size into account for compressed and encrypted tables
#

require_server_version_higher_than 5.7.10

. inc/keyring_file.sh

start_server

load_sakila

mysql -e "ALTER TABLE payment ENCRYPTION='y' COMPRESSION='lz4'" sakila

innodb_wait_for_flush_all

# this will crash with bug present
xtrabackup --backup --target-dir=$topdir/backup --xtrabackup-plugin-dir=$plugin_dir
