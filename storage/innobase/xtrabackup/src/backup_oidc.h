/******************************************************
Copyright (c) 2026 Percona LLC and/or its affiliates.

OpenID Connect authentication: the ID token file and the client plugin that
sends it.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/

#ifndef XTRABACKUP_BACKUP_OIDC_H
#define XTRABACKUP_BACKUP_OIDC_H

#include <mutex>

/** --authentication-openid-connect-client-id-token-file: an account
IDENTIFIED WITH auth_openid_connect logs in with an ID token read from this
file, sent by the authentication_openid_connect_client plugin.

The plugin keeps the token path in one process-wide variable, not per
connection: mysql_plugin_options() sets it for the plugin, and every handshake
reads it. Upstream's plugin frees the path after each handshake (the WL#16269
post-push fix); the 8.4 plugin (PS-11019) does not yet. So every connection
hands over its own path, or a null path when it has no token, and since
connections are opened from several threads (the redo log archive and the
query killer open their own), handing over the path and the handshake that
reads it run under one mutex: see Connect_guard. */
namespace xb::oidc {

/** Reject an empty token file name, and a password together with the token
file: a non-empty --password, or -p that would ask for one.
@param[in]	password_prompt	-p was given without a value
@return true if the options are usable */
bool check_options(bool password_prompt);

/** Expand a leading ~/ in the token file path and make a relative one
absolute, and change nothing else in it: the plugin opens the file in every
handshake, and the backup changes into the datadir before it opens its later
connections.
@param[in]	cwd	the directory xtrabackup was started in */
void resolve_path(const char *cwd);

/** Load the client plugin from --xtrabackup-plugin-dir when the token file is
given. Called once, before any connection is opened and before other threads
start; the plugin does not change after that.
@return true on success, or when no token file is given */
bool load_plugin();

/** Free any copy of the token file path that the plugin still holds: a plugin
that keeps the path between handshakes, or a path given to a connection that
never reached the handshake. xtrabackup does not call mysql_library_end(), so
the plugin is never deinitialized and would not free it itself. */
void release();

/** Hands the token file to the plugin for one connection and holds the mutex
from set_plugin_options() until it is destroyed, so it is made in a block
around mysql_real_connect(). Does nothing when the plugin is not loaded. */
class Connect_guard {
 public:
  Connect_guard();

  /** Take the mutex and hand the plugin the connection's token file, or a
  null path for a connection without one, so that it cannot send a token
  given to another connection.
  @param[in]	id_token_file	the connection's token file, or nullptr
  @return true on success */
  bool set_plugin_options(const char *id_token_file);

 private:
  std::unique_lock<std::mutex> m_lock;
};

}  // namespace xb::oidc

#endif /* XTRABACKUP_BACKUP_OIDC_H */
