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

#include <fstream>
#include <memory>
#include <stdexcept>

#include "backend.h"
#include "kmipclient/Kmip.hpp"

#include <mysql/components/minimal_chassis.h>

#include <components/keyrings/common/data/data_extension.h>
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
    : valid_(false),
      config_(config),
      kmip_client_(nullptr),
      expected_cacheable_size_(std::nullopt) {
  std::string missing_options;
  if (!validate_required_config(config_, missing_options)) {
    std::string err_msg =
        "Can not initialize KMIP backend. Missing required config options: " +
        missing_options;
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    valid_ = false;
    return;
  }

  // check network connection before declaring valid
  try {
    (void)get_kmip_client();
    valid_ = true;
  } catch (std::exception const &e) {
    valid_ = false;
    std::string err_msg =
        "Can not connect to KMIP server. Config: " + config_.server_addr + " " +
        config_.server_port + " " + config_.client_ca + " " +
        config_.client_key + " " + config_.server_ca + " " +
        "Exception: " + e.what();
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
  }
}

Keyring_kmip_backend::~Keyring_kmip_backend() = default;

bool Keyring_kmip_backend::is_empty_or_whitespace(const std::string &value) {
  return value.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
}

bool Keyring_kmip_backend::validate_required_config(
    const config::Config_pod &config, std::string &missing_options) {
  struct Required_option {
    const char *name;
    const std::string &value;
  };

  const Required_option required_options[] = {
      {"server_addr", config.server_addr}, {"server_port", config.server_port},
      {"client_ca", config.client_ca},     {"client_key", config.client_key},
      {"server_ca", config.server_ca},
  };

  missing_options.clear();
  for (const auto &option : required_options) {
    if (!is_empty_or_whitespace(option.value)) continue;
    if (!missing_options.empty()) missing_options += ", ";
    missing_options += option.name;
  }

  return missing_options.empty();
}

bool Keyring_kmip_backend::load_cache(
    keyring_common::operations::Keyring_operations<
        Keyring_kmip_backend, keyring_common::data::Data_extension<IdExt>>
        &operations) {
  try {
    auto &kmip = get_kmip_client();
    reset_expected_cacheable_size();

    auto cached_ids = snapshot_cached_object_ids();
    auto key_ids = cached_ids.key_ids;
    auto secret_ids = cached_ids.secret_ids;

    // Use cached IDs if available; otherwise fetch fresh ones
    if (key_ids.empty() && secret_ids.empty()) {
      key_ids = get_object_ids(
          kmip, kmipclient::object_type::KMIP_OBJTYPE_SYMMETRIC_KEY);
      secret_ids = get_object_ids(
          kmip, kmipclient::object_type::KMIP_OBJTYPE_SECRET_DATA);
      update_cached_object_ids(key_ids, secret_ids);
    }

    set_expected_cacheable_size(key_ids.size() + secret_ids.size());

    for (auto const &id : key_ids) {
      try {
        auto key_obj = with_kmip_operation(
            [&kmip, &id]() { return kmip.client().op_get_key(id, true); });

        auto key = key_obj->value();
        if (key.empty()) {
          std::string err_msg = "Cannot get key value for ID: " + id;
          LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
          decrement_expected_cacheable_size();
          continue;
        }

        auto key_name =
            key_obj->attribute_value(kmipclient::KMIP_ATTR_NAME_NAME);
        if (key_name.empty()) {
          std::string err_msg = "Cannot get key name for ID: " + id;
          LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
          decrement_expected_cacheable_size();
          continue;
        }

        Metadata metadata(key_name, "");

        Data_extension<IdExt> data(
            Data{keyring_common::data::Sensitive_data(
                     reinterpret_cast<const char *>(key.data()), key.size()),
                 "AES"},
            IdExt{id});

        if (operations.insert(metadata, data)) {
          decrement_expected_cacheable_size();
          continue;
        }
      } catch (const std::exception &e) {
        std::string err_msg =
            std::string("Error loading key ") + id + ": " + e.what();
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        decrement_expected_cacheable_size();
        continue;
      }
    }

    for (auto const &id : secret_ids) {
      try {
        auto secret_obj = with_kmip_operation(
            [&kmip, &id]() { return kmip.client().op_get_secret(id); });

        auto secret = secret_obj.value();
        if (secret.empty()) {
          std::string err_msg = "Cannot get secret value for ID: " + id;
          LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
          decrement_expected_cacheable_size();
          continue;
        }

        auto secret_name =
            secret_obj.attribute_value(kmipclient::KMIP_ATTR_NAME_NAME);
        if (secret_name.empty()) {
          std::string err_msg = "Cannot get secret name for ID: " + id;
          LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
          decrement_expected_cacheable_size();
          continue;
        }

        Metadata metadata(secret_name, "");

        Data_extension<IdExt> data(
            Data{keyring_common::data::Sensitive_data(
                     reinterpret_cast<const char *>(secret.data()),
                     secret.size()),
                 "SECRET"},
            IdExt{id});

        if (operations.insert(metadata, data)) {
          decrement_expected_cacheable_size();
          continue;
        }
      } catch (const std::exception &e) {
        std::string err_msg =
            std::string("Error loading secret ") + id + ": " + e.what();
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        decrement_expected_cacheable_size();
        continue;
      }
    }

    // Dispose of cached IDs after loading is complete
    clear_cached_object_ids();
  } catch (const std::exception &e) {
    reset_expected_cacheable_size();
    clear_cached_object_ids();
    std::string err_msg = std::string("std exception in function '") +
                          __func__ + "': " + e.what();
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    return true;
  } catch (...) {
    reset_expected_cacheable_size();
    clear_cached_object_ids();
    std::string err_msg =
        std::string("Unknown exception in function '") + __func__ + '\'';
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
    return true;
  }

  return false;
}

