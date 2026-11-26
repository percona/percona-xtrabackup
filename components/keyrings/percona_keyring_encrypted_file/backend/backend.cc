/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.
   Copyright (c) 2026 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <fstream>
#include <memory>

#include <components/keyrings/common/data_file/reader.h>
#include <components/keyrings/common/data_file/writer.h>
#include <components/keyrings/common/encryption/aes.h>
#include <components/keyrings/common/json_data/json_reader.h>
#include <components/keyrings/common/json_data/json_writer.h>
#include <components/keyrings/common/memstore/cache.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/utils/utils.h>
#include "backend.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"

namespace percona_keyring_encrypted_file::backend {

using keyring_common::data::Data;
using keyring_common::data_file::File_reader;
using keyring_common::data_file::File_writer;
using keyring_common::json_data::Json_data_extension;
using keyring_common::json_data::Json_reader;
using keyring_common::json_data::Json_writer;
using keyring_common::json_data::output_vector;
using keyring_common::meta::Metadata;
using keyring_common::utils::get_random_data;

Json_data_extension ext;

Keyring_encrypted_file_backend::Keyring_encrypted_file_backend(
    const std::string &keyring_file_name, const bool read_only,
    const std::string &password, const uint32_t iterations)
    : keyring_file_name_(keyring_file_name),
      read_only_(read_only),
      password_(password),
      iterations_(iterations),
      valid_(false) {
  if (keyring_file_name_.length() == 0) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_KEYRING_FILE_NAME_EMPTY);
    return;
  }
  std::string data;
  create_file_if_missing(keyring_file_name_);
  {
    const File_reader file_reader(keyring_file_name_, read_only_, data);
    if (!file_reader.valid()) {
      LogComponentErr(ERROR_LEVEL,
                      ER_KEYRING_COMPONENT_KEYRING_FILE_READ_FAILED,
                      keyring_file_name_.c_str());
      return;
    }
  }

  /* It is possible that file is empty and that's ok. */
  if (data.length()) {
    std::string decrypted;
    if (decrypt_data(data, decrypted)) {
      LogComponentErr(ERROR_LEVEL,
                      ER_KEYRING_COMPONENT_KEYRING_FILE_DECRYPT_FAILED,
                      keyring_file_name_.c_str());
      return;
    }
    /* Read JSON data - format check */
    const Json_reader json_reader(decrypted);
    if (!json_reader.valid()) {
      LogComponentErr(ERROR_LEVEL,
                      ER_KEYRING_COMPONENT_KEYRING_FILE_INVALID_FORMAT,
                      keyring_file_name_.c_str());
      return;
    }
    /* Cache */
    json_writer_.set_data(decrypted);
  }
  valid_ = true;
}

bool Keyring_encrypted_file_backend::load_cache(
    keyring_common::operations::Keyring_operations<
        Keyring_encrypted_file_backend> &operations) {
  if (json_writer_.num_elements() == 0) return false;
  const Json_reader json_reader(json_writer_.to_string());
  if (!json_reader.valid()) {
    LogComponentErr(ERROR_LEVEL,
                    ER_KEYRING_COMPONENT_KEYRING_FILE_JSON_EXTRACT_FAILED);
    return true;
  }
  if (json_reader.num_elements() != json_writer_.num_elements()) {
    LogComponentErr(ERROR_LEVEL,
                    ER_KEYRING_COMPONENT_KEYRING_FILE_JSON_EXTRACT_FAILED);
    return true;
  }
  for (size_t i = 0; i < json_reader.num_elements(); ++i) {
    std::unique_ptr<Json_data_extension> data_ext;
    Metadata metadata;
    Data data;
    if (json_reader.get_element(i, metadata, data, data_ext)) {
      LogComponentErr(ERROR_LEVEL,
                      ER_KEYRING_COMPONENT_KEYRING_FILE_KEY_EXTRACT_FAILED);
      return true;
    }
    if (operations.insert(metadata, data)) return true;
  }
  return false;
}

bool Keyring_encrypted_file_backend::get(const Metadata &, Data &) const {
  /* Shouldn't have reached here. */
  return true;
}

bool Keyring_encrypted_file_backend::store(const Metadata &metadata,
                                           Data &data) {
  if (!metadata.valid() || !data.valid()) return true;
  if (json_writer_.add_element(metadata, data, ext)) return true;
  if (write_to_file()) {
    /* Erase stored entry */
    (void)json_writer_.remove_element(metadata, ext);
    return true;
  }
  return false;
}

bool Keyring_encrypted_file_backend::erase(const Metadata &metadata,
                                           Data &data) {
  if (!metadata.valid()) return true;
  if (json_writer_.remove_element(metadata, ext)) return true;
  if (write_to_file()) {
    /* Add entry back */
    (void)json_writer_.add_element(metadata, data, ext);
    return true;
  }
  return false;
}

bool Keyring_encrypted_file_backend::generate(const Metadata &metadata,
                                              Data &data, size_t length) {
  if (!metadata.valid()) return true;

  const std::unique_ptr<unsigned char[]> key(new unsigned char[length]);
  if (!key) return true;
  if (!get_random_data(key, length)) return true;

  pfs_string key_str;
  key_str.assign(reinterpret_cast<const char *>(key.get()), length);
  data.set_data(keyring_common::data::Sensitive_data{key_str});

  return store(metadata, data);
}

