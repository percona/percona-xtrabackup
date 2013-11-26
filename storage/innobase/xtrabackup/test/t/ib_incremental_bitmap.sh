# Test for incremental backups that use changed page bitmaps

require_xtradb

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-track-changed-pages=TRUE
"
ib_inc_extra_args=

. inc/ib_incremental_common.sh

check_bitmap_inc_backup
