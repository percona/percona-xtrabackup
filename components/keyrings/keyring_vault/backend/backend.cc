/* Copyright (c) 2023, Percona and/or its affiliates.

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

#include "backend.h"

#include "vault_curl.h"
#include "vault_parser_composer.h"

#include <components/keyrings/common/data/data.h>
#include <components/keyrings/common/data/meta.h>
#include <components/keyrings/common/utils/utils.h>

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/log_builtins.h> /* LogComponentErr */
#include <mysqld_error.h>

namespace keyring_vault {
namespace backend {

using keyring_common::data::Data;
using keyring_common::meta::Metadata;
using keyring_common::utils::get_random_data;

Keyring_vault_backend::Keyring_vault_backend(
    std::unique_ptr<IKeyring_vault_curl> vault_curl)
    : m_valid{false}, m_size{0}, m_vault_curl{std::move(vault_curl)} {}

bool Keyring_vault_backend::init() {
  try {
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) return false;

    if (m_vault_curl == nullptr || m_vault_curl->init()) {
      curl_global_cleanup();
      return true;
    }

    static pfs_string err_msg("Could not retrieve list of keys from Vault.");
    pfs_string json_response;

    if (m_vault_curl->list_keys(&json_response)) {
      LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                      (err_msg + Keyring_vault_curl::get_errors_from_response(
                                     json_response))
                          .c_str());
      return true;
    }

    if (json_response.empty()) {
      // no keys
      m_valid = true;
      return false;
    }

    m_fetched_keys = std::make_unique<Vault_keys_container>();

    if (Keyring_vault_parser_composer::parse_keys(json_response,
                                                  &*m_fetched_keys)) {
      LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                      err_msg.c_str());
      return true;
    }

    m_size = m_fetched_keys->size();
    m_valid = true;

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    curl_global_cleanup();
    return true;
  }
}

bool Keyring_vault_backend::load_cache(
    keyring_common::operations::Keyring_operations<Keyring_vault_backend>
        &operations) {
  pfs_string json_response;

  for (const auto &key_ptr : *m_fetched_keys) {
    if (key_ptr != nullptr && key_ptr->valid()) {
      json_response.clear();
      Data fetched_data;

      if (m_vault_curl->read_key(*key_ptr, &json_response) ||
          Keyring_vault_parser_composer::parse_key_data(
              json_response, &fetched_data,
              m_vault_curl->get_resolved_secret_mount_point_version())) {
        LogComponentErr(
            ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
            ("Could not read key from Vault." +
             Keyring_vault_curl::get_errors_from_response(json_response))
                .c_str());
        return true;
      }

      if (operations.insert(*key_ptr, fetched_data)) {
        return true;
      }
    }
  }

  m_fetched_keys.reset();

  return false;
}

bool Keyring_vault_backend::get(const Metadata &metadata [[maybe_unused]],
                                Data &data [[maybe_unused]]) const {
  /* Shouldn't have reached here if we cache things. */
  assert(0);
  return false;
}

bool Keyring_vault_backend::store(const Metadata &metadata, Data &data) {
  if (!metadata.valid() || !data.valid()) return true;

  pfs_string json_response;
  pfs_string errors_from_response;

  if (m_vault_curl->write_key(metadata, data, &json_response) ||
      !((errors_from_response =
             Keyring_vault_curl::get_errors_from_response(json_response))
            .empty())) {
    errors_from_response.insert(0, "Could not write key to Vault.");
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    errors_from_response.c_str());
    return true;
  }

  ++m_size;

  return false;
}

bool Keyring_vault_backend::erase(const Metadata &metadata,
                                  Data &data [[maybe_unused]]) {
  if (!metadata.valid()) return true;

  pfs_string json_response;
  pfs_string errors_from_response;

  if (m_vault_curl->delete_key(metadata, &json_response) ||
      !((errors_from_response =
             Keyring_vault_curl::get_errors_from_response(json_response))
            .empty())) {
    LogComponentErr(
        ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
        ("Could not delete key from Vault." + errors_from_response).c_str());
    return true;
  }

  --m_size;

  return false;
}

bool Keyring_vault_backend::generate(const Metadata &metadata, Data &data,
                                     size_t length) {
  if (!metadata.valid()) return true;

  std::unique_ptr<unsigned char[]> key(new unsigned char[length]);
  if (key == nullptr) return true;
  if (!get_random_data(key, length)) return true;

  pfs_string key_str;
  key_str.assign(reinterpret_cast<const char *>(key.get()), length);
  data.set_data(keyring_common::data::Sensitive_data{key_str});
  data.set_type("AES");

  return store(metadata, data);
}

}  // namespace backend
}  // namespace keyring_vault
