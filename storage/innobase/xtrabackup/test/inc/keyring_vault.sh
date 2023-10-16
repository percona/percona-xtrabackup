#
# keyring_vault helpers
#

. inc/common.sh

if [ -z ${KEYRING_TYPE+x} ]; then
  KEYRING_TYPE="component"
fi

instance_local_manifest=""
keyring_component_cnf=${TEST_VAR_ROOT}/component_keyring_vault.cnf
keyring_args="--component-keyring-config=${keyring_component_cnf}"


VAULT_URL="${VAULT_URL:-https://vault.public-ci.percona.com:8200}"
VAULT_MOUNT_POINT=$(uuidgen)
VAULT_TOKEN="${VAULT_TOKEN:-58a90c08-8001-fd5f-6192-7498a48eaf2a}"
VAULT_CA="${VAULT_CA:-${PWD}/inc/vault_ca.crt}"

function keyring_vault_ping()
{
	curl -H "X-Vault-Token: ${VAULT_TOKEN}" \
		--cacert "${VAULT_CA}" -k \
		--connect-timeout 3 \
		"${VAULT_URL}/v1/sys/mounts" || \
	return 1
}

function configure_keyring_file_component()
{
	local VAULT_MOUNT_DATA=""
  if [ "$MYSQL_DEBUG_MODE" = "on" ]; then
    binary="mysqld-debug"
  else
    binary="mysqld"
  fi
  instance_local_manifest="${TEST_VAR_ROOT}/${binary}.my"
  cat <<EOF > "${MYSQLD_DATADIR}/${binary}.my"
{
    "components": "file://component_keyring_vault"
}
EOF
  cat <<EOF > "${MYSQLD_DATADIR}/component_keyring_vault.cnf"
{
  "vault_url": "${VAULT_URL}",
  "secret_mount_point": "${VAULT_MOUNT_POINT}",
  "secret_mount_point_version": "${VAULT_CONFIG_VERSION}",
  "token": "${VAULT_TOKEN}",
  "vault_ca": "${VAULT_CA}"
}
EOF
  cp "${MYSQLD_DATADIR}/${binary}.my" ${instance_local_manifest}
  cp "${MYSQLD_DATADIR}/component_keyring_vault.cnf" ${keyring_component_cnf}

	if [[ "${VAULT_MOUNT_VERSION}" -eq "1" ]];
	then
		VAULT_MOUNT_DATA="{\"type\":\"kv\"}"
	elif [[ "${VAULT_MOUNT_VERSION}" -eq "2" ]];
	then
		VAULT_MOUNT_DATA="{\"type\":\"kv\", \"options\": { \"version\":\"2\" }}"
	fi

	curl -H "X-Vault-Token: ${VAULT_TOKEN}" \
		--cacert "${VAULT_CA}" -k \
		--data "${VAULT_MOUNT_DATA}" \
		-X POST \
		"${VAULT_URL}/v1/sys/mounts/${VAULT_MOUNT_POINT}"
	return $?
}

function keyring_vault_unmount()
{
	curl -H "X-Vault-Token: ${VAULT_TOKEN}" \
		--cacert "${VAULT_CA}" -k \
		-X DELETE \
		"${VAULT_URL}/v1/sys/mounts/${VAULT_MOUNT_POINT}"
	return $?
}

function keyring_vault_list_keys()
{
	curl -H "X-Vault-Token: ${VAULT_TOKEN}" \
		--cacert "${VAULT_CA}" -k \
		-X LIST \
		"${VAULT_URL}/v1/${VAULT_MOUNT_POINT}" \
		| sed 's/^.*\[//' | sed 's/\].*$//' | tr , '\n' | sed 's/"//g'
}

function keyring_vault_remove_all_keys()
{
	for key in `keyring_vault_list_keys` ; do
		curl -H "X-Vault-Token: ${VAULT_TOKEN}" \
			--cacert "${VAULT_CA}" -k \
			-X DELETE \
			"${VAULT_URL}/v1/${VAULT_MOUNT_POINT}/${key}"
	done
}
