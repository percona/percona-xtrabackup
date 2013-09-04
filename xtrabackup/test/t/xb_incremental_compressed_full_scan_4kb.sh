# Test incremental backups that do full data scans with 4KB compressed pages

first_inc_suspend_command=

source t/xb_incremental_compressed.inc

test_incremental_compressed 4

check_full_scan_inc_backup
