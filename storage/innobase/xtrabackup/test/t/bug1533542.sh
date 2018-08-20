########################################################################
# Bug #1533542: innobackupex silently skip extra arguments
########################################################################

. inc/common.sh

start_server

# Full backup
vlog "Starting backup"

# Invoke xtrabackup with an extra argument 'tmp-dir'
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --target-dir=$topdir/backup tmp-dir

# Should failed with error
grep -q "xtrabackup: Error: unknown argument: 'tmp-dir'" $OUTFILE
