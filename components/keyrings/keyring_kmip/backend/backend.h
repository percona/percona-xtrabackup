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

#ifndef KEYRING_FILE_BACKEND_INCLUDED
#define KEYRING_FILE_BACKEND_INCLUDED

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <components/keyrings/common/data/data_extension.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/operations/operations.h>
#include "config/config.h"
#include "kmipclient/Kmip.hpp"

namespace kmipclient {
class Kmip;
}

namespace keyring_kmip {

struct IdExt {
  std::string uuid;
};

namespace backend {

class Keyring_kmip_backend final {
 public:
  explicit Keyring_kmip_backend(config::Config_pod const &config);

  ~Keyring_kmip_backend();

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
             keyring_common::data::Data_extension<IdExt> &data);

  /**
    Erase data located at given key

    @param [in] metadata Key
    @param [in] data     Value - not used.

    @returns Status of erase operation
      @retval false Data deleted
      @retval true  Could not find or delete data
  */
  bool erase(const keyring_common::meta::Metadata &metadata,
             keyring_common::data::Data_extension<IdExt> &data);

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
                keyring_common::data::Data_extension<IdExt> &data,
                size_t length);

  /**
    Populate cache

    @param [in] operations  Handle to operations class

    @returns status of cache insertion
      @retval false Success
      @retval true  Failure
  */
  bool load_cache(keyring_common::operations::Keyring_operations<
                  Keyring_kmip_backend,
                  keyring_common::data::Data_extension<IdExt>> &operations);

  /** Maximum data length supported */
  size_t maximum_data_length() const {
    // TODO
    return 16384;
  }

  /** Get number of elements stored in backend */
  size_t size() const;

  /** Validity */
  bool valid() const { return valid_; }

 private:
  struct Cached_object_ids {
    std::vector<std::string> key_ids;
    std::vector<std::string> secret_ids;
  };

  template <typename Callable>
  auto with_kmip_operation(Callable callable) const -> decltype(callable()) {
    std::lock_guard<std::mutex> op_guard(kmip_operation_mutex_);
    return callable();
  }

  template <typename Callable>
  auto with_cache_state(Callable callable) const -> decltype(callable()) {
    std::lock_guard<std::mutex> cache_guard(cache_state_mutex_);
    return callable();
  }

  Cached_object_ids snapshot_cached_object_ids() const {
    return with_cache_state([this]() {
      return Cached_object_ids{cached_key_ids_, cached_secret_ids_};
    });
  }

  void update_cached_object_ids(
      const std::vector<std::string> &key_ids,
      const std::vector<std::string> &secret_ids) const {
    with_cache_state([this, &key_ids, &secret_ids]() {
      cached_key_ids_ = key_ids;
      cached_secret_ids_ = secret_ids;
    });
  }

  std::optional<size_t> get_expected_cacheable_size() const {
    return with_cache_state([this]() { return expected_cacheable_size_; });
  }

  void reset_expected_cacheable_size() const {
    with_cache_state([this]() { expected_cacheable_size_ = std::nullopt; });
  }

  void set_expected_cacheable_size(size_t value) const {
    with_cache_state([this, value]() { expected_cacheable_size_ = value; });
  }

  void increment_expected_cacheable_size_if_set() const {
    with_cache_state([this]() {
      if (expected_cacheable_size_.has_value()) {
        ++(*expected_cacheable_size_);
      }
    });
  }

  static bool is_empty_or_whitespace(const std::string &value);
  static bool validate_required_config(const config::Config_pod &config,
                                       std::string &missing_options);
  std::vector<std::string> get_object_ids(
      kmipclient::Kmip &kmip, kmipclient::object_type object_type) const {
    return with_kmip_operation([this, &kmip, object_type]() {
      return config_.object_group.empty()
                 ? kmip.client().op_all(object_type, config_.max_objects)
                 : kmip.client().op_locate_by_group(
                       config_.object_group, object_type, config_.max_objects);
    });
  }

  /** Decrement expected cacheable size when a backend object is intentionally
   * skipped. */
  void decrement_expected_cacheable_size() const {
    with_cache_state([this]() {
      if (expected_cacheable_size_.has_value() &&
          *expected_cacheable_size_ > 0) {
        --(*expected_cacheable_size_);
      }
    });
  }

  /** Clear cached object IDs after load_cache completes */
  void clear_cached_object_ids() const {
    with_cache_state([this]() {
      cached_key_ids_.clear();
      cached_secret_ids_.clear();
    });
  }

  /** Validity */
  bool valid_;
  config::Config_pod config_;

  /** KMIP client instance - lazy initialized on first use */
  mutable std::unique_ptr<kmipclient::Kmip> kmip_client_;

  /** Guards lazy initialization of kmip_client_. */
  mutable std::mutex kmip_client_mutex_;

  /** Serializes calls to kmip.client().op_* APIs on the shared client instance.
   */
  mutable std::mutex kmip_operation_mutex_;

  /** Protects expected_cacheable_size_ and cached object ID vectors. */
  mutable std::mutex cache_state_mutex_;

  /**
    Backend size as expected by Keyring_operations::load_cache():
    number of entries that should be inserted into cache.
    It is initialized from locate() counts and adjusted for entries that
    cannot be inserted into cache.
  */
  mutable std::optional<size_t> expected_cacheable_size_;

  /** Cached SYMMETRIC_KEY object IDs - populated by size() and reused in
   * load_cache() */
  mutable std::vector<std::string> cached_key_ids_;

  /** Cached SECRET_DATA object IDs - populated by size() and reused in
   * load_cache() */
  mutable std::vector<std::string> cached_secret_ids_;

  /** Initialize or get existing KMIP client instance */
  kmipclient::Kmip &get_kmip_client() const;
};
}  // namespace backend
}  // namespace keyring_kmip

#endif  // !KEYRING_FILE_BACKEND_INCLUDED
