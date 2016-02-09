########################################################################
# Bug #1533542: innobackupex silently skip extra arguments
########################################################################

. inc/common.sh

start_server

# Full backup
vlog "Starting backup"

# Invoke innobackupex with an extra argument 'tmp-dir'
run_cmd_expect_failure $IB_BIN $IB_ARGS --no-timestamp $topdir/backup tmp-dir

# Should failed with error
grep -q "Error: extra argument found tmp-dir" $OUTFILE
