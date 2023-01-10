############################################################################
# PXB-1961: xtrabackup fails with encryption='n' with Percona server
############################################################################

. inc/common.sh
. inc/keyring_file.sh

require_debug_server
require_server_version_higher_than 5.7.0

start_server

vlog "case#1 restore table created with encryption='n'"
mysql test <<EOF
SET GLOBAL innodb_log_checkpoint_now = 1;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_checkpoint_disabled = 1;
CREATE TABLE t1(a INT) ENGINE=INNODB ENCRYPTION='N';
INSERT INTO t1 VALUES(100);
EOF
run_cmd backup_and_restore test

vlog "case#2 check if table can be altered with encryption='n'"
mysql test <<EOF
SET GLOBAL innodb_checkpoint_disabled = 0;
SET GLOBAL innodb_page_cleaner_disabled_debug = 0;
SET GLOBAL innodb_log_checkpoint_now = 1;
ALTER TABLE t1 ENCRYPTION='Y';
ALTER TABLE t1 ENCRYPTION='N';
INSERT INTO t1 VALUES(100);
EOF
innodb_wait_for_flush_all
run_cmd backup_and_restore test
