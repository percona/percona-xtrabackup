#############################################################################
# Bug #977652: Compressed/Uncompressed incremental backup fails on compressed
#              full backup in Xtrabackup 2.0.0
#############################################################################

require_qpress

. inc/common.sh

start_server --innodb_file_per_table

load_sakila

# Take a full compressed backup
innobackupex --compress --no-timestamp $topdir/full

# Test that incremental backups work without uncompressing the full one
innobackupex --compress --no-timestamp --incremental \
    --incremental-basedir=$topdir/full $topdir/incremental
