############################################################################
# PXB-3849: back up with an ID token issued by a Keycloak server.
#
# KEYCLOAK_URL names a running Keycloak, for example http://keycloak:8080.
# The test waits until it serves the realm (KEYCLOAK_READY_TIMEOUT
# seconds, default 120), fetches a fresh ID token and backs up with it.
# The realm must have a client that allows direct access grants and puts
# itself in the ID token's audience, and a user that can log in to it:
#
#   KEYCLOAK_REALM     default pxb
#   KEYCLOAK_CLIENT    default pxb-xtrabackup
#   KEYCLOAK_USER      default xtrabackup-oidc
#   KEYCLOAK_PASSWORD  default pxb-test
#
# Jenkins starts such a Keycloak and sets KEYCLOAK_URL. Locally,
# OIDC_BOOTSTRAP_KEYCLOAK=1 starts and provisions one in Docker
# (inc/oidc_keycloak_docker.sh) instead. Skips when there is neither.
############################################################################

. inc/common.sh
. inc/oidc_common.sh

oidc_require_server_plugin
oidc_require_client_plugin
oidc_require_token_tools

command -v curl >/dev/null 2>&1 || skip_test "Requires curl"

if [ "${OIDC_BOOTSTRAP_KEYCLOAK:-0}" = "1" ]; then
    . inc/oidc_keycloak_docker.sh
    oidc_keycloak_docker_bootstrap
fi

[ -n "${KEYCLOAK_URL:-}" ] || \
    skip_test "No Keycloak: set KEYCLOAK_URL, or OIDC_BOOTSTRAP_KEYCLOAK=1 to start one in Docker"

KEYCLOAK_REALM=${KEYCLOAK_REALM:-pxb}
KEYCLOAK_CLIENT=${KEYCLOAK_CLIENT:-pxb-xtrabackup}
REALM_URL="${KEYCLOAK_URL%/}/realms/${KEYCLOAK_REALM}"

# A Keycloak started with the job may still be importing its realm.
waited=0
until curl --fail --silent --max-time 3 --output /dev/null \
      "${REALM_URL}/.well-known/openid-configuration"; do
    [ ${waited} -lt ${KEYCLOAK_READY_TIMEOUT:-120} ] || \
        die "Keycloak at ${KEYCLOAK_URL} did not serve realm ${KEYCLOAK_REALM} within ${waited}s"
    sleep 2
    waited=$((waited + 2))
done

# Fetched here and not by whoever started Keycloak: a token fetched when
# the job starts can expire before this test runs.
TOKEN="${TEST_VAR_ROOT}/keycloak_id_token"
oidc_fetch_ropc_token "${REALM_URL}/protocol/openid-connect/token" \
    "${KEYCLOAK_CLIENT}" "${KEYCLOAK_USER:-xtrabackup-oidc}" \
    "${KEYCLOAK_PASSWORD:-pxb-test}" "${TOKEN}"

IDP_SUB=$(oidc_extract_sub "${TOKEN}")
[ -n "${IDP_SUB}" ] || die "no 'sub' claim in the ID token from ${KEYCLOAK_URL}"

start_server ${OIDC_SERVER_START_ARGS}

oidc_install_server_plugin "JSON://{\"keycloak\": {\"issuer-name\": \"${REALM_URL}\", \"jwks-url\": \"${REALM_URL}/protocol/openid-connect/certs\", \"audiences\": [\"${KEYCLOAK_CLIENT}\"]}}"

OIDC_USER=oidc_backup
oidc_create_backup_user "${OIDC_USER}" "${IDP_SUB}" keycloak

load_sakila

oidc_backup_and_restore "${OIDC_USER}" "${TOKEN}" sakila
