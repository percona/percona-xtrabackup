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

#ifndef KEYRING_VAULT_BACKEND_INCLUDED
#define KEYRING_VAULT_BACKEND_INCLUDED

#include "vault_curl.h"
#include "vault_keys_container.h"

#include <components/keyrings/common/data/data_extension.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/operations/operations.h>

#include <memory>
#include <string>

namespace keyring_common {

namespace data {
class Data;
}  // namespace data

namespace meta {
class Metadata;
}  // namespace meta

}  // namespace keyring_common

namespace keyring_vault {
namespace backend {

class Keyring_vault_backend final {
 public:
  explicit Keyring_vault_backend(
      std::unique_ptr<IKeyring_vault_curl> vault_curl);

  ~Keyring_vault_backend() {}

  /**
    Init backend

    @returns Initialization status
      @retval false Initialization successful.
      @retval true  Initialization failed.
  */
  bool init();

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
  bool load_cache(
      keyring_common::operations::Keyring_operations<Keyring_vault_backend>
          &operations);

  /** Maximum data length supported */
  size_t maximum_data_length() const { return 16384; }

  /** Get number of elements stored in backend */
  size_t size() const { return m_size; }

  /** Validity */
  bool valid() const { return m_valid; }

 private:
  bool m_valid;
  size_t m_size;
  std::unique_ptr<IKeyring_vault_curl> m_vault_curl;
  std::unique_ptr<Vault_keys_container> m_fetched_keys;
};

}  // namespace backend
}  // namespace keyring_vault

#endif  // KEYRING_VAULT_BACKEND_INCLUDED
