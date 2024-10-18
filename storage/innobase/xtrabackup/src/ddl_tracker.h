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
#ifndef DDL_TRACKER_H
#define DDL_TRACKER_H
#include <univ.i>
#include "log0types.h"
#include "mtr0types.h"
typedef std::unordered_map<space_id_t, std::string> space_id_to_name_t;
typedef std::unordered_map<std::string, space_id_t> name_to_space_id_t;
typedef std::unordered_map<space_id_t, std::string> meta_map_t;
using filevec = std::vector<std::pair<std::string, space_id_t>>;
struct datadir_entry_t;
extern meta_map_t meta_map;

inline const std::string EXT_DEL = ".del";
inline const std::string EXT_REN = ".ren";
inline const std::string EXT_NEW = ".new";
inline const std::string EXT_IBD = ".ibd";
inline const std::string EXT_IBU = ".ibu";
inline const std::string EXT_META = ".meta";
inline const std::string EXT_NEW_META = ".new.meta";
inline const std::string EXT_DELTA = ".delta";
inline const std::string EXT_NEW_DELTA = ".new.delta";
inline const std::string EXT_CRPT = ".crpt";

class ddl_tracker_t {
 private:
  /** List of all tables copied without lock */
  space_id_to_name_t tables_copied_no_lock;
  /** Tablspaces with their ID and name, as they were copied to backup.*/
  space_id_to_name_t new_tables;
  name_to_space_id_t before_lock_undo;
  name_to_space_id_t after_lock_undo;
  /** Tablespaces involved in encryption or bulk index load.*/
  std::unordered_set<space_id_t> recopy_tables;
  /** Drop operations found in redo log. */
  space_id_to_name_t drops;
  /* For DDL operation found in redo log,  */
  std::unordered_map<space_id_t, std::pair<std::string, std::string>> renames;
  /** Tables that have been deleted between discovery and file open */
  std::unordered_set<std::string> missing_after_discovery;
  /** Tables that have been renamed during scan. Tablespace_id and new table
   *  name */
  space_id_to_name_t renamed_during_scan;

  /** Tables that cannot be decrypted during backup because of encryption
  changes. Copy threads that cannot decrypt page, considers them as corrupted
  page. Can happen only on general tablespaces and mysql.ibd
  key -> space_id
  value -> {path, space_flags} */
  std::unordered_map<space_id_t, std::pair<std::string, ulint>>
      corrupted_tablespaces;
  /** Multiple copy threads can add entries to corrupted_tablespaces and
  recopy_tables concurrently */
  std::mutex m_ddl_tracker_mutex;

  std::tuple<filevec, filevec> handle_undo_ddls();

#ifdef UNIV_DEBUG
  /** Set to true when we do handle_ddl_operations. Used to assert that no
  more ddls are allowed to change the different DDL handling structures */
  bool handle_ddl_ops = false;
#endif /* UNIV_DEBUG */

  /** Check if table is in missing list
  @param[in]  name  tablespace name */
  bool is_missing_after_discovery(const std::string &name);

 public:
  /* Track undo tablespaces. This function is called twice. Once before lock,
  at startup and then again after lock. The contents of two maps are used
  to discover deleted, truncated and new undo tablespaces.
  @param[in] space_id undo tablespace id
  @param[in] name     undo tablespace name */
  void add_undo_tablespace(const space_id_t space_id, std::string name);

  /** Add a new table in the DDL tracker table list.
   @param[in] space_id  tablespace identifier
   @param[in] name      tablespace name */
  void add_table_from_ibd_scan(const space_id_t space_id, std::string name);

  /** Track a new table from the MLOG_FILE_CREATE redo log record
   @param[in] space_id        tablespace identifier
   @param[in] record_lsn      LSN of redo record
   @param[in] name            tablespace name */
  void add_create_table_from_redo(const space_id_t space_id, lsn_t record_lsn,
                                  const char *name);

  /** Track tables from the MLOG_FILE_RENAME redo log record
   @param[in]  space_id        tablespace identifier
   @param[in]  record_lsn      LSN of redo record
   @param[in]  old_name        RENAME from name
   @param[in]  new_name        RENAME to name */
  void add_rename_table_from_redo(const space_id_t space_id, lsn_t record_lsn,
                                  const char *old_name, const char *new_name);

