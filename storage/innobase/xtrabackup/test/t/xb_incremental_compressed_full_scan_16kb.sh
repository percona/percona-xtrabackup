# Test incremental backups that do full data scans with 16KB compressed pages

first_inc_suspend_command=

source t/xb_incremental_compressed.inc

test_incremental_compressed 16

check_full_scan_inc_backup
