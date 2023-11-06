
# PXB-1954: PXB gets stuck in prepare of full backup for encrypted tablespace
#

. inc/keyring_file.sh
. inc/dictionary_common.sh

start_server

mysql test <<EOF
CREATE TABLE t (a INT) ENCRYPTION='y';
EOF

mysql -e "INSERT INTO t (a) VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9)" test

vlog "Long running transaction that modifies table data but is not committed at the time of backup"

$MYSQL $MYSQL_ARGS -e "\
BEGIN;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
INSERT INTO t SELECT * FROM t;\
SELECT SLEEP (10000);
" test &
uncommitted_trx=$!

trap "kill_bg_trx $uncommitted_trx" EXIT

wait_for_bg_trx

vlog "Backup"
xtrabackup --backup --target-dir=$topdir/backup \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

kill_bg_trx $uncommitted_trx

vlog "Record db state"
record_db_state test

vlog "Prepare the backup without keyring. xtrabackup should fail"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --target-dir=$topdir/backup > $topdir/pxb_prepare_fail.log 2>&1

egrep -q "Encryption information in datafile: ./test/t.ibd can't be decrypted, please confirm that keyring is loaded" $topdir/pxb_prepare_fail.log || die "xtrabackup prepare didn't fail without keyring"

vlog "Prepare the backup with keyring"
xtrabackup --prepare --target-dir=$topdir/backup \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} 2>&1 | tee $topdir/pxb_prepare.log

check_rollback_msg $topdir/pxb_prepare.log

vlog "Restore"
stop_server
rm -rf $MYSQLD_DATADIR/*
xtrabackup --move-back --target-dir=${topdir}/backup
start_server
verify_db_state test

vlog "Test 2: No rollback on encrypted table. Lack of keyring shouldn't fail the prepare"

vlog "Backup"
rm -rf $topdir/backup
xtrabackup --backup --target-dir=$topdir/backup \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

vlog "Record db state"
record_db_state test

vlog "Prepare the backup without keyring. xtrabackup should NOT fail.No redo or undo on encrypted table"
vlog "We dont pass ${plugin_dir} and ${keyring_args} intentionally here"
xtrabackup --prepare --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb_prepare2.log

vlog "Restore"
stop_server
rm -rf $MYSQLD_DATADIR/*
xtrabackup --move-back --target-dir=${topdir}/backup
start_server
verify_db_state test
rm $topdir/pxb_prepare.log
rm $topdir/pxb_prepare2.log
rm $topdir/pxb_prepare_fail.log
