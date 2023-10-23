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

#ifndef KEYRING_VAULT_PARSER_COMPOSERO_INCLUDED
#define KEYRING_VAULT_PARSER_COMPOSERO_INCLUDED

#include "components/keyrings/keyring_vault/config/config.h"

#include <components/keyrings/common/data/pfs_string.h>

#include <memory>

namespace keyring_common {

namespace data {
class Data;
}  // namespace data

namespace meta {
class Metadata;
}  // namespace meta

}  // namespace keyring_common

namespace keyring_vault::backend {

class Vault_keys_container;

using keyring_common::data::Data;
using keyring_common::meta::Metadata;
using keyring_vault::config::Vault_version_type;

enum class ParseStatus { Ok, Fail, DataDeleted };

class Keyring_vault_parser_composer final {
 public:
  static bool parse_keys(const pfs_string &payload, Vault_keys_container *keys);
  static ParseStatus parse_key_data(const pfs_string &payload, Data *key,
                                    Vault_version_type vault_version);
  static bool parse_key_signature(const pfs_string &base64_key_signature,
                                  std::unique_ptr<Metadata> &key);
  static bool parse_errors(const pfs_string &payload, pfs_string *errors);

  static bool parse_mount_point_config(
      const pfs_string &config_payload, std::size_t &max_versions,
      bool &cas_required, pfs_optional_string &delete_version_after);
  static bool compose_write_key_postdata(const Data &data,
                                         const pfs_string &encoded_key_data,
                                         Vault_version_type vault_version,
                                         pfs_string &postdata);
};

}  // namespace keyring_vault::backend

#endif  // KEYRING_VAULT_PARSER_COMPOSERO_INCLUDED
