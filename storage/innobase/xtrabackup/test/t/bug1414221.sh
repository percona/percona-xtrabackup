############################################################################
# Bug 1414221: Warning "The log was not applied to the intended LSN"
#              should optionally be an error
############################################################################
. inc/common.sh

start_server --innodb_file_per_table
load_sakila

${MYSQL} ${MYSQL_ARGS} sakila -e "DELETE FROM payment"

# Full backup
# backup root directory
vlog "Starting backup"

full_backup_dir=$topdir/full_backup
xtrabackup --backup --target-dir=$full_backup_dir

ls -al $full_backup_dir/xtrabackup_logfile

sed -i -e 's/to_lsn = [0-9]*$/to_lsn = 999999999/' \
	$full_backup_dir/xtrabackup_checkpoints

vlog "Preparing backup"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --target-dir=$full_backup_dir
vlog "Log applied to full backup"

rm -rf ${full_backup_dir}

############################################################################
# After 8.0.30 running a backup against pre-8.0.30 will skip last block
# as we are writting LOG_BLOCK_EPOCH_NO instead of LOG_BLOCK_CHECKPOINT_NO
# If checkpoint falls within the last block, pxb is incorrectly skipping
# the block
############################################################################

innodb_wait_for_flush_all
${MYSQL} ${MYSQL_ARGS} test -e "CREATE TABLE t (a INT) engine=InnoDB"
${MYSQL} ${MYSQL_ARGS} test -e "INSERT INTO t (a) VALUES (1), (2), (3)"
xtrabackup --backup --throttle=40 --target-dir=$full_backup_dir
xtrabackup --prepare --apply-log-only --target-dir=$full_backup_dir
