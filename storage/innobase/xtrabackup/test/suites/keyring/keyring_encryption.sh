############################################################################
# PXB-1961: xtrabackup fails with encryption='n' with Percona server
############################################################################

. inc/common.sh
. inc/keyring_file.sh

require_xtradb
require_debug_server
require_server_version_higher_than 5.7.0
require_server_version_lower_than 8.0.30

start_server

mysql -e "CREATE TABLE t1(a INT) ENGINE=INNODB ENCRYPTION='N'" test

vlog "case#1 try to restore encryption='keyring'"
mysql test <<EOF
SET GLOBAL innodb_log_checkpoint_now = 1;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_checkpoint_disabled = 1;
CREATE TABLE t2(a INT) ENGINE=INNODB ENCRYPTION='keyring';
INSERT INTO t2 VALUES(100);
EOF
rm -rf $topdir/backup
mkdir -p $topdir/backup
run_cmd_expect_failure xtrabackup --backup --target-dir=$topdir/backup --xtrabackup-plugin-dir=${plugin_dir}
stop_server
