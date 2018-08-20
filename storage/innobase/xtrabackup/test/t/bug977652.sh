#############################################################################
# Bug #977652: Compressed/Uncompressed incremental backup fails on compressed
#              full backup in Xtrabackup 2.0.0
#############################################################################

require_qpress

. inc/common.sh

start_server --innodb_file_per_table

load_sakila

# Take a full compressed backup
xtrabackup --backup --compress --target-dir=$topdir/full

# Test that incremental backups work without uncompressing the full one
xtrabackup --backup --compress \
    --incremental-basedir=$topdir/full --target-dir=$topdir/incremental
