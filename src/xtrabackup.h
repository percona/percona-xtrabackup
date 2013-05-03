/******************************************************
Copyright (c) 2011 Percona Ireland Ltd.

Declarations for xtrabackup.c

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

#ifndef XB_XTRABACKUP_H
#define XB_XTRABACKUP_H

#include "datasink.h"
#include "innodb_int.h"
#include "changed_page_bitmap.h"

typedef struct {
	ulint	page_size;
	ulint	zip_size;
	ulint	space_id;
} xb_delta_info_t;

/* ======== Datafiles iterator ======== */
typedef struct {
	fil_system_t	*system;
	fil_space_t	*space;
	fil_node_t	*node;
	ibool		started;
	os_ib_mutex_t	mutex;
} datafiles_iter_t;

/* value of the --incremental option */
extern lsn_t incremental_lsn;

extern char		*xtrabackup_target_dir;
extern ds_ctxt_t	*ds_meta;
extern ds_ctxt_t	*ds_data;

/* The last checkpoint LSN at the backup startup time */
extern lsn_t checkpoint_lsn_start;

extern xb_page_bitmap *changed_page_bitmap;

extern ulint	xtrabackup_rebuild_threads;

void xtrabackup_io_throttling(void);
my_bool xb_write_delta_metadata(const char *filename,
				const xb_delta_info_t *info);

datafiles_iter_t *datafiles_iter_new(fil_system_t *f_system);
fil_node_t *datafiles_iter_next(datafiles_iter_t *it);
void datafiles_iter_free(datafiles_iter_t *it);

/************************************************************************
Initialize the tablespace memory cache and populate it by scanning for and
opening data files */
ulint xb_data_files_init(void);

/************************************************************************
Destroy the tablespace memory cache. */
void xb_data_files_close(void);

/***********************************************************************
Reads the space flags from a given data file and returns the compressed
page size, or 0 if the space is not compressed. */
ulint xb_get_zip_size(os_file_t file);

#endif /* XB_XTRABACKUP_H */
