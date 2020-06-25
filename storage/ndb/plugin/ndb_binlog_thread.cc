/*
   Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements
#include "storage/ndb/plugin/ndb_binlog_thread.h"

#include <cstdint>

// Using
#include "m_string.h"          // NullS
#include "mysql/status_var.h"  // enum_mysql_show_type
#include "sql/current_thd.h"   // current_thd
#include "storage/ndb/plugin/ndb_global_schema_lock_guard.h"  // Ndb_global_schema_lock_guard
#include "storage/ndb/plugin/ndb_local_connection.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_metadata_change_monitor.h"

int Ndb_binlog_thread::do_init() {
  if (!binlog_hooks.register_hooks(do_after_reset_master)) {
    ndb_log_error("Failed to register binlog hooks");
    return 1;
  }
  return 0;
}

int Ndb_binlog_thread::do_deinit() {
  binlog_hooks.unregister_all();
  return 0;
}

/*
  @brief Callback called when RESET MASTER has successfully removed binlog and
  reset index. This means that ndbcluster also need to clear its own binlog
  index(which is stored in the mysql.ndb_binlog_index table).

  @return 0 on sucess
*/
int Ndb_binlog_thread::do_after_reset_master(void *) {
  DBUG_TRACE;

  // Truncate the mysql.ndb_binlog_index table
  // - if table does not exist ignore the error as it is a
  // "consistent" behavior
  Ndb_local_connection mysqld(current_thd);
  const bool ignore_no_such_table = true;
  if (mysqld.truncate_table("mysql", "ndb_binlog_index",
                            ignore_no_such_table)) {
    // Failed to truncate table
    return 1;
  }
  return 0;
}

void Ndb_binlog_thread::validate_sync_blacklist(THD *thd) {
  metadata_sync.validate_blacklist(thd);
}

bool Ndb_binlog_thread::add_logfile_group_to_check(
    const std::string &lfg_name) {
  return metadata_sync.add_logfile_group(lfg_name);
}

bool Ndb_binlog_thread::add_tablespace_to_check(
    const std::string &tablespace_name) {
  return metadata_sync.add_tablespace(tablespace_name);
}

bool Ndb_binlog_thread::add_schema_to_check(const std::string &schema_name) {
  return metadata_sync.add_schema(schema_name);
}

bool Ndb_binlog_thread::add_table_to_check(const std::string &db_name,
                                           const std::string &table_name) {
  return metadata_sync.add_table(db_name, table_name);
}

static int64_t g_metadata_synced_count = 0;
static void increment_metadata_synced_count() { g_metadata_synced_count++; }

static SHOW_VAR ndb_status_vars_metadata_synced[] = {
    {"metadata_synced_count",
     reinterpret_cast<char *>(&g_metadata_synced_count), SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

int show_ndb_metadata_synced(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = reinterpret_cast<char *>(&ndb_status_vars_metadata_synced);
  return 0;
}

void Ndb_binlog_thread::synchronize_detected_object(THD *thd) {
  if (metadata_sync.object_queue_empty()) {
    // No objects pending sync
    Ndb_metadata_change_monitor::sync_done();
    return;
  }

  Ndb_global_schema_lock_guard global_schema_lock_guard(thd);
  if (!global_schema_lock_guard.try_lock()) {
    // Failed to obtain GSL
    return;
  }

  // Synchronize 1 object from the queue
  std::string schema_name, object_name;
  object_detected_type object_type;
  metadata_sync.get_next_object(schema_name, object_name, object_type);
  switch (object_type) {
    case object_detected_type::LOGFILE_GROUP_OBJECT: {
      bool temp_error;
      if (metadata_sync.sync_logfile_group(thd, object_name, temp_error)) {
        log_info("Logfile group '%s' successfully synchronized",
                 object_name.c_str());
        increment_metadata_synced_count();
      } else if (temp_error) {
        log_info(
            "Failed to synchronize logfile group '%s' due to a temporary "
            "error",
            object_name.c_str());
      } else {
        log_error("Failed to synchronize logfile group '%s'",
                  object_name.c_str());
        metadata_sync.add_object_to_blacklist(schema_name, object_name,
                                              object_type);
        increment_metadata_synced_count();
      }
      break;
    }
    case object_detected_type::TABLESPACE_OBJECT: {
      bool temp_error;
      if (metadata_sync.sync_tablespace(thd, object_name, temp_error)) {
        log_info("Tablespace '%s' successfully synchronized",
                 object_name.c_str());
        increment_metadata_synced_count();
      } else if (temp_error) {
        log_info(
            "Failed to synchronize tablespace '%s' due to a temporary "
            "error",
            object_name.c_str());
      } else {
        log_error("Failed to synchronize tablespace '%s'", object_name.c_str());
        metadata_sync.add_object_to_blacklist(schema_name, object_name,
                                              object_type);
        increment_metadata_synced_count();
      }
      break;
    }
    case object_detected_type::SCHEMA_OBJECT: {
      bool temp_error;
      if (metadata_sync.sync_schema(thd, schema_name, temp_error)) {
        log_info("Schema '%s' successfully synchronized", schema_name.c_str());
        increment_metadata_synced_count();
      } else if (temp_error) {
        log_info("Failed to synchronize schema '%s' due to a temporary error",
                 schema_name.c_str());
      } else {
        log_error("Failed to synchronize schema '%s'", schema_name.c_str());
        metadata_sync.add_object_to_blacklist(schema_name, object_name,
                                              object_type);
        increment_metadata_synced_count();
      }
      break;
    }
    case object_detected_type::TABLE_OBJECT: {
      bool temp_error;
      if (metadata_sync.sync_table(thd, schema_name, object_name, temp_error)) {
        log_info("Table '%s.%s' successfully synchronized", schema_name.c_str(),
                 object_name.c_str());
        increment_metadata_synced_count();
      } else if (temp_error) {
        log_info("Failed to synchronize table '%s.%s' due to a temporary error",
                 schema_name.c_str(), object_name.c_str());
      } else {
        log_error("Failed to synchronize table '%s.%s'", schema_name.c_str(),
                  object_name.c_str());
        metadata_sync.add_object_to_blacklist(schema_name, object_name,
                                              object_type);
        increment_metadata_synced_count();
      }
      break;
    }
    default: {
      // Unexpected type, should never happen
      DBUG_ASSERT(false);
    }
  }
}
