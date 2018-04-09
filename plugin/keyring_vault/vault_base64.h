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

#ifndef MYSQL_VAULT_BASE64_H
#define MYSQL_VAULT_BASE64_H

#include <my_global.h>
#include "secure_string.h"

namespace keyring
{
  class Vault_base64
  {
  public :
    enum Base64Format
    {
      SINGLE_LINE,
      MULTI_LINE
    };
    static bool encode(const void *src, size_t src_len, Secure_string *encoded, Base64Format format);
    static bool decode(const Secure_string &src, Secure_string *dst);
    // It is caller responsibility to delete memory allocated with delete[]
    static bool decode(const Secure_string &src, char **dst, uint64 *dst_length);
  };
}

#endif // MYSQL_VAULT_BASE64_H
