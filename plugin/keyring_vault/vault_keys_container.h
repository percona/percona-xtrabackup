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

#ifndef MYSQL_VAULT_KEYS_CONTAINER_H
#define MYSQL_VAULT_KEYS_CONTAINER_H

#include <my_global.h>

#include <boost/core/noncopyable.hpp>

#include "keys_container.h"

namespace keyring {

class IVault_io;

class Vault_keys_container : public Keys_container,
                             private boost::noncopyable {
 public:
  Vault_keys_container(ILogger *logger) : Keys_container(logger) {}

  my_bool init(IKeyring_io *keyring_io, std::string keyring_storage_url);
  virtual IKey *fetch_key(IKey *key);
  virtual void  set_curl_timeout(uint timeout);

 private:
  virtual my_bool flush_to_backup();
  IVault_io *     vault_io;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEYS_CONTAINER_H
