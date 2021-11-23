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

#ifndef MYSQL_I_VAULT_PARSER_COMPOSER_H
#define MYSQL_I_VAULT_PARSER_COMPOSER_H

#include "my_dbug.h"
#include "plugin/keyring/common/i_keyring_key.h"
#include "plugin/keyring/common/secure_string.h"
#include "vault_credentials.h"
#include "vault_key.h"
#include "vault_keys_list.h"

namespace keyring {
class IVault_parser_composer {
 public:
  struct KeyParameters {
    Secure_string key_id;
    Secure_string user_id;
    Secure_string &operator[](std::size_t i) {
      assert(i <= 1);
      return i == 0 ? key_id : user_id;
    }
    const Secure_string &operator[](std::size_t i) const {
      assert(i <= 1);
      return i == 0 ? key_id : user_id;
    }
  };

  virtual bool parse_keys(const Secure_string &payload,
                          Vault_keys_list *keys) = 0;
  virtual bool parse_key_data(const Secure_string &payload, IKey *key,
                              Vault_version_type vault_version) = 0;
  virtual bool parse_key_signature(const Secure_string &key_signature,
                                   KeyParameters *key_parameters) = 0;
  virtual bool parse_errors(const Secure_string &payload,
                            Secure_string *errors) = 0;
  virtual bool parse_mount_point_config(
      const Secure_string &config_payload, std::size_t &max_versions,
      bool &cas_required, Optional_secure_string &delete_version_after) = 0;
  virtual bool compose_write_key_postdata(const Vault_key &key,
                                          const Secure_string &encoded_key_data,
                                          Vault_version_type vault_version,
                                          Secure_string &postdata) = 0;
  virtual ~IVault_parser_composer() = default;
};
}  // namespace keyring

#endif  // MYSQL_I_VAULT_PARSER_COMPOSER_H
