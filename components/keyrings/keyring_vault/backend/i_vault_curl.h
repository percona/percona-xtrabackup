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
     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef KEYRING_I_VAULT_CURL_INCLUDED
#define KEYRING_I_VAULT_CURL_INCLUDED

#include <components/keyrings/common/data/data.h>
#include <components/keyrings/common/data/keyring_alloc.h>
#include <components/keyrings/common/data/meta.h>
#include <components/keyrings/common/data/pfs_string.h>
#include <components/keyrings/keyring_vault/config/config.h>

#include <boost/core/noncopyable.hpp>

namespace keyring_vault::backend {

using keyring_common::data::Comp_keyring_alloc;
using keyring_common::data::Data;
using keyring_common::meta::Metadata;
using keyring_vault::config::Vault_version_type;

class IKeyring_vault_curl : public Comp_keyring_alloc,
                            private boost::noncopyable {
 public:
  virtual bool init() = 0;
  virtual bool list_keys(pfs_string *response) = 0;
  virtual bool write_key(const Metadata &key, const Data &data,
                         pfs_string *response) = 0;
  virtual bool read_key(const Metadata &key, pfs_string *response) = 0;
  virtual bool delete_key(const Metadata &key, pfs_string *response) = 0;
  virtual Vault_version_type get_resolved_secret_mount_point_version()
      const = 0;

  virtual ~IKeyring_vault_curl() {}
};

}  // namespace keyring_vault::backend

#endif  // KEYRING_I_VAULT_CURL_INCLUDED
