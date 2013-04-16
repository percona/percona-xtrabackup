# Test incremental backups that do full data file scans

mysqld_extra_args=
ib_inc_extra_args=

. inc/ib_incremental_common.sh

check_full_scan_inc_backup