bool Keyring_kmip_backend::get(const Metadata &, Data &) const {
  // This backend is cache-backed; direct fetch is treated as a miss.
  return true;
}

bool Keyring_kmip_backend::store(const Metadata &metadata,
                                 Data_extension<IdExt> &data) {
  if (!metadata.valid() || !data.valid()) return true;

  try {
    auto &kmip = get_kmip_client();
    auto key = data.data().decode();
    std::string id;

    if (data.type() == "AES") {
      // Register and activate AES key
      auto key_vec = std::vector<unsigned char>(key.begin(), key.end());
      auto sym_key = kmipclient::SymmetricKey::aes_from_value(key_vec);

      with_kmip_operation([&kmip, &metadata, &sym_key, &id, this]() {
        id = kmip.client().op_register_key(metadata.key_id(),
                                           config_.object_group, sym_key);
        if (!id.empty()) {
          (void)kmip.client().op_activate(id);
        }
      });

      if (id.empty()) {
        std::string err_msg =
            "Cannot register key with name: " + metadata.key_id() +
            " and group: " + config_.object_group;
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        return true;
      }
    } else if (data.type() == "SECRET") {
      // Register and activate secret (password type)
      auto secret_str = std::string(key.begin(), key.end());
      auto secret = kmipclient::Secret::from_text(
          secret_str, kmipclient::secret_data_type::KMIP_SECDATA_PASSWORD);

      with_kmip_operation([&kmip, &metadata, &secret, &id, this]() {
        id = kmip.client().op_register_secret(metadata.key_id(),
                                              config_.object_group, secret);
        if (!id.empty()) {
          (void)kmip.client().op_activate(id);
        }
      });

      if (id.empty()) {
        std::string err_msg =
            "Cannot register secret with name: " + metadata.key_id() +
            " and group: " + config_.object_group;
        LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
        return true;
      }
    } else {  // we only support AES keys and SECRET type (passwords)
      std::string err_msg = "Unsupported KMIP entity: ";
      err_msg += data.type();
      err_msg += ", can not store";
      LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
      return true;
    }

    data.set_extension({id});
    // Clear cached IDs since a new object was added
    clear_cached_object_ids();
    increment_expected_cacheable_size_if_set();
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
    if (auto expected_size = get_expected_cacheable_size();
        expected_size.has_value()) {
      return *expected_size;
    }

    auto &kmip = get_kmip_client();
    auto cached_ids = snapshot_cached_object_ids();
    auto key_ids = cached_ids.key_ids;
    auto secret_ids = cached_ids.secret_ids;

    // Fetch and cache object IDs if not already cached
    if (key_ids.empty() && secret_ids.empty()) {
      key_ids = get_object_ids(
          kmip, kmipclient::object_type::KMIP_OBJTYPE_SYMMETRIC_KEY);
      secret_ids = get_object_ids(
          kmip, kmipclient::object_type::KMIP_OBJTYPE_SECRET_DATA);
      update_cached_object_ids(key_ids, secret_ids);
    }

    const size_t cacheable_size = key_ids.size() + secret_ids.size();
    set_expected_cacheable_size(cacheable_size);
    return cacheable_size;
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

  try {
    auto &kmip = get_kmip_client();
    const auto &id = data.get_extension().uuid;

    // Revoke the object first (maps to legacy reason 1 - deactivation)
    try {
      with_kmip_operation([&kmip, &id]() {
        (void)kmip.client().op_revoke(
            id, kmipclient::revocation_reason_type::KMIP_REVOKE_UNSPECIFIED,
            "Deleting the key", 0);
      });
    } catch (const std::exception &e) {
      std::string err_msg =
          "Cannot deactivate key/secret with ID: " + id + " Cause: " + e.what();
      LogComponentErr(WARNING_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
      // no reason to fail here, if we're deactivating non-existent key
    }

    // Destroy the object
    try {
      with_kmip_operation(
          [&kmip, &id]() { (void)kmip.client().op_destroy(id); });
    } catch (const std::exception &e) {
      std::string err_msg =
          "Cannot delete key/secret with ID: " + id + " Cause: " + e.what();
      LogComponentErr(WARNING_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
      // no reason to fail here, if we're deleting non-existent key
    }

    // Clear cached IDs since they're now stale after deletion
    clear_cached_object_ids();
  } catch (const std::exception &e) {
    std::string err_msg = std::string("std exception in function '") +
                          __func__ + "': " + e.what();
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
  } catch (...) {
    std::string err_msg =
        std::string("Unknown exception in function '") + __func__ + '\'';
    LogComponentErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, err_msg.c_str());
  }

  decrement_expected_cacheable_size();

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

kmipclient::Kmip &Keyring_kmip_backend::get_kmip_client() const {
  std::lock_guard<std::mutex> guard(kmip_client_mutex_);
  if (!kmip_client_) {
    kmipclient::NetClient::TlsVerificationOptions tls_verification;
    tls_verification.peer_verification = config_.tls_peer_verification;
    tls_verification.hostname_verification = config_.tls_hostname_verification;

    kmip_client_ = std::make_unique<kmipclient::Kmip>(
        config_.server_addr.c_str(), config_.server_port.c_str(),
        config_.client_ca.c_str(), config_.client_key.c_str(),
        config_.server_ca.c_str(), config_.kmip_timeout_ms,
        kmipcore::KMIP_VERSION_1_4, std::shared_ptr<kmipcore::Logger>{},
        tls_verification, true);
  }
  // Return a reference to the lazily initialized client owned by unique_ptr.
  return *kmip_client_;
}

}  // namespace backend

}  // namespace keyring_kmip
