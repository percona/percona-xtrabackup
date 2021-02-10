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

#ifndef MYSQL_VAULT_KEYS_H
#define MYSQL_VAULT_KEYS_H

#include <my_global.h>

#include <list>

#include <boost/core/noncopyable.hpp>

#include "i_serialized_object.h"

namespace keyring {

class Vault_keys_list : public ISerialized_object,
                        private boost::noncopyable {
 public:
  virtual my_bool get_next_key(IKey **key);
  virtual my_bool has_next_key();
  void            push_back(IKey *key);
  size_t          size() const;

  ~Vault_keys_list();

 private:
  typedef std::list<IKey *> Keys_list;
  Keys_list                 keys;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEYS_H
