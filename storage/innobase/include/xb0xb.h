/******************************************************
Copyright (c) 2012 Percona LLC and/or its affiliates.

Declarations of XtraBackup functions called by InnoDB code.

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

#ifndef xb0xb_h
#define xb0xb_h
#include "storage/innobase/xtrabackup/src/ddl_tracker.h"

extern bool innodb_log_checksums_specified;
extern bool innodb_checksum_algorithm_specified;

extern bool opt_lock_ddl_per_table;
extern bool redo_catchup_completed;
extern bool opt_page_tracking;
extern char *xtrabackup_incremental;
extern lsn_t incremental_start_checkpoint_lsn;
extern lsn_t xtrabackup_start_checkpoint;
extern bool use_dumped_tablespace_keys;
extern unsigned long xb_backup_version;
extern bool xb_generated_redo;
enum lock_ddl_type_t { LOCK_DDL_OFF, LOCK_DDL_ON, LOCK_DDL_REDUCED };
extern lock_ddl_type_t opt_lock_ddl;

extern ddl_tracker_t *ddl_tracker;
extern std::vector<ulint> invalid_encrypted_tablespace_ids;

/** Fetch tablespace key from "xtrabackup_keys".
@param[in]	space_id	tablespace id
@param[out]	key		fetched tablespace key
@param[out]	key		fetched tablespace iv */
[[nodiscard]] bool xb_fetch_tablespace_key(ulint space_id, byte *key, byte *iv);

/** Save tablespace key for later use.
@param[in]  space_id    tablespace id
@param[in]  key     tablespace key
@param[in]  key     tablespace iv */
void xb_insert_tablespace_key(ulint space_id, const byte *key, const byte *iv);

/** Fetch tablespace key from "xtrabackup_keys" and set the encryption
type for the tablespace.
@param[in]	space		tablespace
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t xb_set_encryption(fil_space_t *space);

/** Add file to tablespace map.
@param[in]	file_name	file name
@param[in]	tablespace_name	corresponding tablespace name */
void xb_tablespace_map_add(const char *file_name, const char *tablespace_name);

/** Delete tablespace from mapping.
@param[in]	tablespace_name	tablespace name */
void xb_tablespace_map_delete(const char *tablespace_name);

/** Lookup backup file name for given file.
@param[in]	file_name	file name
@return		local file name */
std::string xb_tablespace_backup_file_path(const std::string &file_name);

/************************************************************************
Checks if a table specified as a name in the form "database/name" (InnoDB 5.6)
or "./database/name.ibd" (InnoDB 5.5-) should be skipped from backup based on
the --tables or --tables-file options.
@return TRUE if the table should be skipped. */
bool check_if_skip_table(
    /******************/
    const char *name); /*!< in: path to the table */

/** Amount of memory calculated at --backup for recovery hash records */
extern size_t real_redo_memory;

/** Number of total frames that will be required for prepare */
extern ulint real_redo_frames;

/** This variables holds the result of all conditions that must be set in order
to enable estimate memory functionality. Used at --prepare */
extern bool estimate_memory;

/** Parameter to enable estimate memory. Used at --backup */
extern bool xtrabackup_estimate_memory;
#define SQUOTE(str) "'" << str << "'"

const std::string KEYRING_NOT_LOADED =
    "Unable to decrypt. Please check if xtrabackup is configured correctly to "
    "access the keyring plugin or component. Check --xtrabackup-plugin-dir. "
    "Also verify if valid keyring_file_data is passed with the option "
    "--keyring_file_data. If keyring component is used, check if "
    "--component-keyring-config points to valid configuration";

/** pause xtrabackup and wait for resume.
@param[in]	name	sync point name */
void debug_sync_point(const char *name);

#ifdef UNIV_DEBUG
/** Pause xtrabackup thread and wait for resume.
Thread can be resumed by deleting the sync_point filename
@param[in]	name	sync point name */
void debug_sync_thread(const char *name);
#else
#define debug_sync_thread(A)
#endif /* UNIV_DEBUG */

extern char *xtrabackup_debug_sync;

/** @return true if xtrabackup has locked Server with LOCK INSTANCE FOR BACKP or
LOCK TABLES FOR BACKUP */
bool is_server_locked();
#endif
