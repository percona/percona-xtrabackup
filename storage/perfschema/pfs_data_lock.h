/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef PFS_DATA_LOCK_H
#define PFS_DATA_LOCK_H

/**
  @file storage/perfschema/pfs_data_lock.h
  Performance schema instrumentation (declarations).
*/

#include <mysql/psi/psi_data_lock.h>
#include <unordered_set>
#include <vector>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_std_allocator.h"
#include "storage/perfschema/table_helper.h"

struct pk_pos_data_lock {
 public:
  static constexpr size_t max_len = 128;

  pk_pos_data_lock() { reset(); }

  void reset() {
    memset(m_engine_lock_id, 0, sizeof(m_engine_lock_id));
    m_engine_lock_id_length = 0;
  }

  void set(const pk_pos_data_lock *other) {
    memcpy(m_engine_lock_id, other->m_engine_lock_id, sizeof(m_engine_lock_id));
    m_engine_lock_id_length = other->m_engine_lock_id_length;
  }

  void set(const char *str, size_t len) {
    if ((len == 0) || (len > max_len) || (str == nullptr)) {
      reset();
      return;
    }

    memcpy(m_engine_lock_id, str, len);
    if (len < max_len) {
      memset(&m_engine_lock_id[len], 0, max_len - len);
    }

    m_engine_lock_id_length = len;
  }

  const char *str() const { return m_engine_lock_id; }
  size_t length() const { return m_engine_lock_id_length; }

 private:
  /** Column ENGINE_LOCK_ID */
  char m_engine_lock_id[max_len];
  size_t m_engine_lock_id_length;
};

// This structure is memcmp-ed, so we need to have no padding.
static_assert(sizeof(pk_pos_data_lock) == 128 + sizeof(size_t));

/** A row of table PERFORMANCE_SCHEMA.DATA_LOCKS. */
struct row_data_lock {
  /** Column ENGINE */
  const char *m_engine;
  /** Column ENGINE_LOCK_ID */
  pk_pos_data_lock m_hidden_pk;
  /** Column ENGINE_TRANSACTION_ID */
  ulonglong m_transaction_id;
  /** Column THREAD_ID */
  ulonglong m_thread_id;
  /** Column EVENT_ID */
  ulonglong m_event_id;
  /** Columns OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME */
  PFS_index_view_row m_index_row;
  /** Column PARTITION_NAME */
  const char *m_partition_name;
  size_t m_partition_name_length;
  /** Column SUB_PARTITION_NAME */
  const char *m_sub_partition_name;
  size_t m_sub_partition_name_length;
  /** Column OBJECT_INSTANCE_BEGIN */
  const void *m_identity;
  /** Column LOCK_MODE */
  const char *m_lock_mode;
  /** Column LOCK_TYPE */
  const char *m_lock_type;
  /** Column LOCK_STATUS */
  const char *m_lock_status;
  /** Column LOCK_DATA */
  const char *m_lock_data;
};

struct pk_pos_data_lock_wait {
 public:
  static constexpr size_t max_len = 128;

  pk_pos_data_lock_wait() { reset(); }

  void reset() {
    // POT type, must initialize every byte for memcmp()
    memset(m_requesting_engine_lock_id, 0, sizeof(m_requesting_engine_lock_id));
    m_requesting_engine_lock_id_length = 0;
    memset(m_blocking_engine_lock_id, 0, sizeof(m_blocking_engine_lock_id));
    m_blocking_engine_lock_id_length = 0;
  }

  void set(const pk_pos_data_lock_wait *other) {
    memcpy(m_requesting_engine_lock_id, other->m_requesting_engine_lock_id,
           sizeof(m_requesting_engine_lock_id));
    m_requesting_engine_lock_id_length =
        other->m_requesting_engine_lock_id_length;
    memcpy(m_blocking_engine_lock_id, other->m_blocking_engine_lock_id,
           sizeof(m_blocking_engine_lock_id));
    m_blocking_engine_lock_id_length = other->m_blocking_engine_lock_id_length;
  }