bool Keyring_encrypted_file_backend::write_to_file() {
  std::string ciphertext;
  if (encrypt_data(json_writer_.to_string(), ciphertext)) return true;
  const File_writer file_writer(keyring_file_name_, ciphertext);
  return !file_writer.valid();
}

bool Keyring_encrypted_file_backend::encrypt_data(const std::string &plaintext,
                                                  std::string &ciphertext) {
  using namespace keyring_common::aes_encryption;
  const auto mode = Keyring_aes_opmode::keyring_aes_256_cbc;

  // Generate random salt for PBKDF2
  const std::unique_ptr<unsigned char[]> salt(
      new (std::nothrow) unsigned char[k_salt_size]);
  if (!salt || !get_random_data(salt, k_salt_size)) return true;

  // Generate random IV for AES-CBC
  const std::unique_ptr<unsigned char[]> iv(
      new (std::nothrow) unsigned char[k_iv_size]);
  if (!iv || !get_random_data(iv, k_iv_size)) return true;

  const size_t cipher_size = get_ciphertext_size(plaintext.length(), mode);
  const std::unique_ptr<unsigned char[]> cipher_buf(
      new (std::nothrow) unsigned char[cipher_size]);
  if (!cipher_buf) return true;

  size_t encrypted_length = 0;
  if (aes_encrypt_pbkdf2(
          reinterpret_cast<const unsigned char *>(plaintext.c_str()),
          static_cast<unsigned int>(plaintext.length()), cipher_buf.get(),
          reinterpret_cast<const unsigned char *>(password_.c_str()),
          password_.length(), salt.get(), k_salt_size, iterations_, mode,
          iv.get(), true, &encrypted_length) != AES_OP_OK)
    return true;

  /*
    On-disk format v1:
      [version:1=0x01][salt:32][iterations:4 BE][iv:16][ciphertext]
  */
  ciphertext.assign(1, static_cast<char>(0x01)); /* version */
  ciphertext.append(reinterpret_cast<const char *>(salt.get()), k_salt_size);
  const uint8_t iters_be[4] = {static_cast<uint8_t>((iterations_ >> 24) & 0xFF),
                               static_cast<uint8_t>((iterations_ >> 16) & 0xFF),
                               static_cast<uint8_t>((iterations_ >> 8) & 0xFF),
                               static_cast<uint8_t>(iterations_ & 0xFF)};
  ciphertext.append(reinterpret_cast<const char *>(iters_be), 4);
  ciphertext.append(reinterpret_cast<const char *>(iv.get()), k_iv_size);
  ciphertext.append(reinterpret_cast<const char *>(cipher_buf.get()),
                    encrypted_length);
  return false;
}

bool Keyring_encrypted_file_backend::decrypt_data(const std::string &raw,
                                                  std::string &plaintext) {
  using namespace keyring_common::aes_encryption;
  const auto mode = Keyring_aes_opmode::keyring_aes_256_cbc;

  /*
    Parse v1 header offsets:
      [0]     version
      [1-32]  salt (32 bytes)
      [33-36] iterations (big-endian uint32)
      [37-52] iv (16 bytes)
      [53+]   ciphertext
  */
  if (raw.length() <= k_header_size) return true;

  if (static_cast<uint8_t>(raw[0]) != 0x01) return true;

  const auto *salt = reinterpret_cast<const unsigned char *>(raw.c_str() + 1);
  const uint32_t iterations =
      (static_cast<uint32_t>(static_cast<uint8_t>(raw[33])) << 24) |
      (static_cast<uint32_t>(static_cast<uint8_t>(raw[34])) << 16) |
      (static_cast<uint32_t>(static_cast<uint8_t>(raw[35])) << 8) |
      (static_cast<uint32_t>(static_cast<uint8_t>(raw[36])));
  const auto *iv = reinterpret_cast<const unsigned char *>(raw.c_str() + 37);
  const auto *cipher_data =
      reinterpret_cast<const unsigned char *>(raw.c_str() + k_header_size);
  const size_t cipher_len = raw.length() - k_header_size;

  const std::unique_ptr<unsigned char[]> plain_buf(
      new (std::nothrow) unsigned char[cipher_len]);
  if (!plain_buf) return true;

  size_t decrypted_length = 0;
  if (aes_decrypt_pbkdf2(
          cipher_data, static_cast<unsigned int>(cipher_len), plain_buf.get(),
          reinterpret_cast<const unsigned char *>(password_.c_str()),
          password_.length(), salt, k_salt_size, iterations, mode, iv, true,
          &decrypted_length) != AES_OP_OK)
    return true;

  plaintext.assign(reinterpret_cast<const char *>(plain_buf.get()),
                   decrypted_length);
  return false;
}

void Keyring_encrypted_file_backend::create_file_if_missing(
    const std::string &file_name) {
  std::ifstream f(file_name.c_str());
  if (f.good())
    f.close();
  else {
    std::ofstream o(file_name.c_str());
    o.close();
  }
}

}  // namespace percona_keyring_encrypted_file::backend
