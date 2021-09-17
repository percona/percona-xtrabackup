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

#ifndef MYSQL_VAULT_KEY_SERIALIZER_H
#define MYSQL_VAULT_KEY_SERIALIZER_H

#include "i_serializer.h"
#include "vault_key.h"

namespace keyring {

class Vault_key_serializer : public ISerializer {
 public:
  ISerialized_object *serialize(HASH *keys_hash, IKey *key,
                                const Key_operation operation)
  {
    Vault_key *vault_key= dynamic_cast<Vault_key *>(key);
    assert(vault_key != NULL);
    vault_key->set_key_operation(operation);

    return new Vault_key(*vault_key);
  }
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEY_SERIALIZER_H
