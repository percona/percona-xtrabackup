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

#include "vault_credentials.h"

namespace keyring
{

static Secure_string empty_value;

const Secure_string& get_credential(const Vault_credentials &credentials, const Secure_string &key)
{
  Vault_credentials::const_iterator it = credentials.find(key);
  if (it == credentials.end())
    return empty_value;
  else 
    return it->second;
}

}

