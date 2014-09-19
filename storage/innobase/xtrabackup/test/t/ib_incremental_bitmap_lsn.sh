# Test for fast incremental backups with the --incremental-lsn option

require_xtradb
is_64bit || skip_test "Disabled on 32-bit hosts due to LP bug #1359182"

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-track-changed-pages=TRUE
"
ib_inc_extra_args=
ib_inc_use_lsn=1

. inc/ib_incremental_common.sh

check_bitmap_inc_backup
