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

#include <algorithm>
#include "vault_base64.h"
#include "base64.h"
#include "boost/scoped_array.hpp"
#include "boost/move/unique_ptr.hpp"

namespace keyring
{
  bool Vault_base64::encode(const void *src, size_t src_len, Secure_string *encoded, Base64Format format)
  {
    uint64 memory_needed = base64_needed_encoded_length(src_len);
    boost::scoped_array<char> base64_encoded_text(new char[memory_needed]);
    //Using scoped_array instead of vector or string as those containers do not provide access to underlying
    //data when they are empty. Calling reserve on those containers does not help.
    if (::base64_encode(src, src_len, base64_encoded_text.get()) != 0)
    {
      memset_s(base64_encoded_text.get(), 0, memory_needed, memory_needed);
      return true;
    }
    if (format == SINGLE_LINE)
    {
      char* new_end = std::remove(base64_encoded_text.get(), base64_encoded_text.get() + memory_needed, '\n');
      memory_needed = new_end - base64_encoded_text.get();
    }
    // base64 encode below returns data with NULL terminating string - which we do not care about
    encoded->assign(base64_encoded_text.get(), memory_needed - 1); 
    memset_s(base64_encoded_text.get(), 0, memory_needed, memory_needed);

    return false;
  }

  bool Vault_base64::decode(const Secure_string &src, Secure_string *dst)
  {
    char *data;
    uint64 data_length;
    if (decode(src, &data, &data_length))
      return true;    
    dst->assign(data, data_length);
    memset_s(data, 0, data_length, data_length);
    delete[] data;
    return false;
  }

  bool Vault_base64::decode(const Secure_string &src, char **dst, uint64 *dst_length)
  {
    uint64 base64_length_of_memory_needed_for_decode = base64_needed_decoded_length(src.length());
    boost::movelib::unique_ptr<char[]> data(new char[base64_length_of_memory_needed_for_decode]);

    int64 decoded_length = ::base64_decode(src.c_str(), src.length(), data.get(), NULL, 0);
    if (decoded_length <= 0)
    { 
      memset_s(data.get(), 0, base64_length_of_memory_needed_for_decode, base64_length_of_memory_needed_for_decode);
      return true;
    }
    *dst = data.release();
    *dst_length = decoded_length;

    return false;
  }
}
