#
# keyring_vault helpers
#

. inc/common.sh

plugin_load=keyring_vault.so
plugin_dir=${XTRABACKUP_BASEDIR}/keyring_vault
keyring_vault_config=${TEST_VAR_ROOT}/keyring_vault_config
keyring_args="--keyring-vault-config=${keyring_vault_config}"

MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS:-""}
early-plugin-load=${plugin_load}
keyring-vault-config=${keyring_vault_config}
"

XB_EXTRA_MY_CNF_OPTS="${XB_EXTRA_MY_CNF_OPTS:-""}
xtrabackup-plugin-dir=${plugin_dir}
"

VAULT_URL="${VAULT_URL:-https://vault.public-ci.percona.com:8200}"
VAULT_MOUNT_POINT=$(uuidgen)
VAULT_TOKEN="${VAULT_TOKEN:-58a90c08-8001-fd5f-6192-7498a48eaf2a}"
VAULT_CA="${VAULT_CA:-${PWD}/inc/vault_ca.crt}"

function keyring_vault_ping()
{
	curl -H "X-Vault-Token: ${VAULT_TOKEN}" \
		--cacert "${VAULT_CA}" -k \
		"${VAULT_URL}/v1/sys/mounts" || \
	return 1
}

function keyring_vault_mount()
{
	cat > ${keyring_vault_config} <<EOF
vault_url = ${VAULT_URL}
secret_mount_point = ${VAULT_MOUNT_POINT}
token = ${VAULT_TOKEN}
vault_ca = ${VAULT_CA}
EOF

	curl -H "X-Vault-Token: ${VAULT_TOKEN}" \
		--cacert "${VAULT_CA}" -k \
		--data '{"type":"generic"}' \
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
