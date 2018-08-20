########################################################################
# Bug #729843: innobackupex logs plaintext password
########################################################################

. inc/common.sh

start_server

mkdir $topdir/backup
logfile=$topdir/backup/xtrabackup_log

# Don't use run_cmd_* or xtrabackup functions here to avoid logging
# the full command line (including the password in plaintext)
set +e
$XB_BIN $XB_ARGS --backup --password=secretpassword --target-dir=$topdir/backup 2>&1 | tee $logfile
set -e

# Check that the password was not logged in plaintext
run_cmd_expect_failure grep -- "secretpassword" $logfile
