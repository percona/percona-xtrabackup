#
# PXB-3864: Support component_percona_keyring_encrypted_file
#
. inc/keyring_common.sh
. inc/keyring_encrypted_file.sh

require_keyring_encrypted_file_component

configure_server_with_component

vlog "-- Starting main test with ${KEYRING_TYPE} --"
test_do "ENCRYPTION='y'" "top-secret" ${KEYRING_TYPE}
test_do "ENCRYPTION='y' ROW_FORMAT=COMPRESSED" "none"  ${KEYRING_TYPE}
test_do "ENCRYPTION='y' COMPRESSION='lz4'" "none"  ${KEYRING_TYPE}
keyring_extra_tests
vlog "-- Finished main test with ${KEYRING_TYPE} --"

vlog "-- Starting test with password_file --"
KEYRING_ENCRYPTED_FILE_PASSWORD_MODE="password_file"
XB_EXTRA_MY_CNF_OPTS="
xtrabackup-plugin-dir=${plugin_dir}
"
configure_server_with_component

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (c1 VARCHAR(100)) ENCRYPTION='y';
INSERT INTO t1 (c1) VALUES ('ONE'), ('TWO'), ('THREE');
ALTER INSTANCE ROTATE INNODB MASTER KEY;
INSERT INTO t1 SELECT * FROM t1;
EOF

xtrabackup --backup --target-dir=$topdir/backup

# keyring data file must not contain plain JSON
if grep -q "data_id" ${keyring_encrypted_file}; then
  die "Keyring file ${keyring_encrypted_file} is not encrypted"
fi

record_db_state test
stop_server

vlog "-- Prepare with a wrong password must fail --"
wrong_password_cnf=${TEST_VAR_ROOT}/wrong_password/component_percona_keyring_encrypted_file.cnf
mkdir -p $(dirname ${wrong_password_cnf})
cat <<EOF > ${wrong_password_cnf}
{
  "path": "${keyring_encrypted_file}",
  "read_only": true,
  "password": "wrong_password"
}
EOF
cp -R $topdir/backup $topdir/backup_wrong_password
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare \
  --target-dir=$topdir/backup_wrong_password \
  --xtrabackup-plugin-dir=${plugin_dir} \
  --component-keyring-config=${wrong_password_cnf}
if ! grep -q "Failed to decrypt keyring file" $OUTFILE
then
  die "Prepare with a wrong password did not report decryption failure"
fi

xtrabackup --prepare --target-dir=$topdir/backup ${keyring_args}

rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup
cp ${instance_local_manifest} $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir
start_server
verify_db_state test
stop_server

rm -rf $mysql_datadir $topdir/{backup,backup_wrong_password} \
  ${TEST_VAR_ROOT}/wrong_password ${keyring_encrypted_file_password_file}
cleanup_keyring
vlog "-- Finished test with password_file --"
