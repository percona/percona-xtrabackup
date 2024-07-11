#
# keyring_file helpers
# Read KEYRING_TYPE[component|plugin] to decide which keyring type to setup
# If KEYRING_TYPE is not set, we assume plugin
# TODO: check component install only on version higer than 8.0.24

. inc/common.sh

if [ -z ${KEYRING_TYPE+x} ]; then
  KEYRING_TYPE="component"
fi

if test -d $PWD/../../../../plugin_output_directory
then
  plugin_dir=$PWD/../../../../plugin_output_directory
else
  plugin_dir=$PWD/../../lib/plugin/
fi
keyring_file_plugin=${TEST_VAR_ROOT}/keyring_file_plugin
keyring_file_component=${TEST_VAR_ROOT}/keyring_file_component
XB_EXTRA_MY_CNF_OPTS="${XB_EXTRA_MY_CNF_OPTS:-""}
xtrabackup-plugin-dir=${plugin_dir}
"

if [[ "${KEYRING_TYPE}" = "plugin" ]] || [[ "${KEYRING_TYPE}" = "both" ]]; then
  plugin_load=keyring_file.so
  MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS:-""}
early-plugin-load=${plugin_load}
keyring-file-data=${keyring_file_plugin}
"
fi

instance_local_manifest=""
keyring_component_cnf=${TEST_VAR_ROOT}/component_keyring_file.cnf

if [[ "${KEYRING_TYPE}" = "plugin" ]]; then
  keyring_args="--keyring-file-data=${keyring_file_plugin}"
else
  keyring_args="--component-keyring-config=${keyring_component_cnf}"
fi

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
    "components": "file://component_keyring_file"
}
EOF
  cat <<EOF > "${MYSQLD_DATADIR}/component_keyring_file.cnf"
{
  "path": "${keyring_file_component}",
  "read_only": false
}
EOF
cp "${MYSQLD_DATADIR}/${binary}.my" ${instance_local_manifest}
cp "${MYSQLD_DATADIR}/component_keyring_file.cnf" ${keyring_component_cnf}
}

function cleanup_keyring() {
  rm -rf $keyring_file_plugin
  rm -rf $keyring_file_component
}
