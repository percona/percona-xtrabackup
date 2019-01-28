/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

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

#include "i_keyring_key.h"
#include <my_dbug.h>

namespace keyring {

class System_key_adapter : public IKey {
 public:
  System_key_adapter(uint key_version, IKey *keyring_key)
      : key_version(key_version), keyring_key(keyring_key) {}

  void set_keyring_key(IKey *key, uint key_version) {
    system_key_data.free();
    this->keyring_key = key;
    this->key_version = key_version;
  }

  IKey *get_keyring_key() const noexcept { return keyring_key; }

  uint get_key_version() const noexcept { return key_version; }

  virtual std::string *get_key_signature() const noexcept {
    DBUG_ASSERT(keyring_key != nullptr);
    return keyring_key->get_key_signature();
  }

  virtual std::string *get_key_type() {
    DBUG_ASSERT(keyring_key != nullptr);
    return keyring_key->get_key_type();
  }
  virtual std::string *get_key_id() {
    DBUG_ASSERT(keyring_key != nullptr);
    return keyring_key->get_key_id();
  }
  virtual std::string *get_user_id() {
    DBUG_ASSERT(keyring_key != nullptr);
    return keyring_key->get_user_id();
  }
  virtual uchar *get_key_data() {
    DBUG_ASSERT(keyring_key != nullptr);

    if (system_key_data.get_key_data() == nullptr) construct_system_key_data();

    return system_key_data.get_key_data();
  }
  virtual size_t get_key_data_size() {
    DBUG_ASSERT(keyring_key != nullptr);

    if (system_key_data.get_key_data() == nullptr) construct_system_key_data();

    return system_key_data.get_key_data_size();
  }
  virtual size_t get_key_pod_size() const noexcept {
    DBUG_ASSERT(false);
    return 0;
  }
  virtual uchar *release_key_data() noexcept {
    DBUG_ASSERT(false);
    return nullptr;
  }
  virtual void xor_data() noexcept { DBUG_ASSERT(false); }
  virtual void set_key_data(uchar *key_data, size_t key_data_size) {
    keyring_key->set_key_data(key_data, key_data_size);
  }
  virtual void set_key_type(const std::string *key_type) {
    keyring_key->set_key_type(key_type);
  }
  virtual bool load_from_buffer(uchar *buffer MY_ATTRIBUTE((unused)),
                                size_t *buffer_position MY_ATTRIBUTE((unused)),
                                size_t input_buffer_size
                                    MY_ATTRIBUTE((unused))) noexcept {
    DBUG_ASSERT(false);
    return false;
  }
  virtual void store_in_buffer(uchar *buffer MY_ATTRIBUTE((unused)),
                               size_t *buffer_position
                                   MY_ATTRIBUTE((unused))) const noexcept {
    DBUG_ASSERT(false);
  }
  virtual bool is_key_type_valid() {
    DBUG_ASSERT(false);
    return false;
  }
  virtual bool is_key_id_valid() {
    DBUG_ASSERT(false);
    return false;
  }
  virtual bool is_key_valid() {
    DBUG_ASSERT(false);
    return false;
  }
  virtual bool is_key_length_valid() {
    DBUG_ASSERT(false);
    return false;
  }

 private:
  class System_key_data {
   public:
    System_key_data();
    ~System_key_data();

    bool allocate(size_t key_data_size);
    void free();
    uchar *get_key_data();
    size_t get_key_data_size();

   private:
    uchar *key_data;
    size_t key_data_size;
  };

  void construct_system_key_data();

  System_key_data system_key_data;
  uint key_version;
  IKey *keyring_key;
};

}  // namespace keyring
