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

extern bool innodb_log_checksums_specified;
extern bool innodb_checksum_algorithm_specified;

extern bool opt_lock_ddl_per_table;
extern bool mdl_taken;

extern bool use_dumped_tablespace_keys;

extern std::vector<ulint> invalid_encrypted_tablespace_ids;

/** Fetch tablespace key from "xtrabackup_keys".
@param[in]	space_id	tablespace id
@param[out]	key		fetched tablespace key
@param[out]	key		fetched tablespace iv */
bool xb_fetch_tablespace_key(ulint space_id, byte *key, byte *iv)
    MY_ATTRIBUTE((warn_unused_result));

/** Save tablespace key for later use.
@param[in]  space_id    tablespace id
@param[in]  key     tablespace key
@param[in]  key     tablespace iv */
void xb_insert_tablespace_key(ulint space_id, const byte *key, const byte *iv);

/** Fetch tablespace key from "xtrabackup_keys" and set the encryption
type for the tablespace.
@param[in]	space		tablespace
@return DB_SUCCESS or error code */
dberr_t xb_set_encryption(fil_space_t *space)
    MY_ATTRIBUTE((warn_unused_result));

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

#endif
