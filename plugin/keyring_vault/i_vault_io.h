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

#ifndef I_VAULTIO_INCLUDED
#define I_VAULTIO_INCLUDED

#include <my_global.h>
#include "i_keyring_io.h"

namespace keyring {

class IVault_io : public IKeyring_io
{
public:
  virtual my_bool retrieve_key_type_and_data(IKey *key) = 0;
  virtual void set_curl_timeout(uint timeout) = 0;
  
  virtual ~IVault_io() {}
};

} // namespace keyring

#endif // I_VAULTIO_INCLUDED
