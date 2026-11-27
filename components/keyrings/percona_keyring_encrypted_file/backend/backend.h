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

#ifndef PERCONA_KEYRING_ENCRYPTED_FILE_BACKEND_INCLUDED
#define PERCONA_KEYRING_ENCRYPTED_FILE_BACKEND_INCLUDED

#include <cstddef>
#include <cstdint>  //size_t
#include <string>

#include <components/keyrings/common/json_data/json_writer.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/operations/operations.h>

namespace percona_keyring_encrypted_file::backend {
class Keyring_encrypted_file_backend final {
 public:
  explicit Keyring_encrypted_file_backend(const std::string &keyring_file_name,
                                          bool read_only,
                                          const std::string &password,
                                          uint32_t iterations);

  ~Keyring_encrypted_file_backend() = default;

  /**
    Fetch data

    @param [in]  metadata Key
    @param [out] data     Value

    @returns Status of find operation
      @retval false Entry found. Check data.
      @retval true  Entry missing.
  */
  bool get(const keyring_common::meta::Metadata &metadata,
           keyring_common::data::Data &data) const;

  /**
    Store data

    @param [in]      metadata Key
    @param [in, out] data     Value

    @returns Status of store operation
      @retval false Entry stored successfully
      @retval true  Failure
  */

  bool store(const keyring_common::meta::Metadata &metadata,
             keyring_common::data::Data &data);

  /**
    Erase data located at given key

    @param [in] metadata Key
    @param [in] data     Value - not used.

    @returns Status of erase operation
      @retval false Data deleted
      @retval true  Could not find or delete data
  */
  bool erase(const keyring_common::meta::Metadata &metadata,
             keyring_common::data::Data &data);

  /**
    Generate random data and store it

    @param [in]  metadata Key
    @param [out] data     Generated value
    @param [in]  length   Length of data to be generated

    @returns Status of generate + store operation
      @retval false Data generated and stored successfully
      @retval true  Error

  */
  bool generate(const keyring_common::meta::Metadata &metadata,
                keyring_common::data::Data &data, size_t length);

  /**
    Populate cache

    @param [in] operations  Handle to operations class

    @returns status of cache insertion
      @retval false Success
      @retval true  Failure
  */
  bool load_cache(keyring_common::operations::Keyring_operations<
                  Keyring_encrypted_file_backend> &operations);

  /** Maximum data length supported */
  size_t maximum_data_length() const { return 16384; }

  /** Get number of elements stored in backend */
  size_t size() const { return json_writer_.num_elements(); }

  /** Validity */
  bool valid() const { return valid_; }

 private:
  /*
    On-disk format v1:
      [version:1][salt:32][iterations:4 BE][iv:16][ciphertext]
  */
  static constexpr size_t k_version_size = 1;
  static constexpr size_t k_salt_size = 32;
  static constexpr size_t k_iv_size = 16;
  /* 1 (version) + 32 (salt) + 4 (iterations) + 16 (iv) */
  static constexpr size_t k_header_size =
      k_version_size + k_salt_size + sizeof(uint32_t) + k_iv_size;
  static_assert(k_header_size == (1 + 32 + 4 + 16),
                "v1: Header size must be 53 bytes");
  /**
    Write existing data to file.
    This function overwrites existing data stored in the file.

    @returns Status of write operation.
      @retval false Successfully written data to file
      @retval true  Error writing data to file
  */
  bool write_to_file();

  /** Create data file if missing */
  void create_file_if_missing(const std::string &file_name);

  /**
    Encrypt plaintext using AES-256-CBC with a PBKDF2-derived key.
    Output format: [version:1][salt:32][iterations:4BE][iv:16][ciphertext].

    @param [in]  plaintext  Data to encrypt
    @param [out] ciphertext Encrypted output

    @returns Status of encrypt operation
      @retval false Success
      @retval true  Error
  */
  bool encrypt_data(const std::string &plaintext, std::string &ciphertext);

  /**
    Decrypt data previously produced by encrypt_data().
    Expects v1 format: [version:1][salt:32][iterations:4BE][iv:16][ciphertext].

    @param [in]  raw       Raw bytes read from file
    @param [out] plaintext Decrypted output

    @returns Status of decrypt operation
      @retval false Success
      @retval true  Error (wrong password, truncated input, or corrupt data)
  */
  bool decrypt_data(const std::string &raw, std::string &plaintext);

  /** Keyring file */
  std::string keyring_file_name_;

  /** Read only flag */
  bool read_only_;

  /** Password to be used to encrypt/decrypt file */
  std::string password_;

  /** PBKDF2 iteration count used when writing the file */
  uint32_t iterations_;

  /** In memory cache for keyring data */
  keyring_common::json_data::Json_writer json_writer_;

  /** Validity */
  bool valid_;
};
}  // namespace percona_keyring_encrypted_file::backend

#endif  // !PERCONA_KEYRING_ENCRYPTED_FILE_BACKEND_INCLUDED
