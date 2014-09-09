################################################################################
# Bug #1367377: xtrabackup should reject unknown arguments which are not options
################################################################################

start_server

run_cmd_expect_failure $XB_BIN $XB_ARGS --backup foo

test -d xtrabackup_backupfiles && die "Default backup directory found" || true

run_cmd_expect_failure $XB_BIN $XB_ARGS foo --backup

test -d xtrabackup_backupfiles && die "Default backup directory found" || true
