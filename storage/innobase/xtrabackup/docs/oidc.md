# OpenID Connect authentication: notes for maintainers

PXB-3849. The design and its reasons are in `PXB-improvement-oidc.md`.
This file says where the code is, how to run the tests, and what tends
to go wrong.

## How it works

The OIDC code is in `src/backup_oidc.{h,cc}` (namespace `xb::oidc`);
the rest of xtrabackup calls it from five places.

1. `--authentication-openid-connect-client-id-token-file` sets
   `opt_oidc_id_token_file` (`src/xtrabackup.cc`).
2. `main()` calls `xb::oidc::check_options(tty_password)` before
   `xb::validate_connection_options()`. It rejects an empty file name,
   and the file together with a non-empty `--password` or a `-p` that
   asks for one. `main()` asks for the `-p` password only after these
   checks, so the conflict is reported before the prompt.
   `--skip-password` names no password and is accepted.
3. `main()` calls `xb::oidc::resolve_path(cwd)`, which expands a
   leading `~/` and makes a relative path absolute, from the directory
   xtrabackup was started in: the plugin opens the file in every
   handshake, and the backup changes into the datadir before it opens its
   later connections. It only adds the prefix and leaves the rest of the
   path as given. `my_load_path()` and `unpack_filename()`, used for
   `--target-dir`, are not used here: they cut a path at `FN_REFLEN`,
   collapse `dir/../` as text (wrong when `dir` is a symbolic link) and
   replace everything before a `/~/` with the home directory.
4. `resolve()` (`src/backup_conn.cc`) puts the file in
   `Dsn::id_token_file`. The history destination drops it when
   `--history-user` or `--history-password` is named, as it does for the
   password.
5. `open_connections()` first calls `xb::oidc::load_plugin()`, once, on
   the main thread, before any connection. With a token file it logs the
   file, sets `MYSQL_PLUGIN_DIR` to `opt_plugin_dir`
   (`--xtrabackup-plugin-dir`, or `PLUGINDIR`) on a short-lived handle and
   loads the plugin with `mysql_client_find_plugin()`. The plugin pointer
   does not change after that, so it is read without a lock.
6. `xb::connect()` makes an `xb::oidc::Connect_guard` and calls its
   `set_plugin_options(dsn.id_token_file)` and then
   `mysql_real_connect()` in one block; the guard's destructor releases
   the mutex. When the plugin is loaded, `set_plugin_options()` takes the
   mutex and passes the connection's file with
   `mysql_plugin_options(plugin, "id-token-file", file)`, or a null path
   for a connection without a token. The server asks for
   `auth_openid_connect`, libmysqlclient switches to the client plugin,
   and the plugin reads the file and sends the token.
7. `close_connections()` calls `xb::oidc::release()`, which gives the
   plugin a null path, which makes it free any copy it still holds.
   xtrabackup never calls `mysql_library_end()`, so the plugin is never
   deinitialized and would not free it itself.

