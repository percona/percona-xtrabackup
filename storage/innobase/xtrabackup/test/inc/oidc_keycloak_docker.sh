############################################################################
# Start a throwaway Keycloak in Docker for suites/oidc/oidc_keycloak.sh.
#
# oidc_keycloak_docker_bootstrap starts the container, creates a realm, a
# client and a user with kcadm.sh, and exports KEYCLOAK_URL and the login
# the test uses to fetch an ID token. The container is removed when the
# test exits. Skips the test when docker is not available.
#
# Keycloak is the upstream community image (Apache 2.0).
############################################################################

# Override any of these before sourcing to customise the bootstrapped
# container.  Defaults are chosen to be low-conflict with existing
# containers on developer laptops.
OIDC_KEYCLOAK_IMAGE=${OIDC_KEYCLOAK_IMAGE:-quay.io/keycloak/keycloak:26.0}
OIDC_KEYCLOAK_CONTAINER=${OIDC_KEYCLOAK_CONTAINER:-pxb-oidc-keycloak}
OIDC_KEYCLOAK_PORT=${OIDC_KEYCLOAK_PORT:-8080}
OIDC_KEYCLOAK_ADMIN=${OIDC_KEYCLOAK_ADMIN:-admin}
OIDC_KEYCLOAK_ADMIN_PASSWORD=${OIDC_KEYCLOAK_ADMIN_PASSWORD:-admin}
OIDC_KEYCLOAK_REALM=${OIDC_KEYCLOAK_REALM:-pxb}
OIDC_KEYCLOAK_CLIENT=${OIDC_KEYCLOAK_CLIENT:-pxb-xtrabackup}
OIDC_KEYCLOAK_USER=${OIDC_KEYCLOAK_USER:-xtrabackup-oidc}
OIDC_KEYCLOAK_PASSWORD=${OIDC_KEYCLOAK_PASSWORD:-pxb-test}
OIDC_KEYCLOAK_READY_TIMEOUT=${OIDC_KEYCLOAK_READY_TIMEOUT:-90}

##########################################################################
# Start a Keycloak container in dev mode and wait for it to become
# reachable.  Sets OIDC_KEYCLOAK_BASE_URL on success.
##########################################################################
function oidc_keycloak_docker_start()
{
    if ! command -v docker >/dev/null 2>&1; then
        skip_test "docker not available; cannot bootstrap Keycloak"
    fi

    # If a previous run left a container behind, get rid of it before
    # binding to the same port again.
    docker rm -f "${OIDC_KEYCLOAK_CONTAINER}" >/dev/null 2>&1 || true

    vlog "Starting Keycloak container ${OIDC_KEYCLOAK_CONTAINER} from ${OIDC_KEYCLOAK_IMAGE}"
    run_cmd docker run --rm -d \
        --name "${OIDC_KEYCLOAK_CONTAINER}" \
        -p "${OIDC_KEYCLOAK_PORT}:8080" \
        -e KC_BOOTSTRAP_ADMIN_USERNAME="${OIDC_KEYCLOAK_ADMIN}" \
        -e KC_BOOTSTRAP_ADMIN_PASSWORD="${OIDC_KEYCLOAK_ADMIN_PASSWORD}" \
        "${OIDC_KEYCLOAK_IMAGE}" \
        start-dev >/dev/null

    # Remove the container whenever the test exits, also when Keycloak does
    # not become ready. run.sh cleans up after each test from an EXIT trap:
    # keep it.
    trap 'oidc_keycloak_docker_stop; cleanup_on_test_exit' EXIT

    OIDC_KEYCLOAK_BASE_URL="http://localhost:${OIDC_KEYCLOAK_PORT}"
    export OIDC_KEYCLOAK_BASE_URL

    # Poll the OIDC discovery doc until Keycloak is ready to serve.
    local waited=0
    while true; do
        if curl --fail --silent --show-error --max-time 3 --output /dev/null \
           "${OIDC_KEYCLOAK_BASE_URL}/realms/master/.well-known/openid-configuration"; then
            break
        fi
        waited=$((waited + 2))
        if [ ${waited} -ge ${OIDC_KEYCLOAK_READY_TIMEOUT} ]; then
            docker logs "${OIDC_KEYCLOAK_CONTAINER}" >&2 || true
            die "Keycloak did not become ready within ${OIDC_KEYCLOAK_READY_TIMEOUT}s"
        fi
        sleep 2
    done
    vlog "Keycloak ready at ${OIDC_KEYCLOAK_BASE_URL} after ${waited}s"
}

