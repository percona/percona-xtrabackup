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
#include <os0thread-create.h>  // os_thread_create
#include <univ.i>
#include "backup_copy.h"
#include "file_utils.h"
#include "sql_thd_internal_api.h"  // create_thd, destroy_thd
#include "xb0xb.h"                 // check_if_skip_table
#include "xtrabackup.h"            // datafiles_iter_t

void ddl_tracker_t::backup_file_op(uint32_t space_id, mlog_id_t type,
                                   const byte *buf, ulint len,
                                   lsn_t start_lsn) {
  byte *ptr = (byte *)buf;
  switch (type) {
    case MLOG_FILE_CREATE: {
      assert(len > 6);
      ptr += 4;  // flags
      ptr += 2;  // len
      const char *name = reinterpret_cast<char *>(ptr);
      add_create_table_from_redo(space_id, start_lsn, name);
    } break;
    case MLOG_FILE_RENAME: {
      ulint name_len = mach_read_from_2(ptr);
      ptr += 2;  // from len
      const char *old_name = reinterpret_cast<char *>(ptr);
      ptr += name_len;  // from name
      ptr += 2;         // to len
      const char *new_name = reinterpret_cast<char *>(ptr);
      add_rename_table_from_redo(space_id, start_lsn, old_name, new_name);
    } break;
    case MLOG_FILE_DELETE: {
      ptr += 2;  // len
      const char *name = reinterpret_cast<char *>(ptr);
      add_drop_table_from_redo(space_id, start_lsn, name);
    } break;
    case MLOG_INDEX_LOAD:
      add_to_recopy_tables(space_id, start_lsn, "add index");
      break;
    case MLOG_WRITE_STRING:
      add_to_recopy_tables(space_id, start_lsn, "encryption");
      break;
    default:
#ifdef UNIV_DEBUG
      assert(0);
#endif  // UNIV_DEBUG
      break;
  }
}

void ddl_tracker_t::add_table_from_ibd_scan(const space_id_t space_id,
                                            std::string name) {
  Fil_path::normalize(name);
  if (Fil_path::has_prefix(name, Fil_path::DOT_SLASH)) {
    name.erase(0, 2);
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  tables_in_backup[space_id] = name;
}

void ddl_tracker_t::add_corrupted_tablespace(const space_id_t space_id,
                                             const std::string &path) {
  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);

  corrupted_tablespaces[space_id] = path;
}

void ddl_tracker_t::add_to_recopy_tables(space_id_t space_id, lsn_t start_lsn,
                                         const std::string operation) {
  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  recopy_tables.insert(space_id);
  xb::info() << "DDL tracking : LSN: " << start_lsn << " " << operation
             << " on space ID: " << space_id;
}

