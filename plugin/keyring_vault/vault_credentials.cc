/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "vault_credentials.h"

namespace keyring {

static const Secure_string empty_value;
static const char *const secret_mount_point_key = "secret_mount_point";

const Secure_string &Vault_credentials::get_credential(
    const Secure_string &key) const {
  auto it = vault_credentials.find(key);
  if (it == vault_credentials.end())
    return empty_value;
  else
    return it->second;
}

Secure_string Vault_credentials::get_raw_secret_mount_point() const {
  Secure_string secret_mount_point_path =
      get_credential(secret_mount_point_key);
  std::size_t slash_separator = secret_mount_point_path.find('/');
  return slash_separator != std::string::npos
             ? secret_mount_point_path.substr(0, slash_separator)
             : secret_mount_point_path;
}

Secure_string Vault_credentials::get_raw_directory() const {
  Secure_string secret_mount_point_path =
      get_credential(secret_mount_point_key);
  std::size_t slash_separator = secret_mount_point_path.find('/');
  return slash_separator != std::string::npos &&
                 (slash_separator + 1) < secret_mount_point_path.length()
             ? secret_mount_point_path.substr(slash_separator + 1)
             : "";
}

}  // namespace keyring
