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

#ifndef MYSQL_VAULT_KEYS_CONTAINER_H
#define MYSQL_VAULT_KEYS_CONTAINER_H

#include <boost/core/noncopyable.hpp>
#include "plugin/keyring/common/keys_container.h"

namespace keyring {

class IVault_io;

class Vault_keys_container final : public Keys_container,
                                   private boost::noncopyable {
 public:
  Vault_keys_container(ILogger *logger) noexcept : Keys_container(logger) {}

  bool init(IKeyring_io *keyring_io, std::string keyring_storage_url) override;
  IKey *fetch_key(IKey *key) override;
  virtual void set_curl_timeout(uint timeout);

 private:
  virtual bool flush_to_backup() override;
  IVault_io *vault_io;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEYS_CONTAINER_H
