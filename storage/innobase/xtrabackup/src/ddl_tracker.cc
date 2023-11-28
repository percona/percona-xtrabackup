/******************************************************
Copyright (c) 2023 Percona LLC and/or its affiliates.

DDL Tracker for XtraBackup.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/
#include "ddl_tracker.h"
#include <fil0fil.h>
#include <univ.i>
#include "backup_copy.h"
#include "file_utils.h"
#include "xb0xb.h"       // check_if_skip_table
#include "xtrabackup.h"  // datafiles_iter_t

void ddl_tracker_t::backup_file_op(uint32_t space_id, mlog_id_t type,
                                   const byte *buf, ulint len,
                                   lsn_t start_lsn) {
  // TODO: Do we need a mutex here?
  // this will be populated by redo log parsing thread, which is single threaded
  // this will be read by main thread once we are under LTFB/LIFB
  // so no new entry will be added
  byte *ptr = (byte *)buf;
  ulint name_len;
  char *name;
  std::string old_space_name, new_space_name;
  switch (type) {
    case MLOG_FILE_CREATE:
      assert(len > 6);
      ptr += 4;  // flags
      ptr += 2;  // len
      name = reinterpret_cast<char *>(ptr);
      new_space_name = name;
      Fil_path::normalize(new_space_name);
      if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
        new_space_name.erase(0, 2);
      }
      new_tables[space_id] = new_space_name;
      xb::info() << "DDL tracking : LSN: " << start_lsn
                 << " create table ID: " << space_id
                 << " Name: " << new_space_name;
      break;
    case MLOG_FILE_RENAME:
      name_len = mach_read_from_2(ptr);
      ptr += 2;  // from len
      name = reinterpret_cast<char *>(ptr);
      old_space_name = name;
      ptr += name_len;  // from name
      ptr += 2;         // to len
      name = reinterpret_cast<char *>(ptr);
      new_space_name = name;
      Fil_path::normalize(old_space_name);
      Fil_path::normalize(new_space_name);
      if (Fil_path::has_prefix(old_space_name, Fil_path::DOT_SLASH)) {
        old_space_name.erase(0, 2);
      }
      if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
        new_space_name.erase(0, 2);
      }

      if (renames.find(space_id) != renames.end()) {
        renames[space_id].second = new_space_name;
      } else {
        renames[space_id] = std::make_pair(old_space_name, new_space_name);
      }
      xb::info() << "DDL tracking : LSN: " << start_lsn
                 << " rename table ID: " << space_id
                 << " From: " << old_space_name << " To: " << new_space_name;
      break;
    case MLOG_FILE_DELETE:
      ptr += 2;  // len
      name = reinterpret_cast<char *>(ptr);
      new_space_name = name;
      Fil_path::normalize(new_space_name);
      if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
        new_space_name.erase(0, 2);
      }
      drops[space_id] = new_space_name;

      xb::info() << "DDL tracking : LSN: " << start_lsn
                 << " delete table ID: " << space_id
                 << " Name: " << new_space_name;
      break;
    case MLOG_INDEX_LOAD:
      recopy_tables.insert(space_id);
      xb::info() << "DDL tracking : LSN: " << start_lsn
                 << " direct write on table ID: " << space_id;
      break;
    case MLOG_WRITE_STRING:
      recopy_tables.insert(space_id);
      xb::info() << "DDL tracking :  LSN: " << start_lsn
                 << " encryption operation on table ID: " << space_id;
      break;
    default:
#ifdef UNIV_DEBUG
      assert(0);
#endif  // UNIV_DEBUG
      break;
  }
}

void ddl_tracker_t::add_table(const space_id_t &space_id, std::string name) {
  Fil_path::normalize(name);
  if (Fil_path::has_prefix(name, Fil_path::DOT_SLASH)) {
    name.erase(0, 2);
  }
  tables_in_backup[space_id] = name;
}

void ddl_tracker_t::handle_ddl_operations() {
  // TODO: Make copy multi thread

  xb::info() << "DDL tracking :  handling DDL operations";

  if (new_tables.empty() && renames.empty() && drops.empty() &&
      recopy_tables.empty()) {
    xb::info()
        << "DDL tracking : Finished handling DDL operations - No changes";
    return;
  }
  dberr_t err;

  /* Some tables might get to the new list if the DDL happen in between
   * redo_mgr.start and xb_load_tablespaces. This causes we ending up with two
   * tablespaces with the same spaceID. Remove them from new tables */
  for (auto &table : tables_in_backup) {
    if (new_tables.find(table.first) != new_tables.end()) {
      new_tables.erase(table.first);
    }
  }

  /* recopy_tables will be handled as follow:
    * not in the backup - nothign to do. This is a new table that was created
     during the backup. It will be re-copied anyway as .new in the backup.
    * in the backup - we add it to be recopied if renamed - we delete the old
    file during prepare rename logic will then instruct adjust the proper file
    name to be copied
  */
  for (auto &table : recopy_tables) {
    if (tables_in_backup.find(table) != tables_in_backup.end()) {
      if (renames.find(table) != renames.end()) {
        backup_file_printf((renames[table].first + ".del").c_str(), "%s", "");
      }
      string name = tables_in_backup[table];
      new_tables[table] = name;
    }
  }

  for (auto &table : drops) {
    if (check_if_skip_table(table.second.c_str())) {
      continue;
    }
    /* Remove from rename */
    renames.erase(table.first);

    /* Remove from new tables and skip drop*/
    if (new_tables.find(table.first) != new_tables.end()) {
      new_tables.erase(table.first);
      continue;
    }
    backup_file_printf((table.second + ".del").c_str(), "%s", "");
  }

  for (auto &table : renames) {
    if (check_if_skip_table(table.second.second.c_str())) {
      continue;
    }
    if (check_if_skip_table(table.second.first.c_str())) {
      continue;
    }
    /* renamed new table. update new table entry to renamed table name */
    if (new_tables.find(table.first) != new_tables.end()) {
      new_tables[table.first] = table.second.second;
      continue;
    }
    backup_file_printf((table.second.first + ".ren").c_str(), "%s",
                       table.second.second.c_str());
  }

  fil_close_all_files();
  for (auto table = new_tables.begin(); table != new_tables.end();) {
    if (check_if_skip_table(table->second.c_str())) {
      table = new_tables.erase(table);
      continue;
    }
    std::tie(err, std::ignore) = fil_open_for_xtrabackup(
        table->second, table->second.substr(0, table->second.length() - 4));
    table++;
  }

  datafiles_iter_t *it = datafiles_iter_new(nullptr);
  while (fil_node_t *node = datafiles_iter_next(it)) {
    if (new_tables.find(node->space->id) == new_tables.end()) {
      continue;
    }
    if (check_if_skip_table(node->name)) {
      continue;
    }
    std::string dest_name = node->name;
    dest_name.append(".new");
    xtrabackup_copy_datafile(node, 0, dest_name.c_str());
  }
  datafiles_iter_free(it);
  xb::info() << "DDL tracking :  Finished handling DDL operations";
}
