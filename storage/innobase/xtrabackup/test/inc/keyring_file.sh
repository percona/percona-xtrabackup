#
# keyring_file helpers
#

. inc/common.sh

plugin_load=keyring_file.so
if test -d ${XTRABACKUP_BASEDIR}/keyring
then
  plugin_dir=${XTRABACKUP_BASEDIR}/keyring
else
  plugin_dir=${XTRABACKUP_BASEDIR}/lib/plugin
fi

keyring_file=${TEST_VAR_ROOT}/keyring_file
keyring_args="--keyring-file-data=${keyring_file}"

MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS:-""}
early-plugin-load=${plugin_load}
keyring-file-data=${keyring_file}
"

XB_EXTRA_MY_CNF_OPTS="${XB_EXTRA_MY_CNF_OPTS:-""}
xtrabackup-plugin-dir=${plugin_dir}
"
