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

#ifndef KEYRING_VAULT_BASE64_INCLUDED
#define KEYRING_VAULT_BASE64_INCLUDED

#include "components/keyrings/common/data/pfs_string.h"

#include <memory>

namespace keyring_vault::backend {

class Vault_base64 final {
 public:
  enum class Format { SINGLE_LINE, MULTI_LINE };
  static bool encode(const void *src, size_t src_len, pfs_string *encoded,
                     Format format);
  static bool decode(const pfs_string &src, pfs_string *dst);
  // It is caller responsibility to delete memory allocated with delete[]
  static bool decode(const pfs_string &src, std::unique_ptr<char[]> &dst,
                     uint64 *dst_length);
};

}  // namespace keyring_vault::backend

#endif  // KEYRING_VAULT_BASE64_INCLUDED
