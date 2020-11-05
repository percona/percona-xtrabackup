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

#ifndef MYSQL_VAULT_CREDENTIALS
#define MYSQL_VAULT_CREDENTIALS

#include <my_global.h>
#include <map>
#include "secure_string.h"

namespace keyring {
class Vault_credentials {
 public:
  typedef std::map<Secure_string, Secure_string> Map;

  Vault_credentials() {}
  Vault_credentials(const Map &vault_credentials_map)
  {
    init(vault_credentials_map);
  }

  void init(const Map &vault_credentials)
  {
    this->vault_credentials= vault_credentials;
  }

  const Secure_string &get_credential(const Secure_string &key) const;
  Secure_string        get_raw_secret_mount_point() const;
  Secure_string        get_raw_directory() const;

 private:
  Vault_credentials::Map vault_credentials;
};
}  // namespace keyring

#endif  // MYSQL_VAULT_CREDENTIALS
