########################################################################
# Bug #1706582: Make --lock-ddl and --lock-ddl-per-table mutually exclusive
########################################################################

start_server

vlog "Full backup"

run_cmd_expect_failure xtrabackup --backup --lock-ddl --lock-ddl-per-table \
    --target-dir=$topdir/data/full  2>&1 | \
        grep "Error: --lock-ddl and --lock-ddl-per-table are mutually exclusive"

