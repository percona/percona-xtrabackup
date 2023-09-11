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

#include "vault_keys_container.h"

#include "i_vault_io.h"

namespace keyring {
bool Vault_keys_container::init(IKeyring_io *keyring_io_value,
                                std::string keyring_storage_url_value) {
  vault_io = dynamic_cast<IVault_io *>(keyring_io_value);
  assert(vault_io != nullptr);
  return Keys_container::init(keyring_io_value, keyring_storage_url_value);
}

IKey *Vault_keys_container::fetch_key(IKey *key) {
  assert(key->get_key_data() == nullptr);
  assert(key->get_key_type_as_string()->empty());

  IKey *fetched_key = get_key_from_hash(key);

  if (fetched_key == nullptr) return nullptr;

  if (fetched_key->get_key_type_as_string()->empty() &&
      vault_io->retrieve_key_type_and_data(
          fetched_key))  // key is fetched for the first time
    return nullptr;

  return Keys_container::fetch_key(key);
}

void Vault_keys_container::set_curl_timeout(uint timeout) {
  assert(vault_io != nullptr);
  vault_io->set_curl_timeout(timeout);
}

bool Vault_keys_container::flush_to_backup() { return false; }
}  // namespace keyring
