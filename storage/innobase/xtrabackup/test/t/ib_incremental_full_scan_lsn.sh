# Test incremental full scan backups with the --incremental-lsn option

ib_inc_extra_args=
ib_inc_use_lsn=1

. inc/ib_incremental_common.sh

check_full_scan_inc_backup
