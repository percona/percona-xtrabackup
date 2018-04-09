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

#ifndef MYSQL_I_VAULT_CURL
#define MYSQL_I_VAULT_CURL

#include "i_keyring_key.h"
#include "vault_credentials.h"
#include "secure_string.h"

namespace keyring {

class IVault_curl : public Keyring_alloc
{
public:
  virtual bool init(const Vault_credentials &vault_credentials) = 0;

  virtual bool list_keys(Secure_string *response) = 0;
  virtual bool write_key(const Vault_key &key, Secure_string *response) = 0;
  virtual bool read_key(const Vault_key &key, Secure_string *response) = 0;
  virtual bool delete_key(const Vault_key &key, Secure_string *response) = 0;
  virtual void set_timeout(uint timeout) = 0;

  virtual ~IVault_curl() {};
};

} // namespace keyring

#endif // MYSQL_I_VAULT_CURL_H
