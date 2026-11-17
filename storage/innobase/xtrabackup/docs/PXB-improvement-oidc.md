# PXB-3849: OpenID Connect authentication in Percona XtraBackup

Design document. The notes for maintainers are in `oidc.md`.

## 1. Goal

Percona Server 8.4.10 and later ship the `auth_openid_connect` server
plugin. An account created with

```sql
CREATE USER alice IDENTIFIED WITH 'auth_openid_connect'
  AS '{"identity_provider": "my-idp", "user": "<sub claim>"}';
```

logs in with a signed ID token (a JWT) issued by an identity provider,
not with a password. The `mysql` client does this through the
`authentication_openid_connect_client` client plugin and the
`--authentication-openid-connect-client-id-token-file` option.

xtrabackup could not log in as such an account, so a server whose
backup account uses OIDC could not be backed up. After this change:

```
xtrabackup --backup --user=alice \
    --authentication-openid-connect-client-id-token-file=/path/to/token \
    --target-dir=/backup/full
```

Out of scope: the server plugin (Percona Server), getting a token from
the identity provider (the operator's job, with whatever flow the
provider requires), and refreshing a token during a long backup.

## 2. User interface

### 2.1 New option

`--authentication-openid-connect-client-id-token-file=PATH`: the file
that holds the ID token. Same name as the `mysql` client option. A
relative path is taken from the directory xtrabackup is started in, and
a leading `~/` is expanded to the home directory. The rest of the path is
used as given. An empty value is an error.

| Option | Interaction |
|---|---|
| `--password` | Mutually exclusive. Both set is an error before anything is copied, and `-p` is refused before it asks for the password. `--skip-password` names no password and is accepted. |
| `--xtrabackup-plugin-dir` | Where `authentication_openid_connect_client.so` is loaded from, like the keyring components. Defaults to the compile-time plugin directory, `/usr/lib64/xtrabackup/plugin` or `/usr/lib/xtrabackup/plugin` in the packages. |
| `--history-user`, `--history-password` | The history server connection inherits the token as it inherits the password: only when `--history-user` is not named, and not when `--history-password` is. There is no separate token file for the history server; see 4.4. |
| `--host` / `--socket` / `--ssl-mode` | The token is sent only over a socket, shared memory or TLS. Both plugins refuse plain TCP. |

### 2.2 Files shipped

`authentication_openid_connect_client.so` in the xtrabackup plugin
directory of the RPM and DEB packages and the tarball.

### 2.3 Log

Before the first connection, the backup logs the token file and where the
plugin is loaded from; every connection that uses the token then says so
in its connect line:

```
Using OpenID Connect ID token file '<path>' for authentication to the server.
Loading the authentication_openid_connect_client plugin from '<dir>' (--xtrabackup-plugin-dir).
Connecting to MySQL server host: <h>, user: <u>, password: not set (using OpenID Connect ID token), port: <p>, socket: <s>, purpose: <purpose>
```

### 2.4 Errors

| Condition | Message |
|---|---|
| `--password` and the token option | `--password and --authentication-openid-connect-client-id-token-file are mutually exclusive: the ID token is the credential.` |
| Plugin not in the plugin directory | `Cannot load the authentication_openid_connect_client plugin: <reason>` |
| Token file does not exist or cannot be read | `Failed to set the ID token file '<path>' for the authentication_openid_connect_client plugin. Check that the file exists and is readable.` |
| File does not hold a JWT | Plugin: `The id token file: <path> does not contain a valid JWT.`, then `Access denied` |
| Expired token, untrusted key, wrong subject, plain TCP | `Access denied for user ...`; the server log says why |

## 3. Requirements

Functional:

- FR1. Backup, prepare and copy-back succeed as an OIDC account, and the
  restored data matches the source.
- FR2. A missing, malformed, expired or untrusted token fails the backup
  cleanly. Missing and malformed tokens fail before the server sees one.
- FR3. `--password` with the token option is an error at option time.
- FR4. Every connection to the server being backed up uses the token:
  backup, MDL lock, query killer, redo log archive, redo log consumer.
- FR5. `--xtrabackup-plugin-dir` chooses the client plugin.
- FR6. `xtrabackup --help` documents the option.

Non-functional:

- NFR1. The client plugin is a loadable `.so`, never linked into
  xtrabackup, so a fix can ship by replacing the file.
- NFR2. The plugin is Percona Server's, taken as is (PS-11019), so a
  copy from a Percona Server package works too.
- NFR3. The token never crosses the network in clear text.
- NFR4. Tests skip, and do not fail, when their prerequisites are missing.
- NFR5. The RPM and DEB packages ship the `.so`.
- NFR6. No new runtime dependency for xtrabackup.

## 4. Design

### 4.1 Parts

