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

#include "vault_keys_container.h"

#include <components/keyrings/common/data/meta.h>

namespace keyring_vault::backend {

Vault_keys_container::Keys_list::iterator
Vault_keys_container::begin() noexcept {
  return m_keys.begin();
}

Vault_keys_container::Keys_list::iterator Vault_keys_container::end() noexcept {
  return m_keys.end();
}

size_t Vault_keys_container::size() const noexcept { return m_keys.size(); }

void Vault_keys_container::push_back(std::unique_ptr<Metadata> key) noexcept {
  m_keys.push_back(std::move(key));
}

}  // namespace keyring_vault::backend
