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

#ifndef MYSQL_I_SYSTEM_KEYS_CONTAINER_H
#define MYSQL_I_SYSTEM_KEYS_CONTAINER_H

#include "i_keyring_key.h"

namespace keyring {
class ISystem_keys_container : public Keyring_alloc {
 public:
  virtual IKey *get_latest_key_if_system_key_without_version(IKey *key) = 0;
  virtual void store_or_update_if_system_key_with_version(IKey *key) = 0;
  virtual bool rotate_key_id_if_system_key_without_version(IKey *key) = 0;
  virtual bool is_system_key(IKey *key) = 0;

  virtual ~ISystem_keys_container() {}
};
}  // namespace keyring

#endif  // MYSQL_I_SYSTEM_KEYS_CONTAINER_H
