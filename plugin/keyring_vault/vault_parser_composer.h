/* Copyright (c) 2018, 2021 Percona LLC and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_VAULT_PARSER_COMPOSER_H
#define MYSQL_VAULT_PARSER_COMPOSER_H

#include <my_global.h>

#include "i_vault_parser_composer.h"
#include "secure_string.h"

namespace keyring {

class ILogger;

class Vault_parser_composer : public IVault_parser_composer {
 public:
  Vault_parser_composer(ILogger *logger) : logger(logger) {}

  virtual bool parse_keys(const Secure_string &payload,
                          Vault_keys_list *    keys);
  virtual bool parse_key_data(const Secure_string &payload, IKey *key,
                              Vault_version_type vault_version);
  virtual bool parse_key_signature(const Secure_string &base64_key_signature,
                                   KeyParameters *      key_parameters);
  virtual bool parse_errors(const Secure_string &payload,
                            Secure_string *      errors);

  virtual bool parse_mount_point_config(
      const Secure_string &config_payload, std::size_t &max_versions,
      bool &cas_required, Optional_secure_string &delete_version_after);
  virtual bool compose_write_key_postdata(
      const Vault_key &key, const Secure_string &encoded_key_data,
      Vault_version_type vault_version, Secure_string &postdata);

 private:
  ILogger *logger;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_PARSER_COMPOSER_H
