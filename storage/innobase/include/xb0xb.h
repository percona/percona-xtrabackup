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

#ifdef __cplusplus
extern "C" {
#endif

extern ibool srv_compact_backup;
extern ibool srv_rebuild_indexes;

typedef enum {REDO_LOG_V0, REDO_LOG_V1} redo_log_version_t;
extern ulint redo_log_version;

extern bool innodb_log_checksum_algorithm_specified;
extern bool innodb_checksum_algorithm_specified;

extern my_bool opt_lock_ddl_per_table;

extern bool use_dumped_tablespace_keys;

extern std::vector<ulint> invalid_encrypted_tablespace_ids;
/******************************************************************************
Callback used in buf_page_io_complete() to detect compacted pages.
@return TRUE if the page is marked as compacted, FALSE otherwise. */
ibool
buf_page_is_compacted(
/*==================*/
	const byte*	page);	/*!< in: a database page */

/******************************************************************************
Rebuild all secondary indexes in all tables in separate spaces. Called from
innobase_start_or_create_for_mysql(). */
void
xb_compact_rebuild_indexes(void);

/** Fetch tablespace key from "xtrabackup_keys".
@param[in]	space_id	tablespace id
@param[out]	key		fetched tablespace key
@param[out]	key		fetched tablespace iv */
void
xb_fetch_tablespace_key(ulint space_id, byte *key, byte *iv);

#ifdef __cplusplus
}
#endif

#endif
