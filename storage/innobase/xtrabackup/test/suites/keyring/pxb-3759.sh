#
# PXB-3759: xtrabackup crashes with SIGSEGV at end of backup when server-side
# redo log encryption is enabled and innodb_log_buffer_size > 32 MiB.
#
# Root cause: Redo_Log_Writer::scratch_buf is hardcoded to 16 MiB while the
# source buffer scales with innodb_log_buffer_size / 2. When the log copy
# thread reads more than 16 MiB of redo and the encrypt branch is taken,
# Encryption::encrypt_log writes past scratch_buf, clobbering adjacent heap
# allocations. The crash typically surfaces during destructor cleanup of
# Redo_Log_Reader's aligned buffers whose PFS metadata was overwritten.
#
# Trigger: keyring + innodb_redo_log_encrypt=ON + innodb_log_buffer_size=64M
# plus enough pending redo at backup time to push a single read_logfile()
# call above 16 MiB.
#

require_debug_pxb_version

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh

# keyring_common.sh sets MYSQLD_EXTRA_MY_CNF_OPTS at source time, overwriting
# anything set above. Append our options after sourcing so they survive.
MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS}
innodb_log_buffer_size=64M
innodb_redo_log_capacity=256M
"

configure_server_with_component

mysql -e "CREATE TABLE pxb_3759 (
  id INT PRIMARY KEY AUTO_INCREMENT,
  payload LONGBLOB
) ENGINE=InnoDB" test

mysql test <<'EOF'
DROP PROCEDURE IF EXISTS pxb_3759_load;
DELIMITER //
CREATE PROCEDURE pxb_3759_load(IN n INT)
BEGIN
  DECLARE i INT DEFAULT 0;
  WHILE i < n DO
    INSERT INTO pxb_3759 (payload) VALUES (REPEAT('a', 1024 * 1024));
    SET i = i + 1;
  END WHILE;
END//
DELIMITER ;
EOF

innodb_wait_for_flush_all

mkdir $topdir/backup

vlog "starting backup paused after redo catchup"
xtrabackup --backup --target-dir=$topdir/backup \
           --debug-sync="xtrabackup_pause_after_redo_catchup" \
	   --register-redo-log-consumer \
           2> >(tee $topdir/backup.log) &
job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`

vlog "piling up >16 MiB of redo while xtrabackup is paused"
mysql -e "CALL pxb_3759_load(64)" test

vlog "resuming xtrabackup"
kill -SIGCONT $xb_pid

run_cmd wait $job_pid

if grep -qE "signal 11|SIGSEGV|signal 6|SIGABRT|Assertion failure" $topdir/backup.log ; then
    die "xtrabackup crashed (PXB-3759 reproduced)"
fi

record_db_state test

xtrabackup --prepare --target-dir=$topdir/backup \
  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

cp ${instance_local_manifest} $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

start_server

verify_db_state test
