############################################################################
# Helpers for the OpenID Connect authentication tests in suites/oidc/.
#
# The server side is the auth_openid_connect plugin of Percona Server. The
# client side is authentication_openid_connect_client.so, which PXB builds
# and xtrabackup loads from --xtrabackup-plugin-dir.
#
# Sourced after inc/common.sh.
############################################################################

OIDC_CLIENT_PLUGIN=authentication_openid_connect_client.so
OIDC_SERVER_PLUGIN=auth_openid_connect.so

##########################################################################
# Skip the test unless the server under test can load auth_openid_connect.
#
# The plugin is taken from the server's own plugin directory, which is
# where a Percona Server tarball ships it. AUTH_OIDC_SERVER_SO names
# another copy, for example one from a server build tree; the server is
# then started with that copy's directory as --plugin-dir.
#
# Sets OIDC_SERVER_START_ARGS for start_server.
##########################################################################
function oidc_require_server_plugin()
{
    OIDC_SERVER_START_ARGS=""

    if [ -n "${AUTH_OIDC_SERVER_SO:-}" ]; then
        [ -f "${AUTH_OIDC_SERVER_SO}" ] || \
            die "AUTH_OIDC_SERVER_SO=${AUTH_OIDC_SERVER_SO} does not exist"
        OIDC_SERVER_SO_NAME=$(basename "${AUTH_OIDC_SERVER_SO}")
        OIDC_SERVER_START_ARGS="--plugin-dir=$(dirname "${AUTH_OIDC_SERVER_SO}")"
    elif [ -f "${MYSQL_BASEDIR}/lib/plugin/${OIDC_SERVER_PLUGIN}" ]; then
        OIDC_SERVER_SO_NAME=${OIDC_SERVER_PLUGIN}
    else
        skip_test "Requires a server that ships the ${OIDC_SERVER_PLUGIN} plugin (Percona Server 8.4.10 or later)"
    fi
}

##########################################################################
# Fail unless the client plugin was built next to xtrabackup's other
# plugins. run.sh sets plugin_dir to the build's plugin_output_directory
# or to the installed plugin directory. A missing plugin is a packaging
# bug, so it is an error and not a skip.
##########################################################################
function oidc_require_client_plugin()
{
    [ -f "${plugin_dir}/${OIDC_CLIENT_PLUGIN}" ] || \
        die "${OIDC_CLIENT_PLUGIN} not found in ${plugin_dir}"
}

##########################################################################
# Skip the test unless the tools used to sign and read ID tokens are
# present.
##########################################################################
function oidc_require_token_tools()
{
    command -v openssl >/dev/null 2>&1 || \
        skip_test "Requires openssl to sign and read ID tokens"
    command -v perl >/dev/null 2>&1 || \
        skip_test "Requires perl to sign and read ID tokens"
}

##########################################################################
# Read stdin, write it base64url encoded, without padding.
##########################################################################
function oidc_b64url()
{
    openssl base64 -A | tr '+/' '-_' | tr -d '='
}

##########################################################################
# Create a local identity provider: an RSA key pair, and a server
# configuration that trusts its public key.
#
# Args:
#   $1 - directory to keep the key in
#
# Sets OIDC_IDP_KEY (private key file) and OIDC_IDP_CONFIG (the value for
# auth_openid_connect_configuration).
##########################################################################
OIDC_IDP_NAME=pxb-idp
OIDC_IDP_ISSUER=https://idp.pxb-test.example/realms/xtrabackup
OIDC_IDP_AUDIENCE=pxb-test-client
OIDC_IDP_KID=pxb-key-1

function oidc_create_local_idp()
{
    local dir="$1"
    mkdir -p "${dir}"

    OIDC_IDP_KEY="${dir}/idp_private.pem"
    run_cmd openssl genrsa -out "${OIDC_IDP_KEY}" 2048

    local n
    n=$(openssl rsa -in "${OIDC_IDP_KEY}" -noout -modulus | cut -d= -f2 | \
        perl -ne 'chomp; print pack("H*", $_)' | oidc_b64url)
    [ -n "${n}" ] || die "could not read the modulus of ${OIDC_IDP_KEY}"

    OIDC_IDP_CONFIG="JSON://{\"${OIDC_IDP_NAME}\": {\"issuer-name\": \"${OIDC_IDP_ISSUER}\", \"keys\": [{\"kid\": \"${OIDC_IDP_KID}\", \"kty\": \"RSA\", \"n\": \"${n}\", \"e\": \"AQAB\", \"use\": \"sig\", \"alg\": \"RS256\"}], \"audiences\": [\"${OIDC_IDP_AUDIENCE}\"]}}"
}

##########################################################################
# Write an RS256 ID token signed with a private key.
#
# Args:
#   $1 - output file
#   $2 - subject (the 'sub' claim)
#   $3 - lifetime in seconds from now; negative makes an expired token
#   $4 - private key (default: OIDC_IDP_KEY)
##########################################################################
function oidc_sign_id_token()
{
    local out="$1"
    local sub="$2"
    local ttl="${3:-3600}"
    local key="${4:-${OIDC_IDP_KEY}}"
    local now
    now=$(date +%s)

    local iat=${now}
    local exp=$((now + ttl))
    if [ "${ttl}" -lt 0 ]; then
        iat=$((exp - 3600))
    fi

    local header payload signature
    header=$(printf '{"alg":"RS256","typ":"JWT","kid":"%s"}' "${OIDC_IDP_KID}" | \
             oidc_b64url)
    payload=$(printf '{"iss":"%s","sub":"%s","aud":"%s","iat":%d,"exp":%d}' \
              "${OIDC_IDP_ISSUER}" "${sub}" "${OIDC_IDP_AUDIENCE}" \
              "${iat}" "${exp}" | oidc_b64url)
    signature=$(printf '%s.%s' "${header}" "${payload}" | \
                openssl dgst -sha256 -sign "${key}" | oidc_b64url)

    printf '%s.%s.%s\n' "${header}" "${payload}" "${signature}" > "${out}"
}

