/* Copyright (c) 2018, 2021 Percona LLC and/or its affiliates. All rights
   reserved.

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

#include "plugin/keyring/common/secure_string.h"

namespace keyring {
enum Vault_version_type {
  Vault_version_unknown,
  Vault_version_v1,
  Vault_version_v2,
  Vault_version_auto
};

class Vault_credentials {
  friend class Vault_credentials_parser;

 public:
  Vault_credentials()
      : vault_url_(),
        secret_mount_point_(),
        vault_ca_(),
        token_(),
        secret_mount_point_version_(Vault_version_unknown) {}

  bool is_initialized() const { return !vault_url_.empty(); }

  const Secure_string &get_vault_url() const { return vault_url_; }
  const Secure_string &get_secret_mount_point() const {
    return secret_mount_point_;
  }
  const Secure_string &get_vault_ca() const { return vault_ca_; }
  const Secure_string &get_token() const { return token_; }
  Vault_version_type get_secret_mount_point_version() const {
    return secret_mount_point_version_;
  }

  void swap(Vault_credentials &obj);

 private:
  Vault_credentials(const Secure_string &vault_url,
                    const Secure_string &secret_mount_point,
                    const Secure_string &vault_ca, const Secure_string &token,
                    Vault_version_type secret_mount_point_version)
      : vault_url_(vault_url),
        secret_mount_point_(secret_mount_point),
        vault_ca_(vault_ca),
        token_(token),
        secret_mount_point_version_(secret_mount_point_version) {}

  Secure_string vault_url_;
  Secure_string secret_mount_point_;
  Secure_string vault_ca_;
  Secure_string token_;
  Vault_version_type secret_mount_point_version_;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_CREDENTIALS
