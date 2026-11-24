#
# component_percona_keyring_encrypted_file helpers
# Read KEYRING_ENCRYPTED_FILE_PASSWORD_MODE[password|password_file] to decide
# how the keyring password is passed to the component.
# If KEYRING_ENCRYPTED_FILE_PASSWORD_MODE is not set, we assume password

. inc/common.sh

KEYRING_TYPE="component"

if [ -z ${KEYRING_ENCRYPTED_FILE_PASSWORD_MODE+x} ]; then
  KEYRING_ENCRYPTED_FILE_PASSWORD_MODE="password"
fi

if test -d $PWD/../../../../plugin_output_directory
then
  plugin_dir=$PWD/../../../../plugin_output_directory
else
  plugin_dir=$PWD/../../lib/plugin/
fi
XB_EXTRA_MY_CNF_OPTS="${XB_EXTRA_MY_CNF_OPTS:-""}
xtrabackup-plugin-dir=${plugin_dir}
"

instance_local_manifest=""
keyring_encrypted_file=${TEST_VAR_ROOT}/keyring_encrypted_file
keyring_encrypted_file_password="pxb_secret_password"
keyring_encrypted_file_password_file=${TEST_VAR_ROOT}/keyring_encrypted_file_password
keyring_component_cnf=${TEST_VAR_ROOT}/component_percona_keyring_encrypted_file.cnf
keyring_args="--component-keyring-config=${keyring_component_cnf}"

function require_keyring_encrypted_file_component()
{
  is_xtradb || skip_test "Keyring encrypted file requires Percona Server"
  if [ ! -f "${MYSQL_BASEDIR}/lib/plugin/component_percona_keyring_encrypted_file.so" ]; then
    skip_test "Server does not provide component_percona_keyring_encrypted_file"
  fi
}

function configure_keyring_file_component()
{
  if [ "$MYSQL_DEBUG_MODE" = "on" ]; then
    binary="mysqld-debug"
  else
    binary="mysqld"
  fi
  instance_local_manifest="${TEST_VAR_ROOT}/${binary}.my"
  cat <<EOF > "${MYSQLD_DATADIR}/${binary}.my"
{
    "components": "file://component_percona_keyring_encrypted_file"
}
EOF
  if [[ "${KEYRING_ENCRYPTED_FILE_PASSWORD_MODE}" = "password_file" ]]; then
    printf '%s' "${keyring_encrypted_file_password}" > ${keyring_encrypted_file_password_file}
    password_option="\"password_file\": \"${keyring_encrypted_file_password_file}\""
  else
    password_option="\"password\": \"${keyring_encrypted_file_password}\""
  fi
  cat <<EOF > "${MYSQLD_DATADIR}/component_percona_keyring_encrypted_file.cnf"
{
  "path": "${keyring_encrypted_file}",
  "read_only": false,
  ${password_option},
  "iterations": 1000
}
EOF
cp "${MYSQLD_DATADIR}/${binary}.my" ${instance_local_manifest}
cp "${MYSQLD_DATADIR}/component_percona_keyring_encrypted_file.cnf" ${keyring_component_cnf}
}

function cleanup_keyring() {
  rm -rf $keyring_encrypted_file
}
