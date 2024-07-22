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

extern meta_map_t meta_map;

class ddl_tracker_t {
  /** List of all tables in the backup */
  space_id_to_name_t tables_in_backup;
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
  std::unordered_set<std::string> missing_tables;
  /** Tables that have been renamed during scan. Tablespace_id and new table
   *  name */
  space_id_to_name_t renamed_during_scan;

 private:
  /** Tables that cannot be decrypted during backup because of encryption
  changes. Copy threads that cannot decrypt page, considers them as corrupted
  page. Can happen only on general tablespaces and mysql.ibd */
  std::unordered_map<space_id_t, std::string> corrupted_tablespaces;
  /** Multiple copy threads can add entries to corrupted_tablespaces and
  recopy_tables concurrently */
  std::mutex m_ddl_tracker_mutex;

  std::tuple<filevec, filevec> handle_undo_ddls();

 public:
  void add_undo_tablespace(const space_id_t space_id, std::string name);

  /** Add a new table in the DDL tracker table list.
   @param[in]	space_id	tablespace identifier
   @param[in]	name      tablespace name */
  void add_table_from_ibd_scan(const space_id_t space_id, std::string name);

  /** Track a new table from the MLOG_FILE_CREATE redo log record
   @param[in]	space_id	tablespace identifier
   @param[in]	start_lsn LSN of redo record
   @param[in]	name      tablespace name */
  void add_create_table_from_redo(const space_id_t space_id, lsn_t start_lsn,
                                  const char *name);

  /** Track tables from the MLOG_FILE_RENAME redo log record
   @param[in]	space_id	tablespace identifier
   @param[in]	start_lsn LSN of redo record
   @param[in]	old_name  RENAME from name
   @param[in]	new_name  RENAME to name */
  void add_rename_table_from_redo(const space_id_t space_id, lsn_t start_lsn,
                                  const char *old_name, const char *new_name);

  /** Track tables from the MLOG_FILE_DELTE redo log record
   @param[in]	space_id	tablespace identifier
   @param[in]	start_lsn LSN of redo record
   @param[in]	name      RENAME to name */
  void add_drop_table_from_redo(const space_id_t space_id, lsn_t start_lsn,
                                const char *name);

  /** Add a table to the corrupted tablespace list. The list is later
  converted to  tablespacename.ibd.corrupt files on disk
  @param[in] space_id  Tablespace id
  @param[in] path      Tablespace path */
  void add_corrupted_tablespace(const space_id_t space_id,
                                const std::string &path);

  /** Add a table to the recopy list. These tables are
  1. had ADD INDEX while the backup is in progress
  2. tablespace encryption change from 'y' to 'n' or viceversa
  @param[in] space_id Tablespace id  */
  void add_to_recopy_tables(space_id_t space_id, lsn_t start_lsn,
                            const std::string operation);

  /** Report an operation to create, delete, or rename a file during backup.
  @param[in]	space_id	tablespace identifier
  @param[in]	type		redo log file operation type
  @param[in]	buf		redo log buffer
  @param[in]	len		length of redo entry, in bytes
  @param[in] start_lsn  lsn for REDO record */
  void backup_file_op(uint32_t space_id, mlog_id_t type, const byte *buf,
                      ulint len, lsn_t start_lsn);
  /** Function responsible to generate files based on DDL operations */
  void handle_ddl_operations();
  /** Note that a table has been deleted between disovery and file open
  @param[in]  path  missing table name with path. */
  void add_missing_table(std::string path);
  /** Check if table is in missing list
  @param[in]  name  tablespace name */
  bool is_missing_table(const std::string &name);
  std::string convert_file_name(space_id_t space_id, std::string file_name,
                                std::string ext);
  /** Note that a table was renamed during scan.
   @param[in]	space_id	tablespace identifier
   @param[in]	new_name  tablespace new name */
  void add_renamed_table(const space_id_t &space_id, std::string new_name);
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

#endif  // DDL_TRACKER_H