void ddl_tracker_t::add_missing_table(std::string path) {
  Fil_path::normalize(path);
  if (Fil_path::has_prefix(path, Fil_path::DOT_SLASH)) {
    path.erase(0, 2);
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  missing_tables.insert(path);
}
void ddl_tracker_t::add_create_table_from_redo(const space_id_t space_id,
                                               lsn_t start_lsn,
                                               const char *name) {
  std::string new_space_name = name;
  Fil_path::normalize(new_space_name);
  if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
    new_space_name.erase(0, 2);
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  new_tables[space_id] = new_space_name;
  xb::info() << "DDL tracking : LSN: " << start_lsn
             << " create space ID: " << space_id << " Name: " << new_space_name;
}

void ddl_tracker_t::add_rename_table_from_redo(const space_id_t space_id,
                                               lsn_t start_lsn,
                                               const char *old_name,
                                               const char *new_name) {
  std::string old_space_name{old_name};
  std::string new_space_name{new_name};

  Fil_path::normalize(old_space_name);
  Fil_path::normalize(new_space_name);
  if (Fil_path::has_prefix(old_space_name, Fil_path::DOT_SLASH)) {
    old_space_name.erase(0, 2);
  }
  if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
    new_space_name.erase(0, 2);
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  if (renames.find(space_id) != renames.end()) {
    renames[space_id].second = new_space_name;
  } else {
    renames[space_id] = std::make_pair(old_space_name, new_space_name);
  }
  xb::info() << "DDL tracking : LSN: " << start_lsn
             << " rename space ID: " << space_id << " From: " << old_space_name
             << " To: " << new_space_name;
}

void ddl_tracker_t::add_drop_table_from_redo(const space_id_t space_id,
                                             lsn_t start_lsn,
                                             const char *name) {
  std::string new_space_name{name};
  Fil_path::normalize(new_space_name);
  if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
    new_space_name.erase(0, 2);
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  drops[space_id] = new_space_name;

  xb::info() << "DDL tracking : LSN: " << start_lsn
             << " delete space ID: " << space_id << " Name: " << new_space_name;
}

bool ddl_tracker_t::is_missing_table(const std::string &name) {
  if (missing_tables.count(name)) {
    return true;
  }
  return false;
}

void ddl_tracker_t::add_renamed_table(const space_id_t &space_id,
                                      std::string new_name) {
  Fil_path::normalize(new_name);
  if (Fil_path::has_prefix(new_name, Fil_path::DOT_SLASH)) {
    new_name.erase(0, 2);
  }
  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  renamed_during_scan[space_id] = new_name;
}

/* ======== Data copying thread context ======== */

typedef struct {
  datafiles_iter_t *it;
  uint num;
  uint *count;
  ib_mutex_t *count_mutex;
  bool *error;
  std::thread::id id;
  space_id_to_name_t *new_tables;
} copy_thread_ctxt_t;

static void data_copy_thread_func(copy_thread_ctxt_t *ctxt) {
  uint num = ctxt->num;
  fil_node_t *node;

  /*
    Initialize mysys thread-specific memory so we can
    use mysys functions in this thread.
  */
  my_thread_init();

  /* create THD to get thread number in the error log */
  THD *thd = create_thd(false, false, true, 0, 0);
  debug_sync_point("data_copy_thread_func");

  while ((node = datafiles_iter_next(ctxt->it)) != NULL && !*(ctxt->error)) {
    if (ctxt->new_tables->find(node->space->id) == ctxt->new_tables->end()) {
      continue;
    }
    std::string dest_name = node->name;
    dest_name.append(".new");
    if (xtrabackup_copy_datafile(node, num, dest_name.c_str())) {
      xb::error() << "failed to copy datafile " << node->name;
      *(ctxt->error) = true;
    }
  }

  mutex_enter(ctxt->count_mutex);
  (*ctxt->count)--;
  mutex_exit(ctxt->count_mutex);

  destroy_thd(thd);
  my_thread_end();
}

/* returns .del or .ren file name starting with space_id
  like schema/spaceid.ibd.del
*/
std::string ddl_tracker_t::convert_file_name(space_id_t space_id,
                                             std::string file_name,
                                             std::string ext) {
  auto sep_pos = file_name.find_last_of(Fil_path::SEPARATOR);
  return file_name.substr(0, sep_pos + 1) + std::to_string(space_id) + ext;
}

void ddl_tracker_t::handle_ddl_operations() {
  xb::info() << "DDL tracking : handling DDL operations";

  if (new_tables.empty() && renames.empty() && drops.empty() &&
      recopy_tables.empty() && corrupted_tablespaces.empty()) {
    xb::info()
        << "DDL tracking : Finished handling DDL operations - No changes";
    return;
  }
  dberr_t err;

  for (auto &tablespace : corrupted_tablespaces) {
    /* Create .corrupt file extension with the filename. Prepare should delete
    the corresponding .ibd, before doing *.ibd scan */
    std::string &path = tablespace.second.append(".corrupt");
    backup_file_printf(path.c_str(), "%s", "");
  }

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
        backup_file_printf(
            convert_file_name(table, renames[table].first, ".ibd.del").c_str(),
            "%s", "");
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

    /* Table not in the backup, nothing to drop, skip drop*/
    if (tables_in_backup.find(table.first) == tables_in_backup.end()) {
      continue;
    }

    backup_file_printf(
        convert_file_name(table.first, table.second, ".ibd.del").c_str(), "%s",
        "");
  }

  for (auto &table : renames) {
    if (check_if_skip_table(table.second.second.c_str())) {
      continue;
    }
    if (check_if_skip_table(table.second.first.c_str())) {
      continue;
    }
    /* renamed new table. update new table entry to renamed table name
      or if table is missing and renamed, add the renamed table to the new_table
      list. for example: 1. t1.ibd is discovered
                   2. t1.ibd renamed to t2.ibd
                   3. t2.ibd is opened and loaded to cache to copy
                   4. t1.ibd is missing now
      so we should add t2.ibd to new_tables and skip .ren file so that we don't
      try to rename t1.ibd to t2.idb where t1.ibd is missing   */
    if (new_tables.find(table.first) != new_tables.end() ||
        is_missing_table(table.second.first)) {
      new_tables[table.first] = table.second.second;
      continue;
    }

    /* Table not in the backup, nothing to rename, skip rename*/
    if (tables_in_backup.find(table.first) == tables_in_backup.end()) {
      continue;
    }

    backup_file_printf(
        convert_file_name(table.first, table.second.first, ".ibd.ren").c_str(),
        "%s", table.second.second.c_str());
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

  if (new_tables.empty()) return;

  /* Create data copying threads */
  copy_thread_ctxt_t *data_threads = (copy_thread_ctxt_t *)ut::malloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY,
      sizeof(copy_thread_ctxt_t) * xtrabackup_parallel);
  uint count = xtrabackup_parallel;
  ib_mutex_t count_mutex;
  mutex_create(LATCH_ID_XTRA_COUNT_MUTEX, &count_mutex);
  bool data_copying_error = false;
  datafiles_iter_t *it = datafiles_iter_new(nullptr);
  for (uint i = 0; i < (uint)xtrabackup_parallel; i++) {
    data_threads[i].it = it;
    data_threads[i].num = i + 1;
    data_threads[i].count = &count;
    data_threads[i].count_mutex = &count_mutex;
    data_threads[i].error = &data_copying_error;
    data_threads[i].new_tables = &new_tables;
    os_thread_create(PFS_NOT_INSTRUMENTED, i, data_copy_thread_func,
                     data_threads + i)
        .start();
  }

  /* Wait for threads to exit */
  while (1) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    mutex_enter(&count_mutex);
    if (count == 0) {
      mutex_exit(&count_mutex);
      break;
    }
    mutex_exit(&count_mutex);
  }

  if (data_copying_error) {
    exit(EXIT_FAILURE);
  }

  mutex_free(&count_mutex);
  ut::free(data_threads);
  datafiles_iter_free(it);
  xb::info() << "DDL tracking :  Finished handling DDL operations";
}

/** This map is required during incremental backup prepare becuase the
tablename can change between full and incremental backups. For reduced
lock, during prepare, we process the .del files based on space_id. So we
delete the right tablespace but the delta files are not. For example, consider
this scenario. backup has t1.ibd with space_id 10 <rename t1 to t2>
incremental backup will have t2.ibd.delta and t2.ibd.meta
and if there is drop table t2, with reduced lock we will have 10.ibd.del

Later when processing incremental backup, we process the 10.ibd.del. this
gives us the tablespace t1.ibd. We delete it. Additionally we look for
t1.ibd.delta and t1.ibd.meta and delete them. But in the incremental backup,
we have t2.ibd.delta and t2.ibd.meta.

This map helps us to get the right meta and delta files for a given space id */
meta_map_t meta_map;

void insert_into_meta_map(space_id_t space_id, const std::string &meta_path) {
  meta_map.insert({space_id, meta_path});
}

std::tuple<bool, std::string> is_in_meta_map(space_id_t space_id) {
  auto it = meta_map.find(space_id);
  bool exists = it != meta_map.end();
  return {exists, exists ? it->second : ""};
}
