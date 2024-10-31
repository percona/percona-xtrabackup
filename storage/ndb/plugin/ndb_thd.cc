/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_thd.h"

#include "my_dbug.h"
#include "mysql/thread_type.h"
#include "sql/handler.h"
#include "sql/sql_class.h"
#include "storage/ndb/plugin/ndb_log.h"  // ndb_log_*
#include "storage/ndb/plugin/ndb_thd_ndb.h"

/*
  Make sure THD has a Thd_ndb struct allocated and associated

  - validate_ndb, check if the Ndb object need to be recycled
*/

Ndb *check_ndb_in_thd(THD *thd, bool validate_ndb) {
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (!thd_ndb) {
    if (!(thd_ndb = Thd_ndb::seize(thd))) return nullptr;
    thd_set_thd_ndb(thd, thd_ndb);
  }

  else if (validate_ndb && !thd_ndb->valid_ndb()) {
    if (!thd_ndb->recycle_ndb()) return nullptr;
  }

  return thd_ndb->ndb;
}

bool applying_binlog(const THD *thd) {
  if (thd->slave_thread) {
    DBUG_PRINT("info", ("THD is slave thread"));
    return true;
  }

  if (thd->rli_fake) {
    /*
      Thread is in "pseudo_replica_mode" which is entered implicitly when the
      first BINLOG statement is executed (see 'mysql_client_binlog_statement')
      and explicitly ended when SET @pseudo_replica_mode=0 is finally executed.
    */
    DBUG_PRINT("info", ("THD is in pseduo slave mode"));
    return true;
  }

  return false;
}

extern ulong opt_server_id_mask;

uint32 thd_unmasked_server_id(const THD *thd) {
  const uint32 unmasked_server_id = thd->unmasked_server_id;
  assert(thd->server_id == (thd->unmasked_server_id & opt_server_id_mask));
  return unmasked_server_id;
}

const char *ndb_thd_query(const THD *thd) { return thd->query().str; }

size_t ndb_thd_query_length(const THD *thd) { return thd->query().length; }

ulonglong ndb_thd_get_pfs_thread_id() {
#ifdef HAVE_PSI_THREAD_INTERFACE
  return PSI_THREAD_CALL(get_current_thread_internal_id)();
#else
  return 0;
#endif
}

bool ndb_thd_is_binlog_thread(const THD *thd) {
  return thd->system_thread == SYSTEM_THREAD_NDBCLUSTER_BINLOG;
}

bool ndb_thd_is_replica_thread(const THD *thd) {
  return thd->system_thread == SYSTEM_THREAD_SLAVE_SQL ||
         thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER;
}

bool ndb_thd_is_background_thread(const THD *thd) {
  return thd->system_thread == SYSTEM_THREAD_BACKGROUND;
}

void ndb_thd_register_trans(THD *thd, bool register_trans) {
  // Always register for the statement
  trans_register_ha(thd, false, ndbcluster_hton, nullptr);

  // Register for the transaction if requested.
  if (register_trans &&
      thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
    trans_register_ha(thd, true, ndbcluster_hton, nullptr);
  }
}

void clear_thd_conditions(THD *thd) {
  // Remove the THD conditions
  thd->get_stmt_da()->reset_diagnostics_area();
  thd->get_stmt_da()->reset_condition_info(thd);
}

void log_and_clear_thd_conditions(THD *thd,
                                  condition_logging_level logging_level) {
  // Print THD's list of conditions to error log
  Diagnostics_area::Sql_condition_iterator it(
      thd->get_stmt_da()->sql_conditions());
  const Sql_condition *err;
  while ((err = it++)) {
    switch (logging_level) {
      case condition_logging_level::INFO: {
        ndb_log_info("Got error '%u: %s'", err->mysql_errno(),
                     err->message_text());
      } break;
      case condition_logging_level::WARNING: {
        ndb_log_warning("Got error '%u: %s'", err->mysql_errno(),
                        err->message_text());
      } break;
      case condition_logging_level::ERROR: {
        ndb_log_error("Got error '%u: %s'", err->mysql_errno(),
                      err->message_text());
      } break;
    }
  }
  clear_thd_conditions(thd);
}

Ndb_thd_memory_guard::Ndb_thd_memory_guard(THD *thd [[maybe_unused]])
#ifndef NDEBUG
    : m_thd(thd),
      m_thd_mem_root_size_before(thd->mem_root->allocated_size())
#endif
{
}

Ndb_thd_memory_guard::~Ndb_thd_memory_guard() {
  assert(m_thd->mem_root->allocated_size() <= m_thd_mem_root_size_before);
}

Ndb_thd_guard::Ndb_thd_guard() : m_thd(new THD()) {
  m_thd->system_thread = SYSTEM_THREAD_BACKGROUND;
  m_thd->thread_stack = reinterpret_cast<const char *>(&m_thd);
  m_thd->set_new_thread_id();
  m_thd->store_globals();
  m_thd->set_command(COM_DAEMON);
}

Ndb_thd_guard::~Ndb_thd_guard() {
  if (m_thd) {
    m_thd->release_resources();
    delete m_thd;
  }
}
