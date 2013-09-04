# Test incremental backups that use bitmaps with 16KB compressed pages

require_xtradb

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-track-changed-pages=TRUE
"
first_inc_suspend_command="FLUSH CHANGED_PAGE_BITMAPS;"

source t/xb_incremental_compressed.inc

test_incremental_compressed 16

check_bitmap_inc_backup
