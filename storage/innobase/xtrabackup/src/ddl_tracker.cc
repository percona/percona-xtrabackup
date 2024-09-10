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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <univ.i>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backup_copy.h"
#include "file_utils.h"
#include "fsp0fsp.h"  // fsp_is_undo_tablespace
#include "scope_guard.h"
#include "sql_thd_internal_api.h"  // create_thd, destroy_thd
#include "srv0start.h"
#include "utils.h"
#include "xb0xb.h"       // check_if_skip_table
#include "xtrabackup.h"  // datafiles_iter_t

extern bool xb_read_delta_metadata(const char *filepath, xb_delta_info_t *info);

void ddl_tracker_t::backup_file_op(uint32_t space_id, mlog_id_t type,
                                   const byte *buf, ulint len,
                                   lsn_t record_lsn) {
  ut_ad(!handle_ddl_ops);

  byte *ptr = (byte *)buf;
  switch (type) {
    case MLOG_FILE_CREATE: {
      assert(len > 6);
      ptr += 4;  // flags
      ptr += 2;  // len
      const char *name = reinterpret_cast<char *>(ptr);
      add_create_table_from_redo(space_id, record_lsn, name);
    } break;
    case MLOG_FILE_RENAME: {
      ulint name_len = mach_read_from_2(ptr);
      ptr += 2;  // from len
      const char *old_name = reinterpret_cast<char *>(ptr);
      ptr += name_len;  // from name
      ptr += 2;         // to len
      const char *new_name = reinterpret_cast<char *>(ptr);
      add_rename_table_from_redo(space_id, record_lsn, old_name, new_name);
    } break;
    case MLOG_FILE_DELETE: {
      ptr += 2;  // len
      const char *name = reinterpret_cast<char *>(ptr);
      add_drop_table_from_redo(space_id, record_lsn, name);
    } break;
    case MLOG_INDEX_LOAD:
      add_to_recopy_tables(space_id, record_lsn, "add index");
      break;
    case MLOG_WRITE_STRING:
      add_to_recopy_tables(space_id, record_lsn, "encryption");
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
  // After lock is taken, the scans shouldn't modify the DDL tracker
  // by the time, we are at handle_ddl_operations, we are consolidating
  // and no more changes are allowed
  ut_ad(!handle_ddl_ops);

  Fil_path::normalize(name);
  if (Fil_path::has_prefix(name, Fil_path::DOT_SLASH)) {
    name.erase(0, strlen(Fil_path::DOT_SLASH));
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  tables_copied_no_lock[space_id] = name;
}

void ddl_tracker_t::add_undo_tablespace(const space_id_t space_id,
                                        std::string name) {
  ut_ad(fsp_is_undo_tablespace(space_id));

  Fil_path::normalize(name);
  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  if (is_server_locked()) {
    after_lock_undo[name] = space_id;
  } else {
    before_lock_undo[name] = space_id;
  }
}

void ddl_tracker_t::add_corrupted_tablespace(const space_id_t space_id,
                                             const std::string &path) {
  ut_ad(!handle_ddl_ops);

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);

  corrupted_tablespaces[space_id] = path;
}

void ddl_tracker_t::add_to_recopy_tables(space_id_t space_id, lsn_t record_lsn,
                                         const std::string operation) {
  // There should never be MLOG_INDEX_LOAD on undo tablespaces. However
  // MLOG_WRITE_STRING on new undo tabelsapce creation is possible. But we
  // ignore this as the UNDOs are tracked separately
  if (fsp_is_undo_tablespace(space_id)) {
    return;
  }

  ut_ad(!handle_ddl_ops);

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  recopy_tables.insert(space_id);
  xb::info() << "DDL tracking : LSN: " << record_lsn << " " << operation
             << " on space ID: " << space_id;
}

void ddl_tracker_t::add_missing_after_discovery(std::string path) {
  ut_ad(!handle_ddl_ops);

  Fil_path::normalize(path);
  if (Fil_path::has_prefix(path, Fil_path::DOT_SLASH)) {
    path.erase(0, strlen(Fil_path::DOT_SLASH));
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  missing_after_discovery.insert(path);
}

void ddl_tracker_t::add_create_table_from_redo(const space_id_t space_id,
                                               lsn_t record_lsn,
                                               const char *name) {
  // undo tablespaces are tracked separately.
  if (fsp_is_undo_tablespace(space_id)) {
    return;
  }

  // tables in system tablespace need not be tracked. They can be
  // created via redo log
  if (fsp_is_system_tablespace(space_id)) {
    return;
  }

  ut_ad(!handle_ddl_ops);

  std::string new_space_name = name;
  Fil_path::normalize(new_space_name);
  if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
    new_space_name.erase(0, strlen(Fil_path::DOT_SLASH));
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  new_tables[space_id] = new_space_name;
  xb::info() << "DDL tracking : LSN: " << record_lsn
             << " create space ID: " << space_id << " Name: " << new_space_name;
}

void ddl_tracker_t::add_rename_table_from_redo(const space_id_t space_id,
                                               lsn_t record_lsn,
                                               const char *old_name,
                                               const char *new_name) {
  // Rename of tables in system tablespace is only rename in DD. this is
  // tracked via redo log
  if (fsp_is_system_tablespace(space_id)) {
    return;
  }

  ut_ad(!handle_ddl_ops);

  std::string old_space_name{old_name};
  std::string new_space_name{new_name};

  Fil_path::normalize(old_space_name);
  Fil_path::normalize(new_space_name);
  if (Fil_path::has_prefix(old_space_name, Fil_path::DOT_SLASH)) {
    old_space_name.erase(0, 2);
  }
  if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
    new_space_name.erase(0, strlen(Fil_path::DOT_SLASH));
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  if (renames.find(space_id) != renames.end()) {
    renames[space_id].second = new_space_name;
  } else {
    renames[space_id] = std::make_pair(old_space_name, new_space_name);
  }
  xb::info() << "DDL tracking : LSN: " << record_lsn
             << " rename space ID: " << space_id << " From: " << old_space_name
             << " To: " << new_space_name;
}

void ddl_tracker_t::add_drop_table_from_redo(const space_id_t space_id,
                                             lsn_t record_lsn,
                                             const char *name) {
  // undo tablespaces are tracked separately.
  if (fsp_is_undo_tablespace(space_id)) {
    return;
  }

  // DROP table in system tablespace is drop indexes. There is no file removal
  // this is also tracked/redone from redo
  if (fsp_is_system_tablespace(space_id)) {
    return;
  }

  ut_ad(!handle_ddl_ops);

  std::string new_space_name{name};
  Fil_path::normalize(new_space_name);
  if (Fil_path::has_prefix(new_space_name, Fil_path::DOT_SLASH)) {
    new_space_name.erase(0, strlen(Fil_path::DOT_SLASH));
  }

  std::lock_guard<std::mutex> lock(m_ddl_tracker_mutex);
  drops[space_id] = new_space_name;

  xb::info() << "DDL tracking : LSN: " << record_lsn
             << " delete space ID: " << space_id << " Name: " << new_space_name;
}

bool ddl_tracker_t::is_missing_after_discovery(const std::string &name) {
  return missing_after_discovery.count(name) != 0;
}

void ddl_tracker_t::add_rename_ibd_scan(const space_id_t &space_id,
                                        std::string new_name) {
  // undo tablespaces are tracked separately.
  if (fsp_is_undo_tablespace(space_id)) {
    // MLOG_FILE_RENAME is never done for undo tablespace
    ut_ad(0);
    return;
  }

  // rename tables in system tablespace is only rename in DD
  // tracked via redo. Skip this
  if (fsp_is_system_tablespace(space_id)) {
    return;
  }

  ut_ad(!handle_ddl_ops);

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

static void copy_for_reduced(copy_thread_ctxt_t *ctxt) {
  uint num = ctxt->num;
  fil_node_t *node;

  /*
    Initialize mysys thread-specific memory so we can
    use mysys functions in this thread.
  */
  my_thread_init();

  /* create THD to get thread number in the error log */
  THD *thd = create_thd(false, false, true, 0, 0);
  debug_sync_point("copy_for_reduced");

  while ((node = datafiles_iter_next(ctxt->it)) != NULL && !*(ctxt->error)) {
    if (ctxt->new_tables->find(node->space->id) == ctxt->new_tables->end()) {
      continue;
    }
    std::string dest_name = node->name;
    dest_name.append(EXT_NEW);
    if (xtrabackup_copy_datafile_func(node, num, dest_name.c_str())) {
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

/**
Converts filename from `schema/filename.ibd` format to `schema/spaceid.ext`
format by replacing the `filename` to `spaceid` and appending the extension
@param[in] space_id  The space id of the current file
@param[in] file_name File name that we will modify
@param[in] ext       Extension that we will modify to
@returns .del or .ren file name starting with space_id e.g schema/spaceid.del
*/
static std::string convert_file_name(const space_id_t space_id,
                                     const std::string &file_name,
                                     const std::string &ext) {
  auto sep_pos = file_name.find_last_of(Fil_path::SEPARATOR);
  return file_name.substr(0, sep_pos + 1) + std::to_string(space_id) + ext;
}

// Helper function to print the contents of a vector of pairs
void printVector(const std::string &operation, const filevec &vec) {
  for (const auto &element : vec) {
    xb::info() << operation << element.first << " : " << element.second << " ";
  }
}

// Function to find new, deleted, and changed files between two unordered_maps
std::tuple<filevec, filevec> findChanges(const name_to_space_id_t &before,
                                         const name_to_space_id_t &after) {
  filevec newFiles;
  filevec deletedOrChangedFiles;

  // Iterate through the after map to find new, changed, and unchanged files
  for (const auto &pair : after) {
    auto itBefore = before.find(pair.first);
    if (itBefore == before.end()) {
      // Element is new
      newFiles.emplace_back(pair.first, pair.second);
    } else {
      // Element exists in before map
      if (itBefore->second != pair.second) {
        // Element value has changed
        deletedOrChangedFiles.emplace_back(
            pair.first, itBefore->second);  // Use old value from before map
        newFiles.emplace_back(pair.first, pair.second);
      }
    }
  }

  // Look for deleted
  for (const auto &pair : before) {
    auto itAfter = after.find(pair.first);

    if (itAfter == after.end()) {
      // Element is not present in the after map. This is deleted
      deletedOrChangedFiles.emplace_back(pair.first, pair.second);
    }
  }

  return {newFiles, deletedOrChangedFiles};
}

std::tuple<filevec, filevec> ddl_tracker_t::handle_undo_ddls() {
  xb::info() << "DDL tracking: handling undo DDLs";

  undo_spaces_deinit();
  undo_spaces_init();
  xb_scan_for_tablespaces(true);

  // Generates the after_lock_undo list
  srv_undo_tablespaces_init(false, true);

  std::ostringstream str;
  // xb::info() is not used intentionally, as there is a loss
  // of newlines characters from the stream/string
  str << "Before UNDO files" << xtrabackup::utils::to_string(before_lock_undo)
      << "\n"
      << "After UNDO files" << xtrabackup::utils::to_string(after_lock_undo)
      << "\n";
  fprintf(stderr, "%s\n", str.str().c_str());

  auto [newfiles, deletedOrChangedFiles] =
      findChanges(before_lock_undo, after_lock_undo);

  // Print the results
  xb::info() << "New UNDO files: ";
  printVector("New undo file: ", newfiles);

  xb::info() << "Deleted or truncated UNDO files: ";
  printVector("Deleted undo file: ", deletedOrChangedFiles);

  return {newfiles, deletedOrChangedFiles};
}

dberr_t ddl_tracker_t::handle_ddl_operations() {
  fil_close_all_files();
  fil_close();
  fil_init(LONG_MAX);

  auto [new_undo_files, deleted_undo_files] = handle_undo_ddls();

  xb::info() << "DDL tracking : handling DDL operations";

  if (new_tables.empty() && renames.empty() && drops.empty() &&
      recopy_tables.empty() && corrupted_tablespaces.empty() &&
      new_undo_files.empty() && deleted_undo_files.empty()) {
    xb::info()
        << "DDL tracking : Finished handling DDL operations - No changes";
    return DB_SUCCESS;
  }

  ut_d(handle_ddl_ops = true;);

  std::ostringstream str;
  // xb::info() is not used intentionally, as there is a loss
  // of newlines characters from the stream/string
  str << "Tables_copied: "
      << xtrabackup::utils::to_string(tables_copied_no_lock) << "\n"
      << "New_tables: " << xtrabackup::utils::to_string(new_tables) << "\n"
      << "Renames: " << xtrabackup::utils::to_string(renames) << "\n"
      << "Drops: " << xtrabackup::utils::to_string(drops) << "\n"
      << "Recopy_tables: " << xtrabackup::utils::to_string(recopy_tables)
      << "\n"
      << "Corrupted_tablespaces: "
      << xtrabackup::utils::to_string(corrupted_tablespaces) << "\n"
      << "Missing tables: "
      << xtrabackup::utils::to_string(missing_after_discovery) << "\n"
      << "Renamed_during_scan: "
      << xtrabackup::utils::to_string(renamed_during_scan) << "\n";
  fprintf(stderr, "%s\n", str.str().c_str());

  dberr_t err;

  for (auto &tablespace : corrupted_tablespaces) {
    /* Create .crpt file extension with the filename. Prepare should delete
    the corresponding .ibd, before doing *.ibd scan */
    std::string &path = tablespace.second.append(EXT_CRPT);
    backup_file_printf(path.c_str(), "%s", "");
  }

  /* Some tables might get to the new list if the DDL happen in between
   * redo_mgr.start and xb_load_tablespaces. This causes we ending up with two
   * tablespaces with the same spaceID. Remove them from new tables */
  for (auto &table : tables_copied_no_lock) {
    space_id_t space_id = table.first;
    if (new_tables.find(space_id) != new_tables.end()) {
      new_tables.erase(space_id);
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
    if (tables_copied_no_lock.find(table) != tables_copied_no_lock.end()) {
      if (renames.find(table) != renames.end()) {
        // We never create .del for ibdata*
        ut_ad(!fsp_is_system_tablespace(table));
        backup_file_printf(
            convert_file_name(table, renames[table].first, EXT_DEL).c_str(),
            "%s", "");
      }
      string name = tables_copied_no_lock[table];
      new_tables[table] = name;
    }
  }

  for (auto &table : drops) {
    space_id_t space_id = table.first;
    std::string table_name = table.second;

    if (check_if_skip_table(table_name.c_str())) {
      continue;
    }
    /* Remove from rename */
    renames.erase(space_id);

    /* Remove from new tables and skip drop*/
    if (new_tables.find(space_id) != new_tables.end()) {
      new_tables.erase(space_id);
      continue;
    }

    /* Table not in the backup, nothing to drop, skip drop*/
    if (tables_copied_no_lock.find(space_id) == tables_copied_no_lock.end()) {
      continue;
    }

    // We never create .del for ibdata*
    ut_ad(!fsp_is_system_tablespace(space_id));
    backup_file_printf(convert_file_name(space_id, table_name, EXT_DEL).c_str(),
                       "%s", "");
  }

  for (auto &table : renames) {
    space_id_t space_id = table.first;
    std::string old_table_name = table.second.first;
    std::string new_table_name = table.second.second;

    if (check_if_skip_table(new_table_name.c_str())) {
      continue;
    }
    if (check_if_skip_table(old_table_name.c_str())) {
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
    if (new_tables.find(space_id) != new_tables.end() ||
        is_missing_after_discovery(old_table_name)) {
      new_tables[space_id] = new_table_name;
      continue;
    }

    /* Table not in the backup, nothing to rename, skip rename*/
    if (tables_copied_no_lock.find(space_id) == tables_copied_no_lock.end()) {
      continue;
    }

    backup_file_printf(
        convert_file_name(space_id, old_table_name, EXT_REN).c_str(), "%s",
        new_table_name.c_str());
  }

  for (auto table = new_tables.begin(); table != new_tables.end();) {
    if (check_if_skip_table(table->second.c_str())) {
      table = new_tables.erase(table);
      continue;
    }

    if (fsp_is_system_tablespace(table->first)) {
      // open system tablespace for recopying
      srv_sys_space.shutdown();

      srv_sys_space.set_space_id(TRX_SYS_SPACE);

      /* Create the filespace flags. */
      ulint fsp_flags =
          fsp_flags_init(univ_page_size, false, false, false, false);
      srv_sys_space.set_flags(fsp_flags);

      srv_sys_space.set_name("innodb_system");
      srv_sys_space.set_path(srv_data_home);

      /* Supports raw devices */
      if (!srv_sys_space.parse_params(innobase_data_file_path, true,
                                      xtrabackup_prepare)) {
        xb::error() << "Cannot parse system tablespace params";
        ut_ad(0);
        return DB_ERROR;
      }
      dberr_t err;
      page_no_t sum_of_new_sizes;
      lsn_t flush_lsn;

      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      err = srv_sys_space.check_file_spec(false, 0);

      if (err != DB_SUCCESS) {
        xb::error() << "In reduced mode, recopy could not find system "
                       "tablespace files at the specified datadir";
        ut_ad(0);
        return DB_ERROR;
      }

      err = srv_sys_space.open_or_create(false, false, &sum_of_new_sizes,
                                         &flush_lsn);

      if (err != DB_SUCCESS) {
        xb::error() << "could not reopen system tablespace";
        ut_ad(0);
        return DB_ERROR;
      }

    } else {
      std::tie(err, std::ignore) = fil_open_for_xtrabackup(
          table->second,
          table->second.substr(0, table->second.length() - EXT_REN.length()));

      // Under lock, we should be able to open tablespace
      if (err != DB_SUCCESS) {
        xb::error() << "Tablespace opening under reduced lock reopen failed: "
                    << table->second << " err: " << err;

        // Currently we are aware of ALTER INSTANCE ROTATE MASTER KEY can cause
        // the failure.
        ut_ad(err == DB_SUCCESS || err == DB_INVALID_ENCRYPTION_META);
        return err;
      }
    }
    table++;
  }

  // Create .del files for deleted undo tablespaces
  for (auto &elem : deleted_undo_files) {
    backup_file_printf(
        convert_file_name(elem.second, elem.first, EXT_DEL).c_str(), "%s", "");
  }

  // Add new undo files to be recopied
  for (auto &elem : new_undo_files) {
    new_tables[elem.second] = elem.first;
  }

  if (new_tables.empty()) {
    xb::info() << "DDL tracking : no new files are being copied.";
    return DB_SUCCESS;
  }

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
    os_thread_create(PFS_NOT_INSTRUMENTED, i, copy_for_reduced,
                     data_threads + i)
        .start();
  }

  /* Wait for threads to exit */
  while (1) {
    xb::info() << "Sleeping for 1 second, waiting for the recopy of tables to"
                  " be finished. Expecting count be 0, current count of"
                  " tablespaces to be copied is "
               << count;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    mutex_enter(&count_mutex);
    if (count == 0) {
      mutex_exit(&count_mutex);
      break;
    }
    mutex_exit(&count_mutex);
  }

  if (data_copying_error) {
    xb::error() << "DDL tracking : could not recopy tablespaces.";
    return DB_ERROR;
  }

  mutex_free(&count_mutex);
  ut::free(data_threads);
  datafiles_iter_free(it);
  xb::info() << "DDL tracking :  Finished handling DDL operations";
  return DB_SUCCESS;
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

/** Read file content into STL string */
static std::string read_file_as_string(const std::string &file) {
  try {
    // Check if the file exists using std::filesystem
    if (!std::filesystem::exists(file)) {
      return "";
    }

    std::ifstream f(file, std::ios::in | std::ios::binary);
    if (!f) {
      return "";
    }

    // Read entire file into a string
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    return content;
  } catch (const std::filesystem::filesystem_error &e) {
    // Handle filesystem-related exceptions
    // (e.g., file doesn't exist, permissions issues)
    xb::error() << "Filesystem error: " << e.what() << '\n';
    return "";
  } catch (const std::ifstream::failure &e) {
    // Handle ifstream-related exceptions
    // (e.g., file couldn't be opened, read errors)
    xb::error() << "File read error: " << e.what() << '\n';
    return "";
  } catch (const std::exception &e) {
    // Catch all other standard exceptions
    xb::error() << "Error: " << e.what() << '\n';
    return "";
  }
}

static void delete_force(const std::string &path) {
  if (access(path.c_str(), R_OK) == 0) {
    if (my_delete(path.c_str(), MYF(MY_WME))) {
      xb::error() << "Can not delete file " << path << "; errno " << errno;
      exit(EXIT_FAILURE);
    }
  }
}

static void rename_file(const std::string &from, const std::string &to) {
  if (my_rename(from.c_str(), to.c_str(), MY_WME)) {
    xb::error() << "Can't rename " << from << " to " << to << " errno "
                << errno;
    exit(EXIT_FAILURE);
  }
}

static void rename_force(const std::string &from, const std::string &to) {
  MY_STAT stat_info;
  /* check if dest folder exists */
  std::string dest_dir = to.substr(0, to.find_last_of("/"));
  /* create target dir if not exist */
  if (!my_stat(dest_dir.c_str(), &stat_info, MYF(0)) &&
      (my_mkdir(dest_dir.c_str(), 0750, MYF(0)) < 0)) {
    xb::error() << "cannot mkdir: " << my_errno() << " " << dest_dir;
    exit(EXIT_FAILURE);
  }
  rename_file(from, to);
}

/** Check if the file has the the specified suffix and truncate
  @param[in]            suffix     suffix to look for
  @param[in,out]        path    Filename to check
  @return true if the suffix is found and truncated. */
static bool truncate_suffix(std::string suffix, std::string &path) {
  size_t len = suffix.length();

  if (path.size() < len || path.compare(path.size() - len, len, suffix) != 0) {
    return (false);
  }

  path.resize(path.size() - len);
  return (true);
}

/**
 * Handles RENAME DDL that happened during the backup taken in reduced mode
 * by processing the file with `.ren` extension.
 * example input: `schema/10.ren` file with file content = `schema/new_name.ibd`
 *        result: tablespace with space_id=10 will be renamed to
 * `schema/new_name.ibd`.
 * @param[in] entry		datadir entry
 * @return true on success
 */
bool prepare_handle_ren_files(const datadir_entry_t &entry, void *) {
  if (entry.is_empty_dir) return true;

  std::string ren_file_name = entry.file_name;
  std::string ren_path = entry.path;

  Fil_path::normalize(ren_path);
  // trim .ren
  truncate_suffix(EXT_REN, ren_file_name);
  space_id_t source_space_id = std::stoi(ren_file_name);

  std::string ren_file_content = read_file_as_string(ren_path);

  if (ren_file_content.empty()) {
    xb::error() << "prepare_handle_ren_files: " << ren_path << " is empty.";
    return false;
  }
  // trim .ibd
  std::string dest_space_name = ren_file_content;
  truncate_suffix(EXT_IBD, dest_space_name);

  char *dest_path = Fil_path::make_ibd_from_table_name(dest_space_name);
  auto dest_path_free_guard = create_scope_guard([&]() {
    if (dest_path != nullptr) {
      ut::free(dest_path);
    }
  });

  fil_space_t *fil_space = fil_space_get(source_space_id);

  if (fil_space != nullptr) {
    char *source_path = nullptr, *source_space_name = nullptr;
    bool res = fil_space_read_name_and_filepath(
        fil_space->id, &source_space_name, &source_path);

    auto source_path_free_guard = create_scope_guard([&]() {
      if (source_space_name != nullptr) {
        ut::free(source_space_name);
      }
      if (source_path != nullptr) {
        ut::free(source_path);
      }
    });

    if (!res || !os_file_exists(source_path)) {
      xb::error() << "prepare_handle_ren_files: Tablespace " << fil_space->name
                  << " not found.";
      return false;
    }

    ut_ad(!os_file_exists(dest_path));

    xb::info() << "prepare_handle_ren_files: renaming " << fil_space->name
               << " to " << dest_space_name;

    if (!fil_rename_tablespace(fil_space->id, source_path,
                               dest_space_name.c_str(), NULL)) {
      xb::error() << "prepare_handle_ren_files: Cannot rename "
                  << fil_space->name << " to " << dest_space_name;
      return false;
    }
  } else {
    // In case source file doesn't exist we check if destination file is already
    // there and if destination file DO NOT exist with desired name we throw
    // error
    // in case of incrementals, the original file might be present as .delta
    // So ignore the debug check for incrementals.
    if (!os_file_exists(dest_path) && !xtrabackup_incremental) {
      xb::error() << "prepare_handle_ren_files: Tablespace with space_id "
                  << source_space_id << " is not found."
                  << " Destination path " << dest_path << " is also not found ";
      xb::error() << "The incremental meta map is ";
      fprintf(stderr, "%s", xtrabackup::utils::to_string(meta_map).c_str());
      ut_ad(0);
      return false;
    }
  }

  // rename .delta .meta files as well
  if (xtrabackup_incremental) {
    auto [exists, meta_file] = is_in_meta_map(source_space_id);
    if (exists) {
      std::string to_path = entry.datadir + ren_file_content;

      // create .delta path from .meta
      std::string delta_file = meta_file;
      truncate_suffix(EXT_META, delta_file);
      delta_file.append(EXT_DELTA);

      std::string to_delta(to_path + EXT_DELTA);
      xb::info() << "Renaming incremental delta file from: " << delta_file
                 << " to: " << to_delta;
      rename_force(delta_file, to_delta);

      std::string to_meta(to_path + EXT_META);
      xb::info() << "Renaming incremental meta file from: " << meta_file
                 << " to: " << to_meta;
      rename_force(meta_file, to_meta);
    } else if (fil_space == nullptr) {
      // This means the tablespace is neither found in the fullbackup dir
      // nor in the inc backup directory as .meta and .delta
      xb::error() << "prepare_handle_ren_files(): failed to handle " << ren_path
                  << " ren_file content: " << ren_file_content;
      xb::error() << "The incremental meta map is ";
      fprintf(stderr, "%s", xtrabackup::utils::to_string(meta_map).c_str());

      ut_ad(0);
      return false;
    }
  }

  // delete the .ren file, we don't need it anymore
  os_file_delete(0, ren_path.c_str());
  return true;
}

/**
 * Handles corrupted files during the backup taken in reduced mode
 * by processing the file with `.crpt` extension.
 * These files should be removed before we do *.ibd scan
 * @param[in] entry		datadir entry
 * @return true on success
 * */
bool prepare_handle_corrupt_files(const datadir_entry_t &entry, void *) {
  if (entry.is_empty_dir) return true;

  std::string corrupt_path = entry.path;
  Fil_path::normalize(corrupt_path);
  // trim .crpt
  std::string source_path = corrupt_path;
  truncate_suffix(EXT_CRPT, source_path);

  if (xtrabackup_incremental) {
    std::string delta_file = source_path + EXT_DELTA;
    xb::info() << "prepare_handle_corrupt_files: deleting  " << delta_file;
    if (!os_file_delete_if_exists_func(delta_file.c_str(), nullptr)) {
      xb::error() << "prepare_handle_corrupt_files: Can not delete "
                  << delta_file;
      return false;
    }

    std::string meta_file = source_path + EXT_META;
    xb::info() << "prepare_handle_corrupt_files: deleting  " << meta_file;
    if (!os_file_delete_if_exists_func(meta_file.c_str(), nullptr)) {
      xb::error() << "prepare_handle_corrupt_files: Can not delete "
                  << meta_file;
      return false;
    }
  } else {
    xb::info() << "prepare_handle_corrupt_files: deleting  " << source_path;
    if (!os_file_delete_if_exists_func(source_path.c_str(), nullptr)) {
      xb::error() << "prepare_handle_corrupt_files: Can not delete "
                  << source_path;
      return false;
    }
  }

  // delete the .crpt file, we don't need it anymore
  if (!os_file_delete_if_exists_func(corrupt_path.c_str(), nullptr)) {
    xb::error() << "prepare_handle_corrupt_files: Can not delete "
                << corrupt_path;
    return false;
  }

  return true;
}

/**
 * Handles DELETE DDL that happened during the backup taken in reduced mode
 * by processing the file with `.del` extension.
 * example input: `schema/10.del` file
 *         result: tablespace with space_id=10 will be deleted.
 * @param[in] entry		datadir entry
 * @param[in] arg		unused
 * @return true on success
 */
bool prepare_handle_del_files(const datadir_entry_t &entry,
                              void *arg __attribute__((unused))) {
  if (entry.is_empty_dir) return true;

  std::string del_file_name = entry.file_name;
  std::string del_file = entry.path;

  // trim .del
  truncate_suffix(EXT_DEL, del_file_name);
  space_id_t space_id = atoi(del_file_name.c_str());

  fil_space_t *fil_space = fil_space_get(space_id);
  if (fil_space != nullptr) {
    char *path = nullptr, *space_name = nullptr;
    bool res =
        fil_space_read_name_and_filepath(fil_space->id, &space_name, &path);

    auto guard = create_scope_guard([&]() {
      if (space_name != nullptr) {
        ut::free(space_name);
      }
      if (path != nullptr) {
        ut::free(path);
      }
    });

    if (!res || !os_file_exists(path)) {
      xb::error() << "prepare_handle_del_files: Tablespace " << fil_space->name
                  << " not found.";
      return false;
    }

    xb::info() << "prepare_handle_del_files: deleting " << fil_space->name;

    dberr_t err = fil_delete_tablespace(fil_space->id, BUF_REMOVE_NONE);
    if (err != DB_SUCCESS) {
      xb::error() << "prepare_handle_del_files: Cannot delete "
                  << fil_space->name;
      return false;
    }
  }

  if (xtrabackup_incremental) {
    auto [exists, meta_file] = is_in_meta_map(space_id);
    if (exists) {
      // create .delta path from .meta
      std::string delta_file = meta_file;
      truncate_suffix(EXT_META, delta_file);
      delta_file.append(EXT_DELTA);
      xb::info() << "Deleting incremental meta file: " << meta_file;
      delete_force(meta_file);
      xb::info() << "Deleting incremental delta file: " << delta_file;
      delete_force(delta_file);
    }
  }

  xb::info() << "prepare_handle_del_files: deleting " << del_file.c_str();
  os_file_delete(0, del_file.c_str());
  return true;
}

// Function to check if a string ends with another string
static bool ends_with(const string &full_string, const string &ext) {
  // Check if the ending string is longer than the full
  // string
  if (ext.size() > full_string.size()) return false;

  // Compare the ending of the full string with the target
  // ending
  return full_string.compare(full_string.size() - ext.size(), ext.size(),
                             ext) == 0;
}

/************************************************************************
 * Scan .meta files and build std::unordered_map<space_id, meta_path>.
 * This map is later used to delete the .delta and .meta file for a dropped
 * tablespace (ie. when processing the .del entries in reduced lock)
 * @param[in] entry		datadir entry
 * @return true on success
 */
bool xtrabackup_scan_meta(const datadir_entry_t &entry, void *) {
  const std::string &meta_path = entry.path;
  xb_delta_info_t info;

  if (entry.is_empty_dir) {
    return true;
  }

  IORequest read_request(IORequest::READ);
  IORequest write_request(IORequest::WRITE | IORequest::PUNCH_HOLE);

  ut_a(xtrabackup_incremental);

  // We shouldn't add .new.meta to meta_map, as they are meant to be replaced
  // with the old version of the file.
  if (ends_with(entry.file_name, EXT_NEW_META)) {
    return true;
  }

  if (!xb_read_delta_metadata(meta_path.c_str(), &info)) {
    xb::error() << "xtrabackup_scan_meta(): failed to read .meta file "
                << entry.path;
    return false;
  }

  insert_into_meta_map(info.space_id, meta_path);

  return true;
}

/** Handles CREATE DDL that happened during the backup taken in reduced mode
by processing the file with `.new` extension.
example input: `schema/filename.ibd.new` file
result: file `schema/filename.ibd.new` will be renamed to `schema/filename.ibd`
@param[in] entry  datadir entry
@param[in] arg    unused
@return true on success */

bool prepare_handle_new_files(const datadir_entry_t &entry,
                              void *arg __attribute__((unused))) {
  if (entry.is_empty_dir) return true;
  std::string src_path = entry.path;
  std::string dest_path = src_path;
  xb::info() << "prepare_handle_new_files: " << src_path;
  size_t index = dest_path.find(EXT_NEW);
#ifdef UNIV_DEBUG
  assert(index != std::string::npos);
#endif  // UNIV_DEBUG
  dest_path.erase(index, 4);
  rename_force(src_path, dest_path);

  return true;
}