##########################################################################
# Stop and remove the Keycloak container.  Safe to call multiple times.
##########################################################################
function oidc_keycloak_docker_stop()
{
    if command -v docker >/dev/null 2>&1; then
        docker rm -f "${OIDC_KEYCLOAK_CONTAINER}" >/dev/null 2>&1 || true
    fi
}

##########################################################################
# Run kcadm.sh in the Keycloak container. The arguments are passed as they
# are, so values need no shell quoting.
##########################################################################
function oidc_kcadm()
{
    run_cmd docker exec "${OIDC_KEYCLOAK_CONTAINER}" \
        /opt/keycloak/bin/kcadm.sh "$@"
}

##########################################################################
# Provision a realm, a public client with direct-grant enabled (for
# ROPC), a user, and an audience mapper so the ID token carries the
# client_id in its 'aud' claim.
##########################################################################
function oidc_keycloak_docker_provision()
{
    vlog "Authenticating kcadm to Keycloak master realm"
    oidc_kcadm config credentials \
        --server http://localhost:8080 \
        --realm master \
        --user "${OIDC_KEYCLOAK_ADMIN}" \
        --password "${OIDC_KEYCLOAK_ADMIN_PASSWORD}"

    vlog "Creating realm ${OIDC_KEYCLOAK_REALM}"
    oidc_kcadm create realms \
        -s realm="${OIDC_KEYCLOAK_REALM}" \
        -s enabled=true

    vlog "Creating public client ${OIDC_KEYCLOAK_CLIENT} with directAccessGrants"
    oidc_kcadm create clients \
        -r "${OIDC_KEYCLOAK_REALM}" \
        -s clientId="${OIDC_KEYCLOAK_CLIENT}" \
        -s publicClient=true \
        -s directAccessGrantsEnabled=true \
        -s standardFlowEnabled=false \
        -s 'redirectUris=[]'

    # Keycloak 26 refuses a password login ("Account is not fully set up")
    # for a user without an email address and a name.
    vlog "Creating user ${OIDC_KEYCLOAK_USER}"
    oidc_kcadm create users \
        -r "${OIDC_KEYCLOAK_REALM}" \
        -s username="${OIDC_KEYCLOAK_USER}" \
        -s enabled=true \
        -s email="${OIDC_KEYCLOAK_USER}@example.com" \
        -s emailVerified=true \
        -s firstName=XtraBackup \
        -s lastName=Test
    oidc_kcadm set-password \
        -r "${OIDC_KEYCLOAK_REALM}" \
        --username "${OIDC_KEYCLOAK_USER}" \
        --new-password "${OIDC_KEYCLOAK_PASSWORD}"

    vlog "Adding audience mapper so ID tokens carry aud=${OIDC_KEYCLOAK_CLIENT}"
    local client_iid
    client_iid=$(docker exec "${OIDC_KEYCLOAK_CONTAINER}" \
        /opt/keycloak/bin/kcadm.sh get clients \
        -r "${OIDC_KEYCLOAK_REALM}" \
        -q clientId="${OIDC_KEYCLOAK_CLIENT}" \
        --fields id --format csv --noquotes 2>/dev/null | tail -1)
    [ -n "${client_iid}" ] || die "could not resolve internal id of client ${OIDC_KEYCLOAK_CLIENT}"

    oidc_kcadm create "clients/${client_iid}/protocol-mappers/models" \
        -r "${OIDC_KEYCLOAK_REALM}" \
        -s name=aud-mapper \
        -s protocol=openid-connect \
        -s protocolMapper=oidc-audience-mapper \
        -s "config.\"included.client.audience\"=${OIDC_KEYCLOAK_CLIENT}" \
        -s 'config."id.token.claim"=true' \
        -s 'config."access.token.claim"=false'
}

##########################################################################
# Start and provision Keycloak, and export KEYCLOAK_URL, KEYCLOAK_REALM,
# KEYCLOAK_CLIENT, KEYCLOAK_USER and KEYCLOAK_PASSWORD for oidc_keycloak.sh.
##########################################################################
function oidc_keycloak_docker_bootstrap()
{
    oidc_keycloak_docker_start
    oidc_keycloak_docker_provision

    KEYCLOAK_URL="${OIDC_KEYCLOAK_BASE_URL}"
    KEYCLOAK_REALM="${OIDC_KEYCLOAK_REALM}"
    KEYCLOAK_CLIENT="${OIDC_KEYCLOAK_CLIENT}"
    KEYCLOAK_USER="${OIDC_KEYCLOAK_USER}"
    KEYCLOAK_PASSWORD="${OIDC_KEYCLOAK_PASSWORD}"
    export KEYCLOAK_URL KEYCLOAK_REALM KEYCLOAK_CLIENT KEYCLOAK_USER \
           KEYCLOAK_PASSWORD
}
