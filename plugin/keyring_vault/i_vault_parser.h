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

#ifndef MYSQL_I_VAULT_PARSER_H
#define MYSQL_I_VAULT_PARSER_H

#include "my_global.h"
#include "i_keyring_key.h"
#include "vault_keys_list.h"
#include "logger.h"
#include "secure_string.h"

namespace keyring
{
  class IVault_parser
  {
  public:
    struct KeyParameters
    {
      Secure_string key_id;
      Secure_string user_id;
      Secure_string& operator[](uint i)
      {
        DBUG_ASSERT(i <= 1);
        return i == 0 ? key_id : user_id;
      }
    };

    virtual bool parse_keys(const Secure_string &payload, Vault_keys_list *keys) = 0;
    virtual bool parse_key_data(const Secure_string &payload, IKey *key) = 0;
    virtual bool parse_key_signature(const Secure_string &key_signature, KeyParameters *key_parameters) = 0;
    virtual bool parse_errors(const Secure_string &payload, Secure_string *errors) = 0;
    virtual ~IVault_parser() {}
  };
} // namespace keyring

#endif // MYSQL_I_VAULT_PARSER_H

