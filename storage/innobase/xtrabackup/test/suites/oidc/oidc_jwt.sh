############################################################################
# PXB-3849: back up as an account that authenticates with an OpenID
# Connect ID token.
#
# The identity provider is local: the test makes an RSA key, tells the
# auth_openid_connect server plugin to trust it, and signs its own tokens.
# Nothing outside the test machine is contacted.
#
# Requires a server that ships auth_openid_connect.so (Percona Server
# 8.4.10 or later), and openssl and perl to sign the tokens. Set
# AUTH_OIDC_SERVER_SO to use a server plugin from somewhere else.
############################################################################

. inc/common.sh
. inc/oidc_common.sh

oidc_require_server_plugin
oidc_require_token_tools
oidc_require_client_plugin

start_server ${OIDC_SERVER_START_ARGS}

oidc_create_local_idp "${topdir}/idp"
oidc_install_server_plugin "${OIDC_IDP_CONFIG}"

OIDC_USER=oidc_backup
OIDC_SUB=oidc-backup-subject
oidc_create_backup_user "${OIDC_USER}" "${OIDC_SUB}"

TOKEN="${topdir}/id_token"
oidc_sign_id_token "${TOKEN}" "${OIDC_SUB}" 3600

load_sakila

# The checks in this test need the client plugin's messages and
# xtrabackup's errors in one place.
XB_LOG="${topdir}/xb.log"

function xb_expect_failure()
{
    local expected="$1"
    shift
    local rc=0
    vlog "===> $XB_BIN $XB_ARGS --backup --target-dir=${topdir}/backup_fail $*"
    $XB_BIN $XB_ARGS --backup \
        --target-dir="${topdir}/backup_fail" "$@" > "${XB_LOG}" 2>&1 || rc=$?
    cat "${XB_LOG}"
    [ ${rc} -ne 0 ] || die "xtrabackup succeeded when it was expected to fail"
    grep -qF -- "${expected}" "${XB_LOG}" || \
        die "expected '${expected}' in the xtrabackup output"
    rm -rf "${topdir}/backup_fail"
}

OIDC_ARGS="--xtrabackup-plugin-dir=${plugin_dir} --user=${OIDC_USER}"

vlog "Case 1: backup, prepare and restore over the socket"
# The redo log consumer opens a connection of its own, so the token is
# handed to the plugin more than once.
oidc_backup_and_restore "${OIDC_USER}" "${TOKEN}" sakila \
    --register-redo-log-consumer
grep -q "purpose: redo log consumer" ${OUTFILE} || \
    die "the redo log consumer connection was not opened"
uses=$(grep -c "password: not set (using OpenID Connect ID token)" ${OUTFILE})
[ "${uses}" -ge 2 ] || \
    die "expected every connection to use the ID token, saw ${uses}"
# The plugin is loaded once, before the first connection.
loads=$(grep -c "Loading the authentication_openid_connect_client plugin" ${OUTFILE})
[ "${loads}" -eq 1 ] || \
    die "expected the client plugin to be loaded once, saw ${loads}"

vlog "Case 1a: the history connection inherits the ID token"
# --history-socket gives the history record a connection of its own. Without
# --history-user it logs in as the backup account, with the same token.
rm -rf "${topdir}/backup_hist"
xtrabackup --backup --target-dir="${topdir}/backup_hist" ${OIDC_ARGS} \
    --history=oidc_hist --history-socket=${MYSQLD_SOCKET} \
    --authentication-openid-connect-client-id-token-file="${TOKEN}" \
    > "${topdir}/xb_hist.log" 2>&1 || \
    { cat "${topdir}/xb_hist.log"; die "backup with --history failed"; }
cat "${topdir}/xb_hist.log"
grep -q "password: not set (using OpenID Connect ID token).*purpose: history record" \
    "${topdir}/xb_hist.log" || \
    die "the history connection did not use the ID token"
rows=$(${MYSQL} ${MYSQL_ARGS} -Ns -e \
    "SELECT COUNT(*) FROM PERCONA_SCHEMA.xtrabackup_history WHERE name = 'oidc_hist'")
[ "${rows}" -eq 1 ] || die "expected one history record, found ${rows}"
rm -rf "${topdir}/backup_hist"

vlog "Case 1b: --history-user drops the ID token for the history connection"
# There is no token file for the history server: a history user logs in
# with --history-password, here with none.
xtrabackup --backup --target-dir="${topdir}/backup_hist" ${OIDC_ARGS} \
    --history=oidc_hist_root --history-socket=${MYSQLD_SOCKET} \
    --history-user=root \
    --authentication-openid-connect-client-id-token-file="${TOKEN}" \
    > "${topdir}/xb_hist.log" 2>&1 || \
    { cat "${topdir}/xb_hist.log"; die "backup with --history-user failed"; }
