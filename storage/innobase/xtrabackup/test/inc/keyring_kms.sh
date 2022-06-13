#
# keyring_kmip helpers
# Read KEYRING_TYPE[component|plugin] to decide which keyring type to setup
# If KEYRING_TYPE is not set, we assume component

. inc/common.sh

if [ -z ${KEYRING_TYPE+x} ]; then
  KEYRING_TYPE="component"
fi

keyring_component_cnf=${TEST_VAR_ROOT}/component_keyring_kms.cnf
keyring_args="--component-keyring-config=${keyring_component_cnf}"


KMS_REGION="${KMS_REGION:-us-east-1}"
KMS_AUTH_KEY="${KMS_AUTH_KEY:-}"
KMS_SECRET_ACCESS_KEY="${KMS_SECRET_ACCESS_KEY:-}"
KMS_KEY="${KMS_KEY:-}"

keyring_file=${TEST_VAR_ROOT}/keyring_kms_file

function configure_keyring_file_component()
{
  if [ "$MYSQL_DEBUG_MODE" = "on" ]; then
    binary="mysqld-debug"
  else
    binary="mysqld"
  fi
  cat <<EOF > "${MYSQLD_DATADIR}/${binary}.my"
{
    "components": "file://component_keyring_kms"
}
EOF
  cat <<EOF > "${MYSQLD_DATADIR}/component_keyring_kms.cnf"
{
  "path": "${keyring_file}",
  "read_only": false,
  "region": "${KMS_REGION}",
  "auth_key": "${KMS_AUTH_KEY}",
  "secret_access_key": "${KMS_SECRET_ACCESS_KEY}",
  "kms_key": "${KMS_KEY}"
}
EOF
cp "${MYSQLD_DATADIR}/component_keyring_kms.cnf" ${keyring_component_cnf}
}

function cleanup_keyring() {
  rm -rf $keyring_file
}

function ping_kms()
{
  if [ -z "${KMS_REGION}" ]; then
    return 1
  fi
  if [ -z "${KMS_AUTH_KEY}" ]; then
    return 1
  fi
  if [ -z "${KMS_SECRET_ACCESS_KEY}" ]; then
    return 1
  fi
  if [ -z "${KMS_KEY}" ]; then
    return 1
  fi
  return 0
}
