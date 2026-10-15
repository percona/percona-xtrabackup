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

#include <my_sys.h>
#include <mysql.h>
#include <mysql/client_plugin.h>
#include <sql_plugin.h>

#include <string>

#include "backup_oidc.h"
#include "common.h"
#include "xtrabackup.h"

namespace {

/** Name of the client plugin that sends an OpenID Connect ID token. */
const char *const oidc_plugin_name = "authentication_openid_connect_client";

/** The client plugin, or nullptr when no token file is given. Set only by
load_plugin(), so it is read without the mutex. */
struct st_mysql_client_plugin *oidc_plugin = nullptr;

/** Serializes handing the token file to the plugin and the handshake that
reads it. */
std::mutex oidc_mutex;

/** The token file path after resolve_path(). */
std::string oidc_resolved_path;

}  // namespace

namespace xb::oidc {

bool check_options(bool password_prompt) {
  if (opt_oidc_id_token_file == nullptr) {
    return (true);
  }

  if (*opt_oidc_id_token_file == '\0') {
    xb::error() << "--authentication-openid-connect-client-id-token-file "
                   "requires the name of the file holding the ID token.";
    return (false);
  }

  /* --skip-password and an empty --password name no password. */
  if (password_prompt || (opt_password != nullptr && *opt_password != '\0')) {
    xb::error() << "--password and "
                   "--authentication-openid-connect-client-id-token-file "
                   "are mutually exclusive: the ID token is the credential.";
    return (false);
  }

  return (true);
}

void resolve_path(const char *cwd) {
  if (opt_oidc_id_token_file == nullptr) {
    return;
  }

  /* Only the prefix is added: the rest of the path is passed as given, so
  that the plugin opens the file the user named, at any length. */
  const char *path = opt_oidc_id_token_file;
  if (path[0] == FN_HOMELIB && path[1] == FN_LIBCHAR) {
    if (home_dir == nullptr) {
      return;
    }
    oidc_resolved_path = std::string(home_dir) + (path + 1);
  } else if (!test_if_hard_path(path)) {
    oidc_resolved_path = std::string(cwd) + path;
  } else {
    return;
  }
  opt_oidc_id_token_file = oidc_resolved_path.data();
}

bool load_plugin() {
  ut_ad(oidc_plugin == nullptr);
  if (opt_oidc_id_token_file == nullptr) {
    return (true);
  }

  xb::info() << "Using OpenID Connect ID token file '" << opt_oidc_id_token_file
             << "' for authentication to the server.";
  xb::info() << "Loading the " << oidc_plugin_name << " plugin from '"
             << opt_plugin_dir << "' (--xtrabackup-plugin-dir).";

  /* Client plugins are registered for the whole process; the handle only
  says where to look and carries the error. */
  MYSQL *mysql = mysql_init(nullptr);
  if (mysql == nullptr) {
    xb::error() << "Failed to init MySQL struct.";
    return (false);
  }
  mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir);
  oidc_plugin = mysql_client_find_plugin(mysql, oidc_plugin_name,
                                         MYSQL_CLIENT_AUTHENTICATION_PLUGIN);
  if (oidc_plugin == nullptr) {
    xb::error() << "Cannot load the " << oidc_plugin_name
                << " plugin: " << mysql_error(mysql);
  }
  mysql_close(mysql);

  return (oidc_plugin != nullptr);
}

void release() {
  if (oidc_plugin == nullptr) {
    return;
  }

  std::lock_guard<std::mutex> lock(oidc_mutex);
  /* Without a path the plugin frees the one it has. Versions that do not
  accept a null path report an error for it, which is expected here. */
  mysql_plugin_options(oidc_plugin, "id-token-file", nullptr);
}

Connect_guard::Connect_guard() : m_lock(oidc_mutex, std::defer_lock) {}

bool Connect_guard::set_plugin_options(const char *id_token_file) {
  if (oidc_plugin == nullptr) {
    return (true);
  }

  m_lock.lock();

  if (id_token_file == nullptr) {
    /* Forget a path a previous connection gave the plugin. The 8.4 plugin
    reports an error for a null path after it has freed the old one. */
    mysql_plugin_options(oidc_plugin, "id-token-file", nullptr);
    return (true);
  }

  if (mysql_plugin_options(oidc_plugin, "id-token-file", id_token_file) != 0) {
    xb::error() << "Failed to set the ID token file '" << id_token_file
                << "' for the " << oidc_plugin_name
                << " plugin. Check that the file exists and is readable.";
    return (false);
  }

  return (true);
}

}  // namespace xb::oidc
