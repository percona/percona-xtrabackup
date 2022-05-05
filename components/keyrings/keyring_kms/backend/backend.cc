/* Copyright (c) 2021, Oracle and/or its affiliates.
   Copyright (c) 2022, Percona and/or its affiliates.

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

#include <chrono>
#include <fstream>
#include <memory>
#include <thread>

#include "backend.h"

#include <components/keyrings/common/data_file/reader.h>
#include <components/keyrings/common/data_file/writer.h>
#include <components/keyrings/common/json_data/json_reader.h>
#include <components/keyrings/common/json_data/json_writer.h>
#include <components/keyrings/common/memstore/cache.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/utils/utils.h>
#include "base64.h"

#include <mysql/components/services/log_builtins.h> /* LogComponentErr */
#include "mysqld_error.h"                           /* Errors */

namespace keyring_kms {

namespace backend {

using keyring_common::data::Data;
using keyring_common::data::Sensitive_data;
using keyring_common::data_file::File_reader;
using keyring_common::data_file::File_writer;
using keyring_common::json_data::Json_data_extension;
using keyring_common::json_data::Json_reader;
using keyring_common::json_data::Json_writer;
using keyring_common::json_data::output_vector;
using keyring_common::meta::Metadata;
using keyring_common::utils::get_random_data;

Json_data_extension ext;

Keyring_kms_backend::Keyring_kms_backend(const std::string &keyring_kms_name,
                                         bool read_only,
                                         config::Config_pod const &config)
    : config_(config),
      keyring_kms_name_(keyring_kms_name),
      read_only_(read_only),
      json_writer_(),
      valid_(false),
      kms(config_.region_, config_.auth_key_, config_.secret_access_key_),
      kmsKey(config_.kms_key_) {
  if (keyring_kms_name_.length() == 0) return;
  std::string data;
  output_vector elements;
  create_file_if_missing(keyring_kms_name_);
  {
    /* Read the file */
    File_reader file_reader(keyring_kms_name_, read_only_, data);
    if (!file_reader.valid()) return;
  }

  /* It is possible that file is empty and that's ok. */
  if (data.length()) {
    /* Read JSON data - format check */
    Json_reader json_reader(data);
    if (!json_reader.valid()) return;

    /* Cache */
    json_writer_.set_data(data);
  }
  valid_ = true;
}

bool Keyring_kms_backend::load_cache(
    keyring_common::operations::Keyring_operations<Keyring_kms_backend>
        &operations) {
  if (json_writer_.num_elements() == 0) return false;
  Json_reader json_reader(json_writer_.to_string());
  if (!json_reader.valid()) return true;
  if (json_reader.num_elements() != json_writer_.num_elements()) return true;

  for (size_t i = 0; i < json_reader.num_elements(); ++i) {
    std::unique_ptr<Json_data_extension> data_ext;
    Metadata metadata;
    Data data;
    if (json_reader.get_element(i, metadata, data, data_ext)) return true;
    auto decodedData = data.data().decode();
    std::string currKey(decodedData.begin(), decodedData.end());
    std::string decodedKey;
    auto err = kms.decrypt(currKey, decodedKey);

    // base64 form is always longer than plain
    std::vector<char> decoded(decodedKey.size());
    auto decodedLen = base64_decode(decodedKey.c_str(), decodedKey.size(),
                                    decoded.data(), NULL, 0);

    if (err) {
      (void)json_writer_.remove_element(metadata, ext);
      LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_KMS_ERROR,
                      err.error_message.c_str());
      return true;
    }

    data.set_data(Sensitive_data(pfs_string(decoded.data(), decodedLen)));
    if (operations.insert(metadata, data)) return true;
  }
  return false;
}

bool Keyring_kms_backend::get(const Metadata &, Data &) const {
  /* Shouldn't have reached here. */
  return true;
}

bool Keyring_kms_backend::store(const Metadata &metadata, Data &data) {
  if (!metadata.valid() || !data.valid()) return true;
  Data dataCopy = data;
  {
    auto decodedData = data.data().decode();
    std::string currKey(decodedData.begin(), decodedData.end());
    std::string encodedKey;

    // base64 form needs 4*(n/3) bytes
    std::vector<char> base64Curr((currKey.size() / 3 + 2) * 4);
    base64_encode(currKey.c_str(), currKey.size(), base64Curr.data(), false);
    currKey = base64Curr.data();
    auto err = kms.encrypt(currKey, kmsKey, encodedKey);
    if (err) {
      (void)json_writer_.remove_element(metadata, ext);
      LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_KMS_ERROR,
                      err.error_message.c_str());
      return true;
    }
    dataCopy.set_data(
        Sensitive_data(pfs_string(encodedKey.c_str(), encodedKey.size())));
  }
  if (json_writer_.add_element(metadata, dataCopy, ext)) return true;
  if (write_to_file()) {
    /* Erase stored entry */
    (void)json_writer_.remove_element(metadata, ext);
    return true;
  }
  return false;
}

bool Keyring_kms_backend::erase(const Metadata &metadata, Data &data) {
  if (!metadata.valid()) return true;
  if (json_writer_.remove_element(metadata, ext)) return true;
  if (write_to_file()) {
    /* Add entry back */
    (void)json_writer_.add_element(metadata, data, ext);
    return true;
  }
  return false;
}

bool Keyring_kms_backend::generate(const Metadata &metadata, Data &data,
                                   size_t length) {
  if (!metadata.valid()) return true;

  std::unique_ptr<unsigned char[]> key(new unsigned char[length]);
  if (!key) return true;
  if (!get_random_data(key, length)) return true;

  pfs_string key_str;
  key_str.assign(reinterpret_cast<const char *>(key.get()), length);
  data.set_data(keyring_common::data::Sensitive_data{key_str});

  return store(metadata, data);
}

bool Keyring_kms_backend::write_to_file() {
  /* Get JSON string from cache and feed it to file writer */
  File_writer file_writer(keyring_kms_name_, json_writer_.to_string());
  return !file_writer.valid();
}

void Keyring_kms_backend::create_file_if_missing(std::string const &file_name) {
  std::ifstream f(file_name.c_str());
  if (f.good())
    f.close();
  else {
    std::ofstream o(file_name.c_str());
    o.close();
  }
}

}  // namespace backend

}  // namespace keyring_kms
