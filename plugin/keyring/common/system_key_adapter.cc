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

#include "system_key_adapter.h"
#include "secure_string.h"

namespace keyring
{
  // Adds key's version to keyring's key data. The resulting system key data looks like this:
  // <key_version>:<keyring key data>
  void System_key_adapter::construct_system_key_data()
  {
    Secure_ostringstream system_key_data_version_prefix_ss;
    system_key_data_version_prefix_ss << key_version << ':';
    Secure_string system_key_data_version_prefix = system_key_data_version_prefix_ss.str();
    system_key_data.allocate(system_key_data_version_prefix.length() + keyring_key->get_key_data_size());

    // need to "de"-xor keying key data to be able to add to it key version prefix
    keyring_key->xor_data();
    memcpy(system_key_data.get_key_data(), system_key_data_version_prefix.c_str(), system_key_data_version_prefix.length());
    memcpy(system_key_data.get_key_data() + system_key_data_version_prefix.length(), keyring_key->get_key_data(),
           keyring_key->get_key_data_size());

    size_t keyring_key_data_size = keyring_key->get_key_data_size();
    uchar *keyring_key_data = keyring_key->release_key_data();

    // Using keyring_key's xor function to xor system key data, next
    // restoring keyring key data
    keyring_key->set_key_data(system_key_data.get_key_data(), system_key_data.get_key_data_size());
    keyring_key->xor_data();

    keyring_key->release_key_data();
    keyring_key->set_key_data(keyring_key_data, keyring_key_data_size);

    keyring_key->xor_data();
  }

  System_key_adapter::System_key_data::System_key_data()
      : key_data(NULL)
      , key_data_size(0)
  {}

  System_key_adapter::System_key_data::~System_key_data()
  {
    free();
  }

  bool System_key_adapter::System_key_data::allocate(size_t key_data_size)
  {
    free();
    key_data = new uchar[key_data_size];
    if (key_data)
    {
      this->key_data_size = key_data_size;
      return false;
    }
    return true;
  }

  void System_key_adapter::System_key_data::free()
  {
    if (key_data)
    {
      DBUG_ASSERT(key_data_size <= 512);
      memset_s(key_data, 512, 0, key_data_size);
      delete[] key_data;
      key_data = NULL;
      key_data_size = 0;
    }
  }

  uchar* System_key_adapter::System_key_data::get_key_data()
  {
    return key_data;
  }

  size_t System_key_adapter::System_key_data::get_key_data_size()
  {
    return key_data_size;
  }
} //namespace keyring