  /** Track tables from the MLOG_FILE_DELTE redo log record
   @param[in]  space_id        tablespace identifier
   @param[in]  record_lsn      LSN of redo record
   @param[in]  name            RENAME to name */
  void add_drop_table_from_redo(const space_id_t space_id, lsn_t record_lsn,
                                const char *name);

  /** Add a table to the corrupted tablespace list. The list is later
  converted to  tablespacename.ibd.crpt files on disk
  @param[in] space_id    Tablespace id
  @param[in] path        Tablespace path
  @param[in] space_flags Tablespace flags */
  void add_corrupted_tablespace(const space_id_t space_id,
                                const std::string &path, ulint space_flags);

  /** Add a table to the recopy list. These tables are
  1. had ADD INDEX while the backup is in progress
  2. tablespace encryption change from 'y' to 'n' or viceversa
  @param[in]    space_id    Tablespace id
  @param[in]    record_lsn  LSN of redo record
  @param[in]    operation   Wheter is "add index" or "encryption" */
  void add_to_recopy_tables(space_id_t space_id, lsn_t record_lsn,
                            const std::string operation);

  /** Report an operation to create, delete, or rename a file during backup.
  @param[in] space_id   tablespace identifier
  @param[in] type       redo log file operation type
  @param[in] buf        redo log buffer
  @param[in] len        length of redo entry, in bytes
  @param[in] record_lsn lsn for REDO record */
  void backup_file_op(uint32_t space_id, mlog_id_t type, const byte *buf,
                      ulint len, lsn_t record_lsn);

  /** Handle DDL operations that happenned in reduced lock mode
  @return DB_SUCCESS for success, others for errors */
  dberr_t handle_ddl_operations();

  /** Note that a table has been deleted between disovery and file open
  @param[in]  path  missing table name with path. */
  void add_missing_after_discovery(std::string path);

  /** Note that a table was renamed during scan.
  @param[in]	space_id  tablespace identifier
  @param[in]	new_name  tablespace new name */
  void add_rename_ibd_scan(const space_id_t &space_id, std::string new_name);
};

/** Insert into meta files map. This map is later used to delete the right
.meta and .delta files for a given space_id.del file
@param[in] space_id Tablespace id
@param[in] meta_path Meta file path in the incremental backup directory */
void insert_into_meta_map(space_id_t space_id, const std::string &meta_path);

/** Check if there is a meta (.meta file) for given tablespace id
@param[in] space_id Tablespace id
@return <true, path> if a meta file exists for a given space_id,
else return <false, ""> if it doesnt exist */
std::tuple<bool, std::string> is_in_meta_map(space_id_t space_id);

/**
 * Handle DDL for renamed files
 * example input: test/10.ren file with content = test/new_name.ibd ;
 *        result: tablespace with space_id=10 will be renamed to
 * test/new_name.ibd
 * @return true on success
 */
bool prepare_handle_ren_files(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void * /*data*/);

/**
 * Handle .crpt files. These files should be removed before we do *.ibd scan
 * @return true on success
 * */
bool prepare_handle_corrupt_files(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void * /*data*/);

/**
 * Handle DDL for deleted files
 * example input: test/10.del file
 *        result: tablespace with space_id=10 will be deleted
 * @return true on success
 */
bool prepare_handle_del_files(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void *arg __attribute__((unused)));

/************************************************************************
 * Scan .meta files and build std::unordered_map<space_id, meta_path>.
 * This map is later used to delete the .delta and .meta file for a dropped
 * tablespace (ie. when processing the .del entries in reduced lock)
 * @return true on success
 */
bool xtrabackup_scan_meta(const datadir_entry_t &entry, /*!<in: datadir entry */
                          void * /*data*/);

/** Handles CREATE DDL that happened during the backup taken in reduced mode
by processing the file with `.new` extension.
example input: `schema/filename.ibd.new` file
result: file `schema/filename.ibd.new` will be renamed to `schema/filename.ibd`
@param[in] entry  datadir entry
@param[in] arg    unused
@return true on success */
bool prepare_handle_new_files(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void *arg __attribute__((unused)));

#endif  // DDL_TRACKER_H