```
xtrabackup                       libmysqlclient (in xtrabackup)
  --authentication-openid-        mysql_options(MYSQL_PLUGIN_DIR)
    connect-client-id-token-file  mysql_client_find_plugin()  --dlopen-->  authentication_openid_connect_client.so
  xb::connect()                   mysql_plugin_options("id-token-file")         reads the file, checks the JWT shape,
                                  mysql_real_connect()  <------ handshake ----> checks the transport, sends the token
                                                                                          |
                                                              Percona Server: auth_openid_connect verifies
                                                              signature, issuer, audience, expiry and subject
```

### 4.2 What comes from Percona Server

PS-11019 [8.4] ("Client side OIDC authentication plugin") is
cherry-picked unchanged. It brings:

- `libmysql/authentication_openid_connect_client/`: the plugin, its
  CMake file and a version script that hides everything but
  `_mysql_client_plugin_declaration_`.
- `MYSQL_PLUGIN_VIO_INFO::is_tls_established`, set by libmysqlclient on
  TLS connections, and the matching `.pp` files for `abi_check`. The
  plugin uses it to refuse plain TCP.
- The move of the base64 helpers from `sql-common/oci` to `mysys`, which
  the plugin links.
- The `mysql` client option, `mysql_test_jwt_generator` lookup in MTR:
  harmless in PXB.

On 9.7 and trunk the same plugin comes from upstream MySQL, so nothing is
cherry-picked there.

### 4.3 What PXB adds

- The plugin is built whenever xtrabackup is, not only with
  `WITH_AUTHENTICATION_CLIENT_PLUGINS` (off in PXB builds), and the RPM
  lists it.
- The option, and `src/backup_oidc.{h,cc}`: the plugin is loaded once
  from `--xtrabackup-plugin-dir` before the first connection;
  `xb::connect()` hands it the token file and connects.
- The ID token file is handed to the plugin before every connection,
  and that and the handshake run under one mutex. The plugin keeps the
  path in one process-wide variable (the option belongs to the plugin,
  not to a connection), upstream's plugin on 9.7 and trunk frees it after
  each handshake (the WL#16269 post-push fix), and xtrabackup opens
  connections from several threads. The 8.4 plugin (PS-11019) does not
  free it yet, but is expected to in a later Percona Server 8.4 release,
  so 8.4 uses the same code. A connection without a token, such as a
  history connection that does not inherit it, hands the plugin a null
  path, so it cannot send the backup's token. At the end the plugin is
  given a null path so that it frees any copy it still holds.

### 4.4 Limitation: no separate token for the history server

The history connection uses the main token or a password. An OIDC
account on a history server that needs a different token (another
identity provider or audience) is not supported. Adding it means a
`--history-...-id-token-file` option, and setting the plugin's path
before the history connection: the path is already handed over before
every connection, under the mutex.

### 4.5 Shared library, not built in

The plugin could be linked into libmysqlclient as a built-in (like the
Windows native authentication plugin) or loaded as a `.so` (like every
other client authentication plugin). It is a `.so`:

1. It is the shape Percona Server and MySQL build and test, so the file
   is the same one they ship.
2. A fix to the plugin ships by replacing a file.
3. No change to libmysqlclient's built-in plugin table, so no PXB-only
   code in `sql-common/client.cc`.

The cost is that the file must be found at run time, which
`--xtrabackup-plugin-dir` already does for the keyring components.

### 4.6 Tests

`storage/innobase/xtrabackup/test/suites/oidc/`, in run.sh's default
suite list:

| Test | Token | Runs where |
|---|---|---|
| `oidc_jwt` | Signed by the test with a key it makes; the server is told to trust that key | Any server that ships `auth_openid_connect.so` (the Percona Server tarballs do). Covers FR1 to FR5. |
| `oidc_keycloak` | Fetched by the test from the Keycloak at `KEYCLOAK_URL`, or from one it starts in Docker (`OIDC_BOOTSTRAP_KEYCLOAK=1`) | Jenkins, which starts a Keycloak container for Percona Server targets; Docker; or any provisioned Keycloak |
| `oidc_percona` | Percona's Keycloak, or another with the same realm (`OIDC_IDP_HOST`) | The VPN |

The tests skip against MySQL (no server plugin), without the identity
provider, or without openssl and perl.

### 4.7 Risks

| Risk | Mitigation |
|---|---|
| A different copy of the plugin is loaded | The log names the directory it is loaded from. |
| The token expires during a long backup | Every connection is opened at the start of the backup, except the query killer's. Use a token that outlives the backup. |
| Docker or the VPN is missing in CI | `oidc_jwt` needs neither. Jenkins starts its own Keycloak for `oidc_keycloak`; `oidc_percona` skips without the VPN. |

## 5. Acceptance

1. Merged to 8.4, 9.7 and trunk.
2. The RPM and DEB packages ship `authentication_openid_connect_client.so`.
3. The oidc suite passes on Jenkins.
4. `xtrabackup --help | grep openid` shows the option.
5. The PXB documentation and release notes describe the option.

## References

- PS-11019 [8.4]: Client side OIDC authentication plugin (percona-server 20e8480fd8e)
- PS-10999 [8.4]: OIDC Authentication for Percona Server, PS PR #5941
- OpenID Connect Core 1.0, RFC 7519 (JWT)