cat "${topdir}/xb_hist.log"
grep -q "user: root, password: not set, .*purpose: history record" \
    "${topdir}/xb_hist.log" || \
    die "the history connection did not drop the ID token"
rm -rf "${topdir}/backup_hist"

vlog "Case 1c: a history connection that does not inherit the token cannot send it"
# The history accounts are OIDC accounts here, so the history server asks for
# a token. The plugin keeps one token file for the whole process, and the 8.4
# plugin keeps it after the backup connection's handshake; the history
# connection, opened after it, must hand the plugin a null path and not send
# the backup's token.
xb_expect_failure "The path to ID token file is not set." ${OIDC_ARGS} \
    --history=oidc_hist_user --history-socket=${MYSQLD_SOCKET} \
    --history-user=${OIDC_USER} \
    --authentication-openid-connect-client-id-token-file="${TOKEN}"
grep -q "user: ${OIDC_USER}, password: not set, .*purpose: history record" \
    "${XB_LOG}" || die "the history connection did not drop the ID token"
xb_expect_failure "The path to ID token file is not set." ${OIDC_ARGS} \
    --history=oidc_hist_password --history-socket=${MYSQLD_SOCKET} \
    --history-password=secret \
    --authentication-openid-connect-client-id-token-file="${TOKEN}"
grep -q "user: ${OIDC_USER}, password: set, .*purpose: history record" \
    "${XB_LOG}" || die "the history connection did not drop the ID token"

vlog "Case 1d: a token file path relative to the current directory"
# The backup changes into the datadir before it opens the redo log consumer
# connection, and the client plugin opens the file in that handshake too.
rm -rf "${topdir}/backup_rel"
( cd "$(dirname "${TOKEN}")" && \
  xtrabackup --backup --target-dir="${topdir}/backup_rel" ${OIDC_ARGS} \
      --register-redo-log-consumer \
      --authentication-openid-connect-client-id-token-file="$(basename "${TOKEN}")" \
      > "${topdir}/xb_rel.log" 2>&1 ) || \
    { cat "${topdir}/xb_rel.log"; die "backup with a relative token file path failed"; }
cat "${topdir}/xb_rel.log"
grep -q "purpose: redo log consumer" "${topdir}/xb_rel.log" || \
    die "the redo log consumer connection was not opened"
rm -rf "${topdir}/backup_rel"

vlog "Case 1e: a token file path starting with ~/"
# The value is quoted, as it is in my.cnf: xtrabackup expands ~ itself.
rm -rf "${topdir}/backup_home"
HOME="$(dirname "${TOKEN}")" \
    xtrabackup --backup --target-dir="${topdir}/backup_home" ${OIDC_ARGS} \
    --authentication-openid-connect-client-id-token-file="~/$(basename "${TOKEN}")"
rm -rf "${topdir}/backup_home"

