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

class ddl_tracker_t {
  /** List of all tables in the backup */
  space_id_to_name_t tables_in_backup;
  /** Tablspaces with their ID and name, as they were copied to backup.*/
  space_id_to_name_t new_tables;
  /** Tablespaces involved in encryption or bulk index load.*/
  std::unordered_set<space_id_t> recopy_tables;
  /** Drop operations found in redo log. */
  space_id_to_name_t drops;
  /* For DDL operation found in redo log,  */
  std::unordered_map<space_id_t, std::pair<std::string, std::string>> renames;

 public:
  /** Add a new table in the DDL tracker table list.
   @param[in]	space_id	tablespace identifier
   @param[in]	name      tablespace name */
  void add_table(const space_id_t &space_id, std::string name);

  /** Report an operation to create, delete, or rename a file during backup.
  @param[in]	space_id	tablespace identifier
  @param[in]	type		redo log file operation type
  @param[in]	buf		redo log buffer
  @param[in]	len		length of redo entry, in bytes
  @param[in] start_lsn  lsn for REDO record */
  void backup_file_op(uint32_t space_id, mlog_id_t type, const byte *buf,
                      ulint len, lsn_t start_lsn);
};
#endif  // DDL_TRACKER_H
