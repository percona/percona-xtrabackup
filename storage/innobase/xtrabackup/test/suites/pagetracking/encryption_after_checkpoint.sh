############################################################################
#PXB-2681 PXB crashes with alter encryption='n' happens after the checkpoint
############################################################################

. inc/common.sh
KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

require_debug_server

vlog "take incremental backup"
mysql test <<EOF
CREATE TABLE t1 (i serial) KEY_BLOCK_SIZE=2 ENCRYPTION='Y';
CREATE TABLESPACE tab02k_e ADD DATAFILE 'tab02k_e.ibd'  FILE_BLOCK_SIZE 02k ENCRYPTION='Y';
CREATE TABLE t2 (i serial) KEY_BLOCK_SIZE=2 TABLESPACE tab02k_e ENCRYPTION='Y';
INSERT INTO t2 VALUES(null);
CREATE TABLE t3 (i serial) KEY_BLOCK_SIZE=2 ENCRYPTION='Y';
INSTALL COMPONENT "file://component_mysqlbackup";
SELECT mysqlbackup_page_track_set(1);
SET GLOBAL innodb_log_checkpoint_now = 1;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
EOF

xtrabackup --backup --target-dir=$topdir/backup --page-tracking  --transition-key=123

vlog "run alter table"

mysql test <<EOF
ALTER TABLE t1 ENCRYPTION 'N';
ALTER TABLESPACE tab02k_e ENCRYPTION 'N';
ALTER TABLE t3 ENCRYPTION 'N';
EOF

vlog "take incremental backup"
xtrabackup --backup --target-dir=$topdir/inc --page-tracking --incremental-basedir=$topdir/backup --transition-key=123

record_db_state test

vlog "restore"
stop_server
rm -r $mysql_datadir

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup --transition-key=123

xtrabackup --prepare --target-dir=$topdir/backup --incremental-dir=$topdir/inc --transition-key=123

vlog "copy back"
xtrabackup --copy-back --target-dir=$topdir/backup --transition-key=123 \
               --generate-new-master-key --datadir=$mysql_datadir --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

start_server

vlog "validate tablespace and table data tablespace"
verify_db_state test
mysql test <<EOF
CREATE TABLE t4 (i serial) KEY_BLOCK_SIZE=2 TABLESPACE tab02k_e;
INSERT INTO t4 VALUES(null);
INSERT INTO t2 VALUES(null);
INSERT INTO t3 VALUES(null);
EOF
