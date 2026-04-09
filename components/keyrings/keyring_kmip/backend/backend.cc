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

#include <mysql/components/minimal_chassis.h>

#include <components/keyrings/common/data/data_extension.h>
#include <components/keyrings/common/memstore/cache.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/utils/utils.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

namespace keyring_kmip {

namespace backend {

using keyring_common::data::Data;
using keyring_common::data::Data_extension;
using keyring_common::meta::Metadata;
using keyring_common::utils::get_random_data;

Keyring_kmip_backend::Keyring_kmip_backend(config::Config_pod const &config)
    : valid_(false), config_(config) {
  // check network connection before declaring valid
  try {
    auto ctx = kmip_ctx();
    valid_ = true;
  } catch (std::exception const &e) {
    valid_ = false;
    std::string err_msg =
        "Can not connect to KMIP server. Config: " + config_.server_addr + " " +
        config_.server_port + " " + config_.client_ca + " " +
        config_.client_key + " " + config_.server_ca + " " +
        "Exception:" + e.what();
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
  }
}

bool Keyring_kmip_backend::load_cache(
    keyring_common::operations::Keyring_operations<
        Keyring_kmip_backend, keyring_common::data::Data_extension<IdExt>>
        &operations) {
  // We have to load keys and secrets with state==ACTIVE only
  // TODO: implement better logic with the new KMIP library
  try {
    auto ctx = kmip_ctx();
    // get all keys in the group
    auto keys = (config_.object_group.empty()
                     ? ctx.op_all()
                     : ctx.op_locate_by_group(config_.object_group));

    for (auto const &id : keys) {
      auto key = ctx.op_get(id);
      if (key.empty()) {
        std::string err_msg = "Cannot get key with ID: " + id +
                              " Cause: " + ctx.get_last_result();
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        continue;
      }
      auto key_name = ctx.op_get_name_attr(id);
      if (key_name.empty()) {
        std::string err_msg = "Cannot get key name for ID: " + id +
                              "Cause: " + ctx.get_last_result();
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        continue;
      }

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
    // get all secrets in the group
    auto secrets = (config_.object_group.empty()
                        ? ctx.op_all_secrets()
                        : ctx.op_locate_secrets_by_group(config_.object_group));

    for (auto const &id : secrets) {
      auto secret = ctx.op_get_secret(id);
      if (secret.empty()) {
        std::string err_msg = "Cannot get secret with ID: " + id +
                              " Cause: " + ctx.get_last_result();
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        continue;
      }
      auto secret_name = ctx.op_get_name_attr(id);

      if (secret_name.empty()) {
        std::string err_msg = "Cannot get secret name for ID: " + id +
                              " Cause: " + ctx.get_last_result();
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        continue;
      }

      Metadata metadata(secret_name, "");

      Data_extension<IdExt> data(Data{keyring_common::data::Sensitive_data(
                                          secret.c_str(), secret.size()),
                                      "SECRET"},
                                 IdExt{id});

      if (operations.insert(metadata, data) == true) {
        return true;
      }
    }
  } catch (const std::exception &e) {
    std::string err_msg = std::string("std exception in function '") +
                          __func__ + "': " + e.what();
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    return true;
  } catch (...) {
    std::string err_msg =
        std::string("Unknown exception in function '") + __func__ + '\'';
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    return true;
  }

  return false;
}

bool Keyring_kmip_backend::get(const Metadata &, Data &) const {
  /* Shouldn't have reached here if we cache things. */
  assert(0);
  return false;
}

bool Keyring_kmip_backend::store(const Metadata &metadata,
                                 Data_extension<IdExt> &data) {
  if (!metadata.valid() || !data.valid()) return true;
  kmippp::context::id_t id;
  try {
    auto ctx = kmip_ctx();
    auto key = data.data().decode();
    if (data.type() == "AES") {
      kmippp::context::key_t keyv(key.begin(), key.end());
      id = ctx.op_register(metadata.key_id(), config_.object_group, keyv);
      if (id.empty()) {
        std::string err_msg =
            "Cannot register key with name: " + metadata.key_id() +
            " and group: " + config_.object_group + ctx.get_last_result();
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        return true;
      }
    } else if (data.type() == "SECRET") {
      kmippp::context::name_t secret(key);
      id = ctx.op_register_secret(metadata.key_id(), config_.object_group,
                                  secret, 1);
      if (id.empty()) {
        std::string err_msg =
            "Cannot register secret with name: " + metadata.key_id() +
            " and group: " + config_.object_group + ctx.get_last_result();
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        return true;
      }
    } else {  // we only support AES keys and SECRET type (passwords)
      std::string err_msg = "Unsupported KMIP entity";
      err_msg += data.type();
      err_msg += ", can not store";
      LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
      return true;
    }
    if (!ctx.op_activate(id)) {
      std::string err_msg =
          "Cannot activate key/secret. " + ctx.get_last_result();
      LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
      return true;
    }
    data.set_extension({id});
  } catch (const std::exception &e) {
    std::string err_msg = std::string("std exception in function '") +
                          __func__ + "': " + e.what();
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    return true;
  } catch (...) {
    std::string err_msg =
        std::string("Unknown exception in function '") + __func__ + '\'';
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
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
    auto secrets = (config_.object_group.empty()
                        ? ctx.op_all_secrets()
                        : ctx.op_locate_secrets_by_group(config_.object_group));
    return keys.size() + secrets.size();
    // we may have deactivated keys counted, so we need to count active keys
    // only
    // TODO: implement better logic with the new KMIP library
  } catch (const std::exception &e) {
    std::string err_msg = std::string("std exception in function '") +
                          __func__ + "': " + e.what();
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    return 0;
  } catch (...) {
    std::string err_msg =
        std::string("Unknown exception in function '") + __func__ + '\'';
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    return 0;
  }
}

bool Keyring_kmip_backend::erase(const Metadata &metadata,
                                 Data_extension<IdExt> &data) {
  if (!metadata.valid()) return true;

  auto ctx = kmip_ctx();
  // reason 1 means deactivate, and then incident occurrence time should be 0.
  if (!ctx.op_revoke(data.get_extension().uuid, 1, "Deleting the key", 0)) {
    std::string err_msg =
        "Cannot deactivate key/secret with ID: " + data.get_extension().uuid +
        " Cause: " + ctx.get_last_result();
    LogComponentErr(WARNING_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    // no reason to fail here, if we're deactivating non-exiting key
    // TODO: implement better logic with the new KMIP library
  }

  if (!ctx.op_destroy(data.get_extension().uuid)) {
    std::string err_msg = "Cannot delete key/secret. " + ctx.get_last_result();
    LogComponentErr(WARNING_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    // no reason to fail here, if we're deleting non-exiting key
    // TODO: implement better logic with the new KMIP library
  }
  return false;
}

bool Keyring_kmip_backend::generate(const Metadata &metadata,
                                    Data_extension<IdExt> &data,
                                    size_t length) {
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
