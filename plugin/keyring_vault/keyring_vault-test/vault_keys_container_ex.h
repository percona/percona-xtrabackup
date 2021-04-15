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

#ifndef MYSQL_VAULT_KEYS_CONTAINER_EX_H
#define MYSQL_VAULT_KEYS_CONTAINER_EX_H

#include <unordered_map>

#include "vault_keys_container.h"

namespace keyring {

class Vault_keys_container_ex : public Vault_keys_container {
 public:
  Vault_keys_container_ex(ILogger *logger) : Vault_keys_container(logger) {}

  void remove_all_keys() {
    std::for_each(keys_hash->begin(), keys_hash->end(),
                  [this](const auto &pair) { remove_key(pair.second.get()); });
  }
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEYS_CONTAINER_EX_H
