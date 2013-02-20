/******************************************************
Copyright (c) 2012 Percona Ireland Ltd.

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#ifndef xb0xb_h
#define xb0xb_h

#ifdef __cplusplus
extern "C" {
#endif

extern ibool srv_compact_backup;
extern ibool srv_rebuild_indexes;

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

#ifdef __cplusplus
}
#endif

#endif
