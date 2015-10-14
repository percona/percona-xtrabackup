############################################################################
# Bug 1414221: Warning "The log was not applied to the intended LSN"
#              should optionally be an error
############################################################################
. inc/common.sh

if [ ${ASAN_OPTIONS:-undefined} != "undefined" ]
then
    skip_test "Incompatible with AddressSanitizer"
fi

start_server --innodb_file_per_table
load_sakila

${MYSQL} ${MYSQL_ARGS} sakila -e "DELETE FROM payment"

# Full backup
# backup root directory
vlog "Starting backup"

full_backup_dir=$topdir/full_backup
innobackupex --no-timestamp $full_backup_dir

ls -al $full_backup_dir/xtrabackup_logfile

sed -i -e 's/to_lsn = [0-9]*$/to_lsn = 999999999/' \
	$full_backup_dir/xtrabackup_checkpoints

vlog "Preparing backup"
run_cmd_expect_failure $IB_BIN $IB_ARGS  --apply-log \
	$full_backup_dir
vlog "Log applied to full backup"