##########################################################################
# INSTALL PLUGIN auth_openid_connect and set its configuration.
# Args:
#   $1 - FILE://... or JSON://...
##########################################################################
function oidc_install_server_plugin()
{
    local config="$1"

    vlog "Installing the auth_openid_connect server plugin"
    run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
        "INSTALL PLUGIN auth_openid_connect SONAME '${OIDC_SERVER_SO_NAME}'"
    OIDC_SERVER_CONFIG="${config}"
    oidc_configure_server_plugin
}

##########################################################################
# Set auth_openid_connect_configuration to OIDC_SERVER_CONFIG. The
# variable is not persisted, so a restarted server needs it again.
##########################################################################
function oidc_configure_server_plugin()
{
    run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
        "SET GLOBAL auth_openid_connect_configuration = '${OIDC_SERVER_CONFIG}'"
}

##########################################################################
# CREATE USER ... IDENTIFIED WITH auth_openid_connect for a backup account.
#
# Args:
#   $1 - account name
#   $2 - identity provider subject, matched against the token's 'sub'
#   $3 - identity provider name in the server configuration
##########################################################################
function oidc_create_backup_user()
{
    local user="$1"
    local sub="$2"
    local idp="${3:-${OIDC_IDP_NAME}}"

    vlog "Creating OIDC account ${user} (sub=${sub}, idp=${idp})"
    run_cmd ${MYSQL} ${MYSQL_ARGS} -e "
        CREATE USER '${user}'@'localhost' IDENTIFIED WITH 'auth_openid_connect'
            AS '{\"identity_provider\": \"${idp}\", \"user\": \"${sub}\"}';
        CREATE USER '${user}'@'127.0.0.1' IDENTIFIED WITH 'auth_openid_connect'
            AS '{\"identity_provider\": \"${idp}\", \"user\": \"${sub}\"}';
        GRANT ALL ON *.* TO '${user}'@'localhost';
        GRANT ALL ON *.* TO '${user}'@'127.0.0.1';
    "
}

##########################################################################
# Print the 'sub' claim of a token. Real identity providers put an
# internal id there, not the login name.
##########################################################################
function oidc_extract_sub()
{
    local token_file="$1"
    local b64
    b64=$(cut -d. -f2 "${token_file}" | tr -- '-_' '+/')
    local pad=$(( (4 - ${#b64} % 4) % 4 ))
    printf '%s%s' "${b64}" "$(printf '%*s' ${pad} '' | tr ' ' '=')" | \
        openssl base64 -d -A | \
        perl -ne 'print "$1\n" if /"sub"\s*:\s*"([^"]*)"/'
}

##########################################################################
# Fetch an ID token with a Resource Owner Password Credentials grant.
# The response is read with perl, which the tests need anyway, not jq.
#
# Args: token endpoint, client id, user, password, output file
##########################################################################
function oidc_fetch_ropc_token()
{
    local token_url="$1"
    local client_id="$2"
    local username="$3"
    local password="$4"
    local out_file="$5"

    command -v curl >/dev/null 2>&1 || \
        skip_test "Requires curl to fetch an ID token"

    vlog "Fetching an ID token from ${token_url} for ${username}"
    curl --fail --silent --show-error --max-time 15 \
         -X POST "${token_url}" \
         -H "Content-Type: application/x-www-form-urlencoded" \
         -d "grant_type=password" \
         --data-urlencode "client_id=${client_id}" \
         --data-urlencode "username=${username}" \
         --data-urlencode "password=${password}" \
         -d "scope=openid" | \
        perl -ne 'if (/"id_token"\s*:\s*"([^"]+)"/) { print "$1\n"; $ok = 1 }
                  END { exit !$ok }' > "${out_file}" || \
        die "failed to fetch an ID token from ${token_url}"
}

##########################################################################
# Back up as an OIDC account, prepare, restore, and compare the data.
#
# Args:
#   $1 - account name
#   $2 - ID token file
#   $3 - database to compare
#   $@ - extra xtrabackup --backup options
##########################################################################
function oidc_backup_and_restore()
{
    local user="$1"
    local token_file="$2"
    local db="$3"
    shift 3

    record_db_state "${db}"

    rm -rf "${topdir}/backup"
    xtrabackup --backup --target-dir="${topdir}/backup" \
        --xtrabackup-plugin-dir="${plugin_dir}" \
        --user="${user}" \
        --authentication-openid-connect-client-id-token-file="${token_file}" \
        "$@"

    # Without these lines another credential could have let the backup in.
    grep -q "Using OpenID Connect ID token file '${token_file}'" ${OUTFILE} || \
        die "xtrabackup did not log that it used the ID token file"
    grep -q "password: not set (using OpenID Connect ID token)" ${OUTFILE} || \
        die "xtrabackup did not log the OIDC credential"

    xtrabackup --prepare --target-dir="${topdir}/backup"

    stop_server
    rm -rf "${mysql_datadir}"
    xtrabackup --copy-back --target-dir="${topdir}/backup"
    start_server ${OIDC_SERVER_START_ARGS}
    oidc_configure_server_plugin

    verify_db_state "${db}"
}