The plugin keeps the path in one process-wide variable, not per
connection: `mysql_plugin_options()` takes the plugin, not a `MYSQL`, and
the handshake ignores the `MYSQL` it is given. Upstream's plugin (9.7 and
trunk) frees the path after each handshake (the WL#16269 post-push fix),
so the path is handed over before every connection. The 8.4 plugin
(PS-11019) does not free it yet; the missing commits are expected in a
later Percona Server 8.4 release, and handing the path over again is
harmless until then. Connections are opened from several threads (the
redo log archive and the query killer open their own), so handing over
the path and the handshake run under the mutex in `backup_oidc.cc`.

## History connection

The history connection (`--history` with `--history-host` and the other
`--history-*` options) has no token file of its own. It uses the main
token when `--history-user` and `--history-password` are not given, and
a password otherwise. Because the plugin keeps one path for the whole
process, a history connection that does not inherit the token could find
the path handed over for the backup connection. So a connection without a
token hands the plugin a null path, under the same mutex, and its
handshake fails with "The path to ID token file is not set" if the
history server asks for a token.

The history connection cannot log in as an OIDC account with a
different token, for example on a history server that trusts another
identity provider or audience. The path is already handed over before
every connection under the mutex, so supporting that would only need a
new option and a `Dsn` field.

## Files

| File | What |
|---|---|
| `libmysql/authentication_openid_connect_client/` | The client plugin. On 8.4 it is PS-11019 cherry-picked as is, on 9.7 and trunk it is upstream's. Do not change it in PXB: keep it the same file Percona Server ships. |
| `libmysql/authentication_openid_connect_client/CMakeLists.txt` | The one PXB change: `AND NOT WITH_XTRABACKUP` in the skip condition, so the plugin is always built. |
| `include/mysql/plugin_auth_common.h` | `is_tls_established` in `MYSQL_PLUGIN_VIO_INFO` (PS-11019 on 8.4, upstream on 9.7 and trunk). Set in `mpvio_info()` in `sql-common/client.cc`. |
| `storage/innobase/xtrabackup/src/backup_oidc.{h,cc}` | `xb::oidc`: the option checks, the path, loading the plugin, `Connect_guard` and its mutex, `release()`. |
| `storage/innobase/xtrabackup/src/backup_conn.{h,cc}` | `Dsn::id_token_file`, and the calls from `connect()`, `open_connections()` and `close_connections()`. |
| `storage/innobase/xtrabackup/src/xtrabackup.{h,cc}` | The option. |
| `storage/innobase/xtrabackup/utils/percona-xtrabackup.spec` | Lists the `.so`. The DEB installs `plugin/*.so`. |
| `storage/innobase/xtrabackup/test/inc/oidc_common.sh` | Test helpers. |
| `storage/innobase/xtrabackup/test/inc/oidc_keycloak_docker.sh` | Keycloak in Docker. |
| `storage/innobase/xtrabackup/test/suites/oidc/` | The tests. |

## Running the tests

From the build tree's test directory, against a Percona Server tarball
(8.4.10 or later, or 9.7.2 or later) that ships `auth_openid_connect.so`:

```
cd bld/storage/innobase/xtrabackup/test
./run.sh -f -d /path/to/Percona-Server-8.4.11-11-Linux.x86_64.glibc2.35-minimal -s oidc
```

`oidc_jwt` needs nothing else. It makes an RSA key, puts the public key
in `auth_openid_connect_configuration` as a JWKS, and signs tokens with
`openssl dgst`. The other two need an identity provider:

| Variable | Used by | Meaning |
|---|---|---|
| `AUTH_OIDC_SERVER_SO` | all | Use this `auth_openid_connect.so` instead of the server's own; the server is started with its directory as `--plugin-dir`. |
| `KEYCLOAK_URL` | `oidc_keycloak` | A running Keycloak, for example `http://keycloak:8080`. The test waits for it (`KEYCLOAK_READY_TIMEOUT`, default 120 s) and fetches a fresh token as `KEYCLOAK_USER` with `KEYCLOAK_PASSWORD` from `KEYCLOAK_CLIENT` in `KEYCLOAK_REALM` (defaults `xtrabackup-oidc`, `pxb-test`, `pxb-xtrabackup`, `pxb`). This is how Jenkins runs it: see below. |
| `OIDC_BOOTSTRAP_KEYCLOAK=1` | `oidc_keycloak` | Start Keycloak in Docker (`quay.io/keycloak/keycloak:26.0`), provision it with the defaults above and set `KEYCLOAK_URL`. |
| `OIDC_KEYCLOAK_*` | `oidc_keycloak_docker.sh` | Image, port, realm, client, user and password of the container. |
| `OIDC_IDP_HOST`, `OIDC_IDP_USER_ID` | `oidc_percona` | Another Keycloak with the same realm, client and user, and the account's `"user"` (default: the token's `sub`). `MTR_OIDC_IDP_HOST` and `MTR_OIDC_USER_ID`, the names Percona Server's MTR test uses, work too. |

On Jenkins, `run-test` in Percona-Lab/jenkins-pipelines (`pxb/v2/docker`)
starts `quay.io/keycloak/keycloak:26.0` beside the MinIO, Vault and KMIP
containers when `WITH_KEYCLOAK_TESTS` is true and the target is Percona
Server. The realm, client and user are imported from
`keycloak-realm-pxb.json` there, and the test container gets the server
as `KEYCLOAK_URL`.

`oidc_percona` uses `keycloak.int.percona.com` with the same client and
user as Percona Server's `auth_openid_connect.idp` MTR test, and skips
when it cannot reach it.

`run.sh` sets `plugin_dir` to `plugin_output_directory` in a build tree
and to `lib/plugin` in the test package; the tests pass it as
`--xtrabackup-plugin-dir`. A missing client plugin fails the test: it is
a build or packaging bug.

## Pitfalls

- **`Access denied ... (using password: NO)` with a real identity
  provider.** The account's `"user"` must be the token's `sub` claim,
  which Keycloak and others set to an internal id, not the login name.
  `oidc_extract_sub` reads it.
- **Access denied after a restart.** `auth_openid_connect_configuration`
  set with `SET GLOBAL` is gone after a restart, including the one after
  `--copy-back`. `oidc_backup_and_restore` sets it again.
- **Access denied over TCP.** The server plugin refuses a connection
  without TLS before asking for the token and logs `unsecure connection,
  use TLS, socket or memory`. Use the socket or `--ssl-mode=REQUIRED`.
- **`Failed to set the ID token file`.** The plugin opens the file when
  it is given the path, so a missing or unreadable file fails here,
  before the server is contacted.
- **The plugin is skipped by CMake.** It needs `WITH_XTRABACKUP` or
  `WITH_AUTHENTICATION_CLIENT_PLUGINS`. Check for
  `plugin_output_directory/authentication_openid_connect_client.so`.