  void set(const char *requesting_lock_id, size_t requesting_lock_id_length,
           const char *blocking_lock_id, size_t blocking_lock_id_length) {
    if ((requesting_lock_id_length == 0) ||
        (requesting_lock_id_length > max_len) ||
        (requesting_lock_id == nullptr) || (blocking_lock_id_length == 0) ||
        (blocking_lock_id_length > max_len) || (blocking_lock_id == nullptr)) {
      reset();
      return;
    }

    // POT type, must initialize every byte for memcmp()

    memcpy(m_requesting_engine_lock_id, requesting_lock_id,
           requesting_lock_id_length);
    if (requesting_lock_id_length < max_len) {
      memset(&m_requesting_engine_lock_id[requesting_lock_id_length], 0,
             max_len - requesting_lock_id_length);
    }

    m_requesting_engine_lock_id_length = requesting_lock_id_length;

    memcpy(m_blocking_engine_lock_id, blocking_lock_id,
           blocking_lock_id_length);
    if (blocking_lock_id_length < max_len) {
      memset(&m_blocking_engine_lock_id[blocking_lock_id_length], 0,
             max_len - blocking_lock_id_length);
    }

    m_blocking_engine_lock_id_length = blocking_lock_id_length;
  }

  const char *get_requesting_lock_id() const {
    return m_requesting_engine_lock_id;
  }

  size_t get_requesting_lock_id_length() const {
    return m_requesting_engine_lock_id_length;
  }

  const char *get_blocking_lock_id() const { return m_blocking_engine_lock_id; }

  size_t get_blocking_lock_id_length() const {
    return m_blocking_engine_lock_id_length;
  }

 private:
  /** Column REQUESTING_ENGINE_LOCK_ID */
  char m_requesting_engine_lock_id[max_len];
  size_t m_requesting_engine_lock_id_length;
  /** Column BLOCKING_ENGINE_LOCK_ID */
  char m_blocking_engine_lock_id[max_len];
  size_t m_blocking_engine_lock_id_length;
};

// This structure is memcmp-ed, so we need to have no padding.
static_assert(sizeof(pk_pos_data_lock_wait) == 2 * (128 + sizeof(size_t)));

/** A row of table PERFORMANCE_SCHEMA.DATA_LOCK_WAITS. */
struct row_data_lock_wait {
  /** Column ENGINE */
  const char *m_engine{nullptr};
  /** Engine (REQUESTING_LOCK_ID, BLOCKING_LOCK_ID) key */
  pk_pos_data_lock_wait m_hidden_pk;
  /** Column REQUESTING_ENGINE_TRANSACTION_ID */
  ulonglong m_requesting_transaction_id{0};
  /** Column REQUESTING_THREAD_ID */
  ulonglong m_requesting_thread_id{0};
  /** Column REQUESTING_EVENT_ID */
  ulonglong m_requesting_event_id{0};
  /** Column REQUESTING_OBJECT_INSTANCE_BEGIN */
  const void *m_requesting_identity{nullptr};
  /** Column BLOCKING_ENGINE_TRANSACTION_ID */
  ulonglong m_blocking_transaction_id{0};
  /** Column BLOCKING_THREAD_ID */
  ulonglong m_blocking_thread_id{0};
  /** Column BLOCKING_EVENT_ID */
  ulonglong m_blocking_event_id{0};
  /** Column BLOCKING_OBJECT_INSTANCE_BEGIN */
  const void *m_blocking_identity{nullptr};
};

class PFS_index_data_locks : public PFS_engine_index {
 public:
  PFS_index_data_locks(PFS_engine_key *key_1, PFS_engine_key *key_2)
      : PFS_engine_index(key_1, key_2) {}

  PFS_index_data_locks(PFS_engine_key *key_1, PFS_engine_key *key_2,
                       PFS_engine_key *key_3, PFS_engine_key *key_4)
      : PFS_engine_index(key_1, key_2, key_3, key_4) {}

  ~PFS_index_data_locks() override = default;

  virtual bool match_engine(const char *engine [[maybe_unused]],
                            size_t engine_length [[maybe_unused]]) {
    return true;
  }