vlog "Case 1f: the rest of the token file path is used as given"
# Each path below names a valid token. Where the path could be rewritten,
# an expired token sits at the place a rewrite would lead to, so a rewritten
# path fails the backup.
oidc_sign_id_token "${topdir}/expired_decoy" "${OIDC_SUB}" -60
# Longer than FN_REFLEN.
long_dir="${topdir}/long"
while [ ${#long_dir} -lt 700 ]; do
    long_dir="${long_dir}/dddddddddddddddddddddddddddddddddddddddd"
done
mkdir -p "${long_dir}"
cp "${TOKEN}" "${long_dir}/id_token"
# A directory named ~ is not the home directory.
mkdir -p "${topdir}/tilde/~" "${topdir}/home"
cp "${TOKEN}" "${topdir}/tilde/~/id_token"
cp "${topdir}/expired_decoy" "${topdir}/home/id_token"
# dir/../ is resolved by the file system, through a symbolic link.
mkdir -p "${topdir}/links/real/sub"
ln -s "${topdir}/links/real/sub" "${topdir}/links/lnk"
cp "${TOKEN}" "${topdir}/links/real/id_token"
cp "${topdir}/expired_decoy" "${topdir}/links/id_token"
for path in "${long_dir}/id_token" "${topdir}/tilde/~/id_token" \
            "${topdir}/links/lnk/../id_token"; do
    rm -rf "${topdir}/backup_path"
    HOME="${topdir}/home" \
        xtrabackup --backup --target-dir="${topdir}/backup_path" ${OIDC_ARGS} \
        --authentication-openid-connect-client-id-token-file="${path}"
    grep -qF "Using OpenID Connect ID token file '${path}'" ${OUTFILE} || \
        die "the token file path was changed: ${path}"
done
rm -rf "${topdir}/backup_path" "${topdir}/long" "${topdir}/tilde" \
    "${topdir}/home" "${topdir}/links"

vlog "Case 2: backup over TCP with TLS"
rm -rf "${topdir}/backup_tls"
xtrabackup --backup --target-dir="${topdir}/backup_tls" ${OIDC_ARGS} \
    --host=127.0.0.1 --port=${MYSQLD_PORT} --ssl-mode=REQUIRED \
    --authentication-openid-connect-client-id-token-file="${TOKEN}"
rm -rf "${topdir}/backup_tls"

vlog "Case 3: TCP without TLS is refused"
# The server plugin refuses the connection before it asks for the token,
# so the token never crosses the network in clear text.
xb_expect_failure "Access denied for user '${OIDC_USER}'" ${OIDC_ARGS} \
    --host=127.0.0.1 --port=${MYSQLD_PORT} --ssl-mode=DISABLED \
    --authentication-openid-connect-client-id-token-file="${TOKEN}"
grep -q "unsecure connection, use TLS, socket or memory" ${MYSQLD_ERRFILE} || \
    die "the server did not refuse the connection for lack of TLS"

vlog "Case 4: --password is rejected together with the ID token"
xb_expect_failure "--password and --authentication-openid-connect-client-id-token-file are mutually exclusive" \
    ${OIDC_ARGS} --password=secret \
    --authentication-openid-connect-client-id-token-file="${TOKEN}"

vlog "Case 4b: -p is refused before it asks for the password"
# Without a terminal the prompt would read an empty password from stdin and
# the backup would go on.
xb_expect_failure "--password and --authentication-openid-connect-client-id-token-file are mutually exclusive" \
    ${OIDC_ARGS} -p \
    --authentication-openid-connect-client-id-token-file="${TOKEN}" < /dev/null
if grep -q "Enter password" "${XB_LOG}"; then
    die "xtrabackup asked for the password before it refused -p"
fi

vlog "Case 4a: --skip-password names no password and is accepted"
rm -rf "${topdir}/backup_skip"
xtrabackup --backup --target-dir="${topdir}/backup_skip" ${OIDC_ARGS} \
    --skip-password \
    --authentication-openid-connect-client-id-token-file="${TOKEN}"
rm -rf "${topdir}/backup_skip"

vlog "Case 5: a token file that does not exist"
xb_expect_failure "Failed to set the ID token file '${topdir}/no_such_token'" \
    ${OIDC_ARGS} \
    --authentication-openid-connect-client-id-token-file="${topdir}/no_such_token"

vlog "Case 5a: an empty token file name"
xb_expect_failure "--authentication-openid-connect-client-id-token-file requires the name of the file holding the ID token" \
    ${OIDC_ARGS} --authentication-openid-connect-client-id-token-file=

vlog "Case 6: a token file that does not hold a JWT"
echo "not a token" > "${topdir}/bad_token"
xb_expect_failure "does not contain a valid JWT" ${OIDC_ARGS} \
    --authentication-openid-connect-client-id-token-file="${topdir}/bad_token"

vlog "Case 7: an expired token"
oidc_sign_id_token "${topdir}/expired_token" "${OIDC_SUB}" -60
xb_expect_failure "Access denied for user '${OIDC_USER}'" ${OIDC_ARGS} \
    --authentication-openid-connect-client-id-token-file="${topdir}/expired_token"

vlog "Case 8: a token signed by a key the server does not trust"
run_cmd openssl genrsa -out "${topdir}/other_key.pem" 2048
oidc_sign_id_token "${topdir}/foreign_token" "${OIDC_SUB}" 3600 \
    "${topdir}/other_key.pem"
xb_expect_failure "Access denied for user '${OIDC_USER}'" ${OIDC_ARGS} \
    --authentication-openid-connect-client-id-token-file="${topdir}/foreign_token"

vlog "Case 9: a token for another subject"
oidc_sign_id_token "${topdir}/other_sub_token" someone-else 3600
xb_expect_failure "Access denied for user '${OIDC_USER}'" ${OIDC_ARGS} \
    --authentication-openid-connect-client-id-token-file="${topdir}/other_sub_token"

vlog "Case 10: the client plugin is not in --xtrabackup-plugin-dir"
mkdir -p "${topdir}/empty_plugin_dir"
xb_expect_failure "Cannot load the authentication_openid_connect_client plugin" \
    --xtrabackup-plugin-dir="${topdir}/empty_plugin_dir" --user=${OIDC_USER} \
    --authentication-openid-connect-client-id-token-file="${TOKEN}"
