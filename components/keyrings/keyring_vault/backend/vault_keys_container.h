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

#ifndef KEYRING_VAULT_KEYS_CONTAINER_INCLUDED
#define KEYRING_VAULT_KEYS_CONTAINER_INCLUDED

#include <components/keyrings/common/data/pfs_string.h>

#include <boost/core/noncopyable.hpp>

#include <list>
#include <memory>

namespace keyring_common::meta {
class Metadata;
}  // namespace keyring_common::meta

namespace keyring_vault::backend {

using keyring_common::meta::Metadata;

class Vault_keys_container : private boost::noncopyable {
  typedef std::list<std::unique_ptr<Metadata>> Keys_list;

 public:
  Keys_list::iterator begin() noexcept;
  Keys_list::iterator end() noexcept;

  void push_back(std::unique_ptr<Metadata> key) noexcept;
  size_t size() const noexcept;

 private:
  Keys_list m_keys;
};

}  // namespace keyring_vault::backend

#endif  // KEYRING_VAULT_KEYS_CONTAINER_INCLUDED