  virtual bool match_lock_id(const char *engine_lock_id [[maybe_unused]],
                             size_t engine_lock_id_length [[maybe_unused]]) {
    return true;
  }

  virtual bool match_transaction_id(ulonglong engine_transaction_id
                                    [[maybe_unused]]) {
    return true;
  }

  virtual bool match_thread_id_event_id(ulonglong thread_id [[maybe_unused]],
                                        ulonglong event_id [[maybe_unused]]) {
    return true;
  }

  virtual bool match_object(const char *table_schema [[maybe_unused]],
                            size_t table_schema_length [[maybe_unused]],
                            const char *table_name [[maybe_unused]],
                            size_t table_name_length [[maybe_unused]],
                            const char *partition_name [[maybe_unused]],
                            size_t partition_name_length [[maybe_unused]],
                            const char *sub_partition_name [[maybe_unused]],
                            size_t sub_partition_name_length [[maybe_unused]]) {
    return true;
  }
};

class PFS_pk_data_locks : public PFS_index_data_locks {
 public:
  PFS_pk_data_locks()
      : PFS_index_data_locks(&m_key_1, &m_key_2),
        m_key_1("ENGINE_LOCK_ID"),
        m_key_2("ENGINE") {}

  ~PFS_pk_data_locks() override = default;

  pk_pos_data_lock *get_pk() {
    if (m_fields >= 1) {
      const char *key_value;
      size_t key_value_length;
      bool is_null;

      /* Read the value of ENGINE_LOCK_ID. */
      m_key_1.get_exact_key_value(is_null, key_value, key_value_length);
      if (is_null) {
        return nullptr;
      }

      /* Build a primary key with it. */
      m_pk_pos.set(key_value, key_value_length);

      /*
       * IMPORTANT NOTE:
       * We do not read the second field, ENGINE.
       * See comments in table_data_locks::index_next()
       */
      return &m_pk_pos;
    }

    return nullptr;
  }

