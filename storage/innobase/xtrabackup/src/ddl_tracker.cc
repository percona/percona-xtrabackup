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
