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

#include "vault_keys_list.h"

namespace keyring {

// The caller takes ownership of the key, thus it is
// his resposibility to free the key
my_bool Vault_keys_list::get_next_key(IKey **key)
{
  *key= NULL;
  if (size() == 0)
    return TRUE;
  *key= keys.front();
  keys.pop_front();
  return FALSE;
}

my_bool Vault_keys_list::has_next_key() { return size() != 0; }

size_t Vault_keys_list::size() const { return keys.size(); }

Vault_keys_list::~Vault_keys_list()
{
  // remove not fetched keys
  for (Keys_list::iterator iter= keys.begin(); iter != keys.end(); ++iter)
    delete *iter;
}

void Vault_keys_list::push_back(IKey *key) { keys.push_back(key); }

}  // namespace keyring