  bool match_lock_id(const char *engine_lock_id,
                     size_t engine_lock_id_length) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(engine_lock_id, engine_lock_id_length)) {
        return false;
      }
    }

    return true;
  }

  bool match_engine(const char *engine, size_t engine_length) override {
    if (m_fields >= 2) {
      if (!m_key_2.match(engine, engine_length)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_engine_lock_id m_key_1;
  PFS_key_engine_name m_key_2;
  pk_pos_data_lock m_pk_pos;
};

class PFS_index_data_locks_by_transaction_id : public PFS_index_data_locks {
 public:
  PFS_index_data_locks_by_transaction_id()
      : PFS_index_data_locks(&m_key_1, &m_key_2),
        m_key_1("ENGINE_TRANSACTION_ID"),
        m_key_2("ENGINE") {}

  ~PFS_index_data_locks_by_transaction_id() override = default;

  bool match_transaction_id(ulonglong engine_transaction_id) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(engine_transaction_id)) {
        return false;
      }
    }

    return true;
  }

  bool match_engine(const char *engine, size_t engine_length) override {
    if (m_fields >= 2) {
      if (!m_key_2.match(engine, engine_length)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_engine_transaction_id m_key_1;
  PFS_key_engine_name m_key_2;
};

class PFS_index_data_locks_by_thread_id : public PFS_index_data_locks {
 public:
  PFS_index_data_locks_by_thread_id()
      : PFS_index_data_locks(&m_key_1, &m_key_2),
        m_key_1("THREAD_ID"),
        m_key_2("EVENT_ID") {}

  ~PFS_index_data_locks_by_thread_id() override = default;

  bool match_thread_id_event_id(ulonglong thread_id,
                                ulonglong event_id) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(thread_id)) {
        return false;
      }
    }

    if (m_fields >= 2) {
      if (!m_key_2.match(event_id)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_thread_id m_key_1;
  PFS_key_event_id m_key_2;
};

class PFS_index_data_locks_by_object : public PFS_index_data_locks {
 public:
  PFS_index_data_locks_by_object()
      : PFS_index_data_locks(&m_key_1, &m_key_2, &m_key_3, &m_key_4),
        m_key_1("OBJECT_SCHEMA"),
        m_key_2("OBJECT_NAME"),
        m_key_3("PARTITION_NAME"),
        m_key_4("SUBPARTITION_NAME") {}

  ~PFS_index_data_locks_by_object() override = default;

  bool match_object(const char *table_schema, size_t table_schema_length,
                    const char *table_name, size_t table_name_length,
                    const char *partition_name, size_t partition_name_length,
                    const char *sub_partition_name,
                    size_t sub_partition_name_length) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(table_schema, table_schema_length)) {
        return false;
      }
    }

    if (m_fields >= 2) {
      if (!m_key_2.match(table_name, table_name_length)) {
        return false;
      }
    }

    if (m_fields >= 3) {
      if (!m_key_3.match(partition_name, partition_name_length)) {
        return false;
      }
    }

    if (m_fields >= 4) {
      if (!m_key_4.match(sub_partition_name, sub_partition_name_length)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_object_schema m_key_1;
  PFS_key_object_name m_key_2;
  PFS_key_name m_key_3;
  PFS_key_name m_key_4;
};

class PFS_index_data_lock_waits : public PFS_engine_index {
 public:
  PFS_index_data_lock_waits(PFS_engine_key *key_1, PFS_engine_key *key_2)
      : PFS_engine_index(key_1, key_2) {}

  PFS_index_data_lock_waits(PFS_engine_key *key_1, PFS_engine_key *key_2,
                            PFS_engine_key *key_3)
      : PFS_engine_index(key_1, key_2, key_3) {}

  ~PFS_index_data_lock_waits() override = default;

  virtual bool match_engine(const char *engine [[maybe_unused]],
                            size_t engine_length [[maybe_unused]]) {
    return true;
  }

  virtual bool match_requesting_lock_id(const char *engine_lock_id
                                        [[maybe_unused]],
                                        size_t engine_lock_id_length
                                        [[maybe_unused]]) {
    return true;
  }

  virtual bool match_blocking_lock_id(const char *engine_lock_id
                                      [[maybe_unused]],
                                      size_t engine_lock_id_length
                                      [[maybe_unused]]) {
    return true;
  }

  virtual bool match_requesting_transaction_id(ulonglong engine_transaction_id
                                               [[maybe_unused]]) {
    return true;
  }

  virtual bool match_blocking_transaction_id(ulonglong engine_transaction_id
                                             [[maybe_unused]]) {
    return true;
  }

  virtual bool match_requesting_thread_id_event_id(ulonglong thread_id
                                                   [[maybe_unused]],
                                                   ulonglong event_id
                                                   [[maybe_unused]]) {
    return true;
  }

  virtual bool match_blocking_thread_id_event_id(ulonglong thread_id
                                                 [[maybe_unused]],
                                                 ulonglong event_id
                                                 [[maybe_unused]]) {
    return true;
  }
};

class PFS_pk_data_lock_waits : public PFS_index_data_lock_waits {
 public:
  PFS_pk_data_lock_waits()
      : PFS_index_data_lock_waits(&m_key_1, &m_key_2, &m_key_3),
        m_key_1("REQUESTING_ENGINE_LOCK_ID"),
        m_key_2("BLOCKING_ENGINE_LOCK_ID"),
        m_key_3("ENGINE") {}

  ~PFS_pk_data_lock_waits() override = default;

  pk_pos_data_lock_wait *get_pk() {
    if (m_fields >= 2) {
      const char *key_value_1;
      size_t key_value_length_1;
      bool is_null_1;
      const char *key_value_2;
      size_t key_value_length_2;
      bool is_null_2;

      /* Read the value of REQUESTING_ENGINE_LOCK_ID. */
      m_key_1.get_exact_key_value(is_null_1, key_value_1, key_value_length_1);
      if (is_null_1) {
        return nullptr;
      }

      /* Read the value of BLOCKING_ENGINE_LOCK_ID. */
      m_key_2.get_exact_key_value(is_null_2, key_value_2, key_value_length_2);
      if (is_null_2) {
        return nullptr;
      }

      /* Build a primary key with it. */
      m_pk_pos.set(key_value_1, key_value_length_1, key_value_2,
                   key_value_length_2);

      /*
       * IMPORTANT NOTE:
       * We do not read the third field, ENGINE.
       * See comments in table_data_lock_waits::index_next()
       */
      return &m_pk_pos;
    }

    return nullptr;
  }

  bool match_requesting_lock_id(const char *engine_lock_id,
                                size_t engine_lock_id_length) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(engine_lock_id, engine_lock_id_length)) {
        return false;
      }
    }

    return true;
  }

  bool match_blocking_lock_id(const char *engine_lock_id,
                              size_t engine_lock_id_length) override {
    if (m_fields >= 2) {
      if (!m_key_2.match(engine_lock_id, engine_lock_id_length)) {
        return false;
      }
    }

    return true;
  }

  bool match_engine(const char *engine, size_t engine_length) override {
    if (m_fields >= 3) {
      if (!m_key_3.match(engine, engine_length)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_engine_lock_id m_key_1;
  PFS_key_engine_lock_id m_key_2;
  PFS_key_engine_name m_key_3;
  pk_pos_data_lock_wait m_pk_pos;
};

class PFS_index_data_lock_waits_by_requesting_lock_id
    : public PFS_index_data_lock_waits {
 public:
  PFS_index_data_lock_waits_by_requesting_lock_id()
      : PFS_index_data_lock_waits(&m_key_1, &m_key_2),
        m_key_1("REQUESTING_ENGINE_LOCK_ID"),
        m_key_2("ENGINE") {}

  ~PFS_index_data_lock_waits_by_requesting_lock_id() override = default;

  bool match_requesting_lock_id(const char *engine_lock_id,
                                size_t engine_lock_id_length) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(engine_lock_id, engine_lock_id_length)) {
        return false;
      }
    }

    return true;
  }

  bool match_engine(const char *engine, size_t engine_length) override {
    if (m_fields >= 2) {
      if (!m_key_2.match(engine, engine_length)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_engine_lock_id m_key_1;
  PFS_key_engine_name m_key_2;
};

class PFS_index_data_lock_waits_by_blocking_lock_id
    : public PFS_index_data_lock_waits {
 public:
  PFS_index_data_lock_waits_by_blocking_lock_id()
      : PFS_index_data_lock_waits(&m_key_1, &m_key_2),
        m_key_1("BLOCKING_ENGINE_LOCK_ID"),
        m_key_2("ENGINE") {}

  ~PFS_index_data_lock_waits_by_blocking_lock_id() override = default;

  bool match_blocking_lock_id(const char *engine_lock_id,
                              size_t engine_lock_id_length) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(engine_lock_id, engine_lock_id_length)) {
        return false;
      }
    }

    return true;
  }

  bool match_engine(const char *engine, size_t engine_length) override {
    if (m_fields >= 2) {
      if (!m_key_2.match(engine, engine_length)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_engine_lock_id m_key_1;
  PFS_key_engine_name m_key_2;
};

class PFS_index_data_lock_waits_by_requesting_transaction_id
    : public PFS_index_data_lock_waits {
 public:
  PFS_index_data_lock_waits_by_requesting_transaction_id()
      : PFS_index_data_lock_waits(&m_key_1, &m_key_2),
        m_key_1("REQUESTING_ENGINE_TRANSACTION_ID"),
        m_key_2("ENGINE") {}

  ~PFS_index_data_lock_waits_by_requesting_transaction_id() override = default;

  bool match_requesting_transaction_id(
      ulonglong engine_transaction_id) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(engine_transaction_id)) {
        return false;
      }
    }

    return true;
  }

  bool match_engine(const char *engine, size_t engine_length) override {
    if (m_fields >= 2) {
      if (!m_key_2.match(engine, engine_length)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_engine_transaction_id m_key_1;
  PFS_key_engine_name m_key_2;
};

class PFS_index_data_lock_waits_by_blocking_transaction_id
    : public PFS_index_data_lock_waits {
 public:
  PFS_index_data_lock_waits_by_blocking_transaction_id()
      : PFS_index_data_lock_waits(&m_key_1, &m_key_2),
        m_key_1("BLOCKING_ENGINE_TRANSACTION_ID"),
        m_key_2("ENGINE") {}

  ~PFS_index_data_lock_waits_by_blocking_transaction_id() override = default;

  bool match_blocking_transaction_id(ulonglong engine_transaction_id) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(engine_transaction_id)) {
        return false;
      }
    }

    return true;
  }

  bool match_engine(const char *engine, size_t engine_length) override {
    if (m_fields >= 2) {
      if (!m_key_2.match(engine, engine_length)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_engine_transaction_id m_key_1;
  PFS_key_engine_name m_key_2;
};

class PFS_index_data_lock_waits_by_requesting_thread_id
    : public PFS_index_data_lock_waits {
 public:
  PFS_index_data_lock_waits_by_requesting_thread_id()
      : PFS_index_data_lock_waits(&m_key_1, &m_key_2),
        m_key_1("REQUESTING_THREAD_ID"),
        m_key_2("REQUESTING_EVENT_ID") {}

  ~PFS_index_data_lock_waits_by_requesting_thread_id() override = default;

  bool match_requesting_thread_id_event_id(ulonglong thread_id,
                                           ulonglong event_id) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(thread_id)) {
        return false;
      }
    }

    if (m_fields >= 2) {
      if (!m_key_2.match(event_id)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_thread_id m_key_1;
  PFS_key_event_id m_key_2;
};

class PFS_index_data_lock_waits_by_blocking_thread_id
    : public PFS_index_data_lock_waits {
 public:
  PFS_index_data_lock_waits_by_blocking_thread_id()
      : PFS_index_data_lock_waits(&m_key_1, &m_key_2),
        m_key_1("BLOCKING_THREAD_ID"),
        m_key_2("BLOCKING_EVENT_ID") {}

  ~PFS_index_data_lock_waits_by_blocking_thread_id() override = default;

  bool match_blocking_thread_id_event_id(ulonglong thread_id,
                                         ulonglong event_id) override {
    if (m_fields >= 1) {
      if (!m_key_1.match(thread_id)) {
        return false;
      }
    }

    if (m_fields >= 2) {
      if (!m_key_2.match(event_id)) {
        return false;
      }
    }

    return true;
  }

 private:
  PFS_key_thread_id m_key_1;
  PFS_key_event_id m_key_2;
};

template <class T>
class PFS_data_container_allocator : public PFS_std_allocator<T> {
 public:
  PFS_data_container_allocator()
      : PFS_std_allocator<T>(&builtin_memory_data_container) {}

  PFS_data_container_allocator(const PFS_data_container_allocator &other)
      : PFS_std_allocator<T>(other) {}

  template <class U>
  constexpr explicit PFS_data_container_allocator(
      const PFS_data_container_allocator<U> &u) noexcept
      : PFS_std_allocator<T>(u) {}
};

class PFS_data_cache {
 private:
  typedef std::unordered_set<std::string, std::hash<std::string>,
                             std::equal_to<>,
                             PFS_data_container_allocator<std::string>>
      set_type;

  set_type m_set;

 public:
  PFS_data_cache() = default;
  ~PFS_data_cache() = default;

  const char *cache_data(const char *ptr, size_t length) {
    /*
      std::string is just a sequence of bytes,
      which actually can contain a 0 byte ...
      Never use strlen() on the binary data.
    */
    const std::string key(ptr, length);
    std::pair<set_type::iterator, bool> ret;

    ret = m_set.insert(key);
    return (*ret.first).data();
  }

  void clear() { m_set.clear(); }
};

class PFS_data_lock_container : public PSI_server_data_lock_container {
 public:
  PFS_data_lock_container();
  ~PFS_data_lock_container() override;

  const char *cache_string(const char *string) override;
  const char *cache_data(const char *ptr, size_t length) override;
  void cache_identifier(PSI_identifier kind, const char *str, size_t length,
                        const char **cached_ptr,
                        size_t *cached_length) override;

  bool accept_engine(const char *engine, size_t engine_length) override;
  bool accept_lock_id(const char *engine_lock_id,
                      size_t engine_lock_id_length) override;
  bool accept_transaction_id(ulonglong transaction_id) override;
  bool accept_thread_id_event_id(ulonglong thread_id,
                                 ulonglong event_id) override;
  bool accept_object(const char *table_schema, size_t table_schema_length,
                     const char *table_name, size_t table_name_length,
                     const char *partition_name, size_t partition_name_length,
                     const char *sub_partition_name,
                     size_t sub_partition_name_length) override;

  void add_lock_row(const char *engine, size_t engine_length,
                    const char *engine_lock_id, size_t engine_lock_id_length,
                    ulonglong transaction_id, ulonglong thread_id,
                    ulonglong event_id, const char *table_schema,
                    size_t table_schema_length, const char *table_name,
                    size_t table_name_length, const char *partition_name,
                    size_t partition_name_length,
                    const char *sub_partition_name,
                    size_t sub_partition_name_length, const char *index_name,
                    size_t index_name_length, const void *identity,
                    const char *lock_mode, const char *lock_type,
                    const char *lock_status, const char *lock_data) override;

  /**
    Clear the container.
    New rows added will start at index 0.
  */
  void clear();
  /**
    Shrink the container.
    New rows added will continue to use the current index.
  */
  void shrink();
  row_data_lock *get_row(size_t index);

  void set_filter(PFS_index_data_locks *filter) { m_filter = filter; }

 private:
  size_t m_logical_row_index;
  std::vector<row_data_lock, PFS_data_container_allocator<row_data_lock>>
      m_rows;
  PFS_data_cache m_cache;
  PFS_index_data_locks *m_filter;
};

class PFS_data_lock_wait_container
    : public PSI_server_data_lock_wait_container {
 public:
  PFS_data_lock_wait_container();
  ~PFS_data_lock_wait_container() override;

  const char *cache_string(const char *string) override;
  const char *cache_data(const char *ptr, size_t length) override;

  bool accept_engine(const char *engine, size_t engine_length) override;
  bool accept_requesting_lock_id(const char *engine_lock_id,
                                 size_t engine_lock_id_length) override;
  bool accept_blocking_lock_id(const char *engine_lock_id,
                               size_t engine_lock_id_length) override;
  bool accept_requesting_transaction_id(ulonglong transaction_id) override;
  bool accept_blocking_transaction_id(ulonglong transaction_id) override;
  bool accept_requesting_thread_id_event_id(ulonglong thread_id,
                                            ulonglong event_id) override;
  bool accept_blocking_thread_id_event_id(ulonglong thread_id,
                                          ulonglong event_id) override;

  void add_lock_wait_row(
      const char *engine, size_t engine_length,
      const char *requesting_engine_lock_id,
      size_t requesting_engine_lock_id_length,
      ulonglong requesting_transaction_id, ulonglong requesting_thread_id,
      ulonglong requesting_event_id, const void *requesting_identity,
      const char *blocking_engine_lock_id,
      size_t blocking_engine_lock_id_length, ulonglong blocking_transaction_id,
      ulonglong blocking_thread_id, ulonglong blocking_event_id,
      const void *blocking_identity) override;

  /**
    Clear the container.
    New rows added will start at index 0.
  */
  void clear();
  /**
    Shrink the container.
    New rows added will continue to use the current index.
  */
  void shrink();
  row_data_lock_wait *get_row(size_t index);

  void set_filter(PFS_index_data_lock_waits *filter) { m_filter = filter; }

 private:
  size_t m_logical_row_index;
  std::vector<row_data_lock_wait,
              PFS_data_container_allocator<row_data_lock_wait>>
      m_rows;
  PFS_data_cache m_cache;
  PFS_index_data_lock_waits *m_filter;
};

#endif
