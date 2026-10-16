############################################################################
# PXB-3849: back up with an ID token issued by Percona's Keycloak.
#
# The same identity provider, realm, client and user as Percona Server's
# auth_openid_connect.idp MTR test. Needs the VPN to reach
# keycloak.int.percona.com, and curl. Skips when the identity provider
# cannot be reached.
#
# Like the MTR test, it takes from the environment:
#   OIDC_IDP_HOST     the Keycloak (default https://keycloak.int.percona.com)
#   OIDC_IDP_USER_ID  the account's "user", which must be the token's
#                     'sub' (default: read from the token)
# MTR_OIDC_IDP_HOST and MTR_OIDC_USER_ID, the MTR test's names, are used
# when these are not set.
############################################################################

. inc/common.sh
. inc/oidc_common.sh

oidc_require_server_plugin
oidc_require_client_plugin
oidc_require_token_tools

IDP_HOST=${OIDC_IDP_HOST:-${MTR_OIDC_IDP_HOST:-https://keycloak.int.percona.com}}
IDP_BASE="${IDP_HOST%/}/realms/master"
IDP_URL="${IDP_BASE}/protocol/openid-connect"
IDP_CLIENT_ID="myclient"

command -v curl >/dev/null 2>&1 || skip_test "Requires curl"
if ! curl --fail --silent --max-time 5 --output /dev/null "${IDP_URL}/certs"
then
    skip_test "Keycloak is not reachable at ${IDP_URL} (Percona's needs the VPN)"
fi

TOKEN="${TEST_VAR_ROOT}/percona_id_token"
oidc_fetch_ropc_token "${IDP_URL}/token" "${IDP_CLIENT_ID}" \
                      kkuser alamakota1 "${TOKEN}"
IDP_SUB=${OIDC_IDP_USER_ID:-${MTR_OIDC_USER_ID:-$(oidc_extract_sub "${TOKEN}")}}
[ -n "${IDP_SUB}" ] || die "no 'sub' claim in the fetched ID token"

start_server ${OIDC_SERVER_START_ARGS}

oidc_install_server_plugin "JSON://{\"percona-idp\": {\"issuer-name\": \"${IDP_BASE}\", \"jwks-url\": \"${IDP_URL}/certs\", \"audiences\": [\"${IDP_CLIENT_ID}\"]}}"

OIDC_USER=oidc_backup
oidc_create_backup_user "${OIDC_USER}" "${IDP_SUB}" percona-idp

load_sakila

oidc_backup_and_restore "${OIDC_USER}" "${TOKEN}" sakila
