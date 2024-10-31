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

/**
  @file storage/perfschema/table_data_lock_waits.cc
  Table DATA_LOCK_WAITS (implementation).
*/

#include "storage/perfschema/table_data_lock_waits.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"

THR_LOCK table_data_lock_waits::m_table_lock;

Plugin_table table_data_lock_waits::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "data_lock_waits",
    /* Definition */
    "  ENGINE VARCHAR(32) not null,\n"
    "  REQUESTING_ENGINE_LOCK_ID VARCHAR(128) not null,\n"
    "  REQUESTING_ENGINE_TRANSACTION_ID BIGINT unsigned,\n"
    "  REQUESTING_THREAD_ID BIGINT unsigned,\n"
    "  REQUESTING_EVENT_ID BIGINT unsigned,\n"
    "  REQUESTING_OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  BLOCKING_ENGINE_LOCK_ID VARCHAR(128) not null,\n"
    "  BLOCKING_ENGINE_TRANSACTION_ID BIGINT unsigned,\n"
    "  BLOCKING_THREAD_ID BIGINT unsigned,\n"
    "  BLOCKING_EVENT_ID BIGINT unsigned,\n"
    "  BLOCKING_OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  PRIMARY KEY (REQUESTING_ENGINE_LOCK_ID, BLOCKING_ENGINE_LOCK_ID, "
    "ENGINE) USING HASH,\n"
    "  KEY (REQUESTING_ENGINE_LOCK_ID, ENGINE) USING HASH,\n"
    "  KEY (BLOCKING_ENGINE_LOCK_ID, ENGINE) USING HASH,\n"
    "  KEY (REQUESTING_ENGINE_TRANSACTION_ID, ENGINE) USING HASH,\n"
    "  KEY (BLOCKING_ENGINE_TRANSACTION_ID, ENGINE) USING HASH,\n"
    "  KEY (REQUESTING_THREAD_ID, REQUESTING_EVENT_ID) USING HASH,\n"
    "  KEY (BLOCKING_THREAD_ID, BLOCKING_EVENT_ID) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_data_lock_waits::m_share = {
    &pfs_readonly_acl,
    table_data_lock_waits::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_data_lock_waits::get_row_count,
    sizeof(pk_pos_t),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_data_lock_waits::create(PFS_engine_table_share *) {
  return new table_data_lock_waits();
}

ha_rows table_data_lock_waits::get_row_count() {
  // FIXME
  return 99999;
}

table_data_lock_waits::table_data_lock_waits()
    : PFS_engine_table(&m_share, &m_pk_pos),
      m_row(nullptr),
      m_opened_pk(nullptr),
      m_opened_index(nullptr) {
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++) {
    m_iterator[i] = nullptr;
  }
}

void table_data_lock_waits::destroy_iterators() {
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++) {
    if (m_iterator[i] != nullptr) {
      g_data_lock_inspector[i]->destroy_data_lock_wait_iterator(m_iterator[i]);
      m_iterator[i] = nullptr;
    }
  }
}

table_data_lock_waits::~table_data_lock_waits() { destroy_iterators(); }

void table_data_lock_waits::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
  m_pk_pos.reset();
  m_container.clear();
  destroy_iterators();
}

