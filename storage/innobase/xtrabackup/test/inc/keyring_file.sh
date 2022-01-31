#
# keyring_file helpers
# Read KEYRING_TYPE[component|plugin] to decide which keyring type to setup
# If KEYRING_TYPE is not set, we assume plugin
# TODO: check component install only on version higer than 8.0.24

. inc/common.sh

if [ -z ${KEYRING_TYPE+x} ]; then
  KEYRING_TYPE="plugin"
fi

if test -d $PWD/../../../../plugin_output_directory
then
  plugin_dir=$PWD/../../../../plugin_output_directory
else
  plugin_dir=$PWD/../../lib/plugin/
fi
keyring_file=${TEST_VAR_ROOT}/keyring_file
keyring_args="--keyring-file-data=${keyring_file}"
XB_EXTRA_MY_CNF_OPTS="${XB_EXTRA_MY_CNF_OPTS:-""}
xtrabackup-plugin-dir=${plugin_dir}
"

if [[ "${KEYRING_TYPE}" = "plugin" ]] || [[ "${KEYRING_TYPE}" = "both" ]]; then
  plugin_load=keyring_file.so
  MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS:-""}
early-plugin-load=${plugin_load}
keyring-file-data=${keyring_file}
"
fi

function configure_keyring_file_component()
{
  if [ "$MYSQL_DEBUG_MODE" = "on" ]; then
    binary="mysqld-debug"
  else
    binary="mysqld"
  fi
  cat <<EOF > "${MYSQLD_DATADIR}/${binary}.my"
{
    "components": "file://component_keyring_file"
}
EOF
  cat <<EOF > "${MYSQLD_DATADIR}/component_keyring_file.cnf"
{
  "path": "${keyring_file}",
  "read_only": false
}
EOF
}

function cleanup_keyring() {
	rm -rf $keyring_file
}
