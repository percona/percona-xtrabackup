#
# Component keyring file specific variables
#
require_server_version_higher_than 8.0.23

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (c1 VARCHAR(100)) ENCRYPTION='y';
INSERT INTO t1 (c1) VALUES ('ONE'), ('TWO'), ('THREE');
INSERT INTO t1 (c1) VALUES ('10'), ('20'), ('30');
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
CREATE TABLE t2 (c1 VARCHAR(100)) ENCRYPTION='y';
INSERT INTO t2 SELECT * FROM t1;
EOF

xtrabackup --backup --target-dir=$topdir/backup1 \
--component-keyring-file-config=${MYSQLD_DATADIR}/component_keyring_file.cnf 2>&1 | tee $topdir/pxb.log
grep_count "xtrabackup: Warning: \-\-component-keyring-file-config will be ignored for \-\-backup operation" $topdir/pxb.log 1

cp -r $topdir/backup1 $topdir/backup2
record_db_state test
shutdown_server

xtrabackup --stats --datadir=${mysql_datadir} \
  --component-keyring-file-config=${MYSQLD_DATADIR}/component_keyring_file.cnf \
  --xtrabackup-plugin-dir=${plugin_dir} 2>&1 | tee $topdir/pxb.log

grep_count "Encryption can't find master key, please check the keyring is loaded." $topdir/pxb.log 0

# Test 1: Pass non existing configuration file:
run_cmd_expect_failure $XB_BIN $XB_ARGS \
--prepare --target-dir=$topdir/backup1 --xtrabackup-plugin-dir=${plugin_dir} \
--component-keyring-file-config=$topdir/xtrabackup_component_keyring_file_invalid.cnf 2>&1 | tee $topdir/pxb.log

grep_count "Component configuration file is not readable or not found" $topdir/pxb.log 1

# Test 2: Pass non existing configuration file:
touch $topdir/xtrabackup_component_keyring_file_invalid.cnf
run_cmd_expect_failure $XB_BIN $XB_ARGS \
--prepare --target-dir=$topdir/backup1 --xtrabackup-plugin-dir=${plugin_dir} \
--component-keyring-file-config=$topdir/xtrabackup_component_keyring_file_invalid.cnf 2>&1 | tee $topdir/pxb.log

grep_count "Component configuration file is empty" $topdir/pxb.log 1

# Test 3: Try to prepare with rubbish on component config on target-dir
cp $topdir/backup1/xtrabackup_component_keyring_file.cnf $topdir
echo "bla" > $topdir/backup1/xtrabackup_component_keyring_file.cnf
run_cmd_expect_failure $XB_BIN $XB_ARGS \
--prepare --target-dir=$topdir/backup1 --xtrabackup-plugin-dir=${plugin_dir} 2>&1 | tee $topdir/pxb.log

grep_count "Component configuration file is not a valid JSON" $topdir/pxb.log 1

# Test 4: Pass valid JSON config file without path member
echo "{}" > $topdir/xtrabackup_component_keyring_file_invalid.cnf
run_cmd_expect_failure $XB_BIN $XB_ARGS \
--prepare --target-dir=$topdir/backup1 --xtrabackup-plugin-dir=${plugin_dir} \
--component-keyring-file-config=$topdir/xtrabackup_component_keyring_file_invalid.cnf 2>&1 | tee $topdir/pxb.log

grep_count "Component configuration does not have path member" $topdir/pxb.log 1

# Test 5: Try to prepare with good component config on target-dir but
# pass rubbish as parameter
run_cmd_expect_failure $XB_BIN $XB_ARGS \
--prepare --target-dir=$topdir/backup2 \
--component-keyring-file-config=$topdir/backup1/xtrabackup_component_keyring_file.cnf \
--xtrabackup-plugin-dir=${plugin_dir} 2>&1 | tee $topdir/pxb.log
grep_count "Component configuration file is not a valid JSON" $topdir/pxb.log 1

# Test 6: Try to prepare with rubbish on component config on target-dir but
# pass good config as parameter
xtrabackup --prepare --target-dir=$topdir/backup1 \
--component-keyring-file-config=$topdir/xtrabackup_component_keyring_file.cnf \
--xtrabackup-plugin-dir=${plugin_dir} 2>&1 | tee $topdir/pxb.log

rm -rf ${mysql_datadir}
xtrabackup --move-back --target-dir=$topdir/backup1

start_server
run_cmd verify_db_state test

stop_server
rm -rf $topdir
vlog "-- Starting main test with ${KEYRING_TYPE} --"
test_do "ENCRYPTION='y'" "top-secret" ${KEYRING_TYPE}
test_do "ENCRYPTION='y' ROW_FORMAT=COMPRESSED" "none"  ${KEYRING_TYPE}
test_do "ENCRYPTION='y' COMPRESSION='lz4'" "none"  ${KEYRING_TYPE}
keyring_extra_tests

vlog "-- Finished main test with ${KEYRING_TYPE} --"