int table_data_lock_waits::rnd_next() {
  row_data_lock_wait *data;

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_engine();
       m_pos.next_engine()) {
    const unsigned int index = m_pos.m_index_1;

    if (m_iterator[index] == nullptr) {
      if (g_data_lock_inspector[index] == nullptr) {
        continue;
      }

      m_iterator[index] =
          g_data_lock_inspector[index]->create_data_lock_wait_iterator();

      if (m_iterator[index] == nullptr) {
        continue;
      }
    }

    bool iterator_done = false;
    PSI_engine_data_lock_wait_iterator *it = m_iterator[index];

    for (;;) {
      data = m_container.get_row(m_pos.m_index_2);
      if (data != nullptr) {
        m_row = data;
        m_next_pos.set_after(&m_pos);
        m_pk_pos.set(&m_row->m_hidden_pk);
        return 0;
      }

      if (iterator_done) {
        break;
      }

      m_container.shrink();

      /*
        The implementation of PSI_engine_data_lock_wait_iterator::scan(),
        inside a storage engine, is expected to:
        - (1) not report all the data at once,
        - (2) implement re-startable scans internally,
        - (3) report a bounded number of rows per scan.

        This is to allow allocating only a bounded amount of memory
        in the data container, to cap the peak memory consumption
        of the container.

        TODO: Innodb_data_lock_wait_iterator::scan()
        does not satisfy (3) currently.
      */

      iterator_done = it->scan(&m_container);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_data_lock_waits::rnd_pos(const void *pos) {
  row_data_lock_wait *data;

  set_position(pos);

  /*
    TODO: Multiple engine support.
    Find the proper engine based on column ENGINE.
  */
  static_assert(COUNT_DATA_LOCK_ENGINES == 1,
                "We don't support multiple engines yet.");
  constexpr unsigned int index = 0;

  if (m_iterator[index] == nullptr) {
    if (g_data_lock_inspector[index] == nullptr) {
      return HA_ERR_RECORD_DELETED;
    }

    m_iterator[index] =
        g_data_lock_inspector[index]->create_data_lock_wait_iterator();

    if (m_iterator[index] == nullptr) {
      return HA_ERR_RECORD_DELETED;
    }
  }

  PSI_engine_data_lock_wait_iterator *it = m_iterator[index];

  m_container.clear();
  it->fetch(&m_container, m_pk_pos.get_requesting_lock_id(),
            m_pk_pos.get_requesting_lock_id_length(),
            m_pk_pos.get_blocking_lock_id(),
            m_pk_pos.get_blocking_lock_id_length());
  data = m_container.get_row(0);
  if (data != nullptr) {
    m_row = data;
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

int table_data_lock_waits::index_init(uint idx, bool) {
  PFS_pk_data_lock_waits *pk = nullptr;
  PFS_index_data_lock_waits *index = nullptr;

  switch (idx) {
    case 0:
      pk = PFS_NEW(PFS_pk_data_lock_waits);
      index = pk;
      break;
    case 1:
      index = PFS_NEW(PFS_index_data_lock_waits_by_requesting_lock_id);
      break;
    case 2:
      index = PFS_NEW(PFS_index_data_lock_waits_by_blocking_lock_id);
      break;
    case 3:
      index = PFS_NEW(PFS_index_data_lock_waits_by_requesting_transaction_id);
      break;
    case 4:
      index = PFS_NEW(PFS_index_data_lock_waits_by_blocking_transaction_id);
      break;
    case 5:
      index = PFS_NEW(PFS_index_data_lock_waits_by_requesting_thread_id);
      break;
    case 6:
      index = PFS_NEW(PFS_index_data_lock_waits_by_blocking_thread_id);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_pk = pk;
  m_opened_index = index;
  m_index = index;

  m_container.set_filter(m_opened_index);
  return 0;
}

int table_data_lock_waits::index_next() {
  int status;

  if (m_opened_pk != nullptr) {
    pk_pos_data_lock_wait *position = m_opened_pk->get_pk();
    /*
     * In the ideal case when:
     * - the opened index is the PRIMARY KEY
     * - the keypart field REQUESTING_ENGINE_LOCK_ID is provided
     * - the keypart field BLOCKING_ENGINE_LOCK_ID is provided
     * - the index fetch is an exact match HA_READ_KEY_EXACT
     * then we can inspect the REQUESTING_ENGINE_LOCK_ID
     * and BLOCKING_ENGINE_LOCK_ID values,
     * and perform a PSI_engine_data_lock_wait_iterator::fetch()
     * in the underlying storage engine.
     *
     * Evaluating the condition in the third part
     * of the primary key, ENGINE, will be done as
     * an index condition pushdown when adding rows
     * to the container, filtered by
     * PFS_pk_data_lock_waits::match_engine().
     */
    if (position != nullptr) {
      if (m_opened_pk->m_key_fetch_count == 0) {
        status = rnd_pos(position);
        if (status != 0) {
          status = HA_ERR_KEY_NOT_FOUND;
        }
      } else {
        status = HA_ERR_KEY_NOT_FOUND;
      }

      m_opened_pk->m_key_fetch_count++;
      return status;
    }
  }

  /*
   * For every other cases:
   * - index is the PRIMARY KEY, but both fields
   *   REQUESTING_ENGINE_LOCK_ID and BLOCKING_ENGINE_LOCK_ID are not available
   *   (for example, only REQUESTING_ENGINE_LOCK_ID is provided)
   * - index is not the PRIMARY KEY
   * we execute a scan, with filtering done as an index condition pushdown,
   * attached to the data container.
   */
  status = rnd_next();

  return status;
}

int table_data_lock_waits::read_row_values(TABLE *table, unsigned char *buf,
                                           Field **fields, bool read_all) {
  Field *f;

  if (unlikely(m_row == nullptr)) {
    return HA_ERR_RECORD_DELETED;
  }

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* ENGINE */
          set_field_varchar_utf8mb4(f, m_row->m_engine);
          break;
        case 1: /* REQUESTING_ENGINE_LOCK_ID */
          set_field_varchar_utf8mb4(
              f, m_row->m_hidden_pk.get_requesting_lock_id(),
              m_row->m_hidden_pk.get_requesting_lock_id_length());
          break;
        case 2: /* REQUESTING_ENGINE_TRANSACTION_ID */
          set_field_ulonglong(f, m_row->m_requesting_transaction_id);
          break;
        case 3: /* REQUESTING_THREAD_ID */
          set_field_ulonglong(f, m_row->m_requesting_thread_id);
          break;
        case 4: /* REQUESTING_EVENT_ID */
          set_field_ulonglong(f, m_row->m_requesting_event_id);
          break;
        case 5: /* REQUESTING_OBJECT_INSTANCE_BEGIN */
          set_field_ulonglong(f, (intptr)m_row->m_requesting_identity);
          break;
        case 6: /* BLOCKING_ENGINE_LOCK_ID */
          set_field_varchar_utf8mb4(
              f, m_row->m_hidden_pk.get_blocking_lock_id(),
              m_row->m_hidden_pk.get_blocking_lock_id_length());
          break;
        case 7: /* BLOCKING_ENGINE_TRANSACTION_ID */
          set_field_ulonglong(f, m_row->m_blocking_transaction_id);
          break;
        case 8: /* BLOCKING_THREAD_ID */
          set_field_ulonglong(f, m_row->m_blocking_thread_id);
          break;
        case 9: /* BLOCKING_EVENT_ID */
          set_field_ulonglong(f, m_row->m_blocking_event_id);
          break;
        case 10: /* BLOCKING_OBJECT_INSTANCE_BEGIN */
          set_field_ulonglong(f, (intptr)m_row->m_blocking_identity);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
