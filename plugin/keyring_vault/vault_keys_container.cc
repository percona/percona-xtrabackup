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

#include "vault_keys_container.h"

#include <my_global.h>

#include "i_vault_io.h"

namespace keyring {
my_bool Vault_keys_container::init(IKeyring_io *keyring_io,
                                   std::string  keyring_storage_url)
{
  vault_io= dynamic_cast<IVault_io *>(keyring_io);
  assert(vault_io != NULL);
  return Keys_container::init(keyring_io, keyring_storage_url);
}

IKey *Vault_keys_container::fetch_key(IKey *key)
{
  assert(key->get_key_data() == NULL);
  assert(key->get_key_type()->empty());

  IKey *fetched_key= get_key_from_hash(key);

  if (fetched_key == NULL)
    return NULL;

  if (fetched_key->get_key_type()->empty() &&
      vault_io->retrieve_key_type_and_data(
          fetched_key))  // key is fetched for the first time
    return NULL;

  return Keys_container::fetch_key(key);
}

void Vault_keys_container::set_curl_timeout(uint timeout)
{
  assert(vault_io != NULL);
  vault_io->set_curl_timeout(timeout);
}


my_bool Vault_keys_container::flush_to_backup() { return FALSE; }
}  // namespace keyring
