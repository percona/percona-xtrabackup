/* Copyright (c) 2021, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cassert>
#include <fstream>
#include <memory>

#include "backend.h"
#include "my_dbug.h"

#include <mysql/components/minimal_chassis.h>

#include <components/keyrings/common/data/data_extension.h>
#include <components/keyrings/common/memstore/cache.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/utils/utils.h>

namespace keyring_kmip {

namespace backend {

using keyring_common::data::Data;
using keyring_common::data::Data_extension;
using keyring_common::meta::Metadata;
using keyring_common::utils::get_random_data;

Keyring_kmip_backend::Keyring_kmip_backend(config::Config_pod const &config)
    : valid_(false), config_(config) {
  DBUG_TRACE;
  valid_ = true;
}

bool Keyring_kmip_backend::load_cache(
    keyring_common::operations::Keyring_operations<
        Keyring_kmip_backend, keyring_common::data::Data_extension<IdExt>>
        &operations) {
  DBUG_TRACE;
  try {
    auto ctx = kmip_ctx();

    auto keys = (config_.object_group.empty()
                     ? ctx.op_all()
                     : ctx.op_locate_by_group(config_.object_group));

    for (auto const &id : keys) {
      auto key = ctx.op_get(id);
      auto key_name = ctx.op_get_name_attr(id);

      if (key_name.empty()) continue;

      Metadata metadata(key_name, "");

      Data_extension<IdExt> data(
          Data{keyring_common::data::Sensitive_data(
                   reinterpret_cast<char *>(key.data()), key.size()),
               "AES"},
          IdExt{id});

      if (operations.insert(metadata, data) == true) {
        return true;
      }
    }

  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }

  return false;
}

bool Keyring_kmip_backend::get(const Metadata &, Data &) const {
  /* Shouldn't have reached here if we cache things. */
  assert(0);
  DBUG_TRACE;
  return false;
}

bool Keyring_kmip_backend::store(const Metadata &metadata,
                                 Data_extension<IdExt> &data) {
  DBUG_TRACE;
  if (!metadata.valid() || !data.valid()) return true;
  if (data.type() != "AES") {
    // we only support AES keys
    return true;
  }
  try {
    auto ctx = kmip_ctx();
    auto key = data.data().decode();
    kmippp::context::key_t keyv(key.begin(), key.end());
    auto id = ctx.op_register(metadata.key_id(), config_.object_group, keyv);
    if (id.empty()) {
      return true;
    }
    data.set_extension({id});
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

size_t Keyring_kmip_backend::size() const {
  try {
    auto ctx = kmip_ctx();

    auto keys = (config_.object_group.empty()
                     ? ctx.op_all()
                     : ctx.op_locate_by_group(config_.object_group));

    return keys.size();
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return 0;
  }
}

bool Keyring_kmip_backend::erase(const Metadata &metadata,
                                 Data_extension<IdExt> &data) {
  DBUG_TRACE;
  if (!metadata.valid()) return true;

  auto ctx = kmip_ctx();
  return !ctx.op_destroy(data.get_extension().uuid);
}

bool Keyring_kmip_backend::generate(const Metadata &metadata,
                                    Data_extension<IdExt> &data,
                                    size_t length) {
  DBUG_TRACE;
  if (!metadata.valid()) return true;

  std::unique_ptr<unsigned char[]> key(new unsigned char[length]);
  if (!key) return true;
  if (!get_random_data(key, length)) return true;

  pfs_string key_str;
  key_str.assign(reinterpret_cast<const char *>(key.get()), length);
  Data inner_data = data.get_data();
  inner_data.set_data(keyring_common::data::Sensitive_data{key_str});
  data.set_data(inner_data);

  return store(metadata, data);
}

kmippp::context Keyring_kmip_backend::kmip_ctx() const {
  return kmippp::context(config_.server_addr, config_.server_port,
                         config_.client_ca, config_.client_key,
                         config_.server_ca);
}

}  // namespace backend

}  // namespace keyring_kmip
