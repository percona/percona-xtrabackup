/******************************************************
Copyright (c) 2011-2014 Percona LLC and/or its affiliates.

Declarations for xtrabackup.cc

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

#include <my_getopt.h>
#include "datasink.h"
#include "xbstream.h"
#include "changed_page_bitmap.h"

#ifdef __WIN__
#define XB_FILE_UNDEFINED NULL
#else
#define XB_FILE_UNDEFINED (-1)
#endif

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
extern char		*xtrabackup_incremental_dir;
extern char		*xtrabackup_incremental_basedir;
extern char		*innobase_data_home_dir;
extern char		*innobase_buffer_pool_filename;
extern ds_ctxt_t	*ds_meta;
extern ds_ctxt_t	*ds_data;

/* The last checkpoint LSN at the backup startup time */
extern lsn_t checkpoint_lsn_start;

extern xb_page_bitmap *changed_page_bitmap;

extern ulint	xtrabackup_rebuild_threads;

extern my_option	xb_long_options[];
extern uint		xb_long_options_count;

extern char		*xtrabackup_incremental;
extern my_bool		xtrabackup_incremental_force_scan;

extern lsn_t		metadata_from_lsn;
extern lsn_t		metadata_to_lsn;
extern lsn_t		metadata_last_lsn;

extern xb_stream_fmt_t	xtrabackup_stream_fmt;
extern ibool		xtrabackup_stream;

extern char		*xtrabackup_tables;
extern char		*xtrabackup_tables_file;
extern char		*xtrabackup_databases;
extern char		*xtrabackup_databases_file;

extern my_bool		xtrabackup_compact;
extern ibool		xtrabackup_compress;
extern ibool		xtrabackup_encrypt;

extern my_bool		xtrabackup_backup;
extern my_bool		xtrabackup_prepare;
extern my_bool		xtrabackup_apply_log_only;
extern my_bool		xtrabackup_copy_back;
extern my_bool		xtrabackup_move_back;
extern my_bool		xtrabackup_decrypt_decompress;

extern char		*innobase_data_file_path;
extern char		*innobase_doublewrite_file;
extern char		*xtrabackup_encrypt_key;
extern char		*xtrabackup_encrypt_key_file;
extern longlong		innobase_log_file_size;
extern long		innobase_log_files_in_group;

extern const char	*xtrabackup_encrypt_algo_names[];
extern TYPELIB		xtrabackup_encrypt_algo_typelib;

extern bool		xtrabackup_innodb_data_file_path_explicit;
extern bool		xtrabackup_innodb_log_file_size_explicit;

extern int		xtrabackup_parallel;

extern my_bool		xb_close_files;
extern const char	*xtrabackup_compress_alg;
extern uint		xtrabackup_compress_threads;
extern ulonglong	xtrabackup_compress_chunk_size;
extern ulong		xtrabackup_encrypt_algo;
extern uint		xtrabackup_encrypt_threads;
extern ulonglong	xtrabackup_encrypt_chunk_size;
extern my_bool		xtrabackup_export;
extern char		*xtrabackup_incremental_basedir;
extern char		*xtrabackup_extra_lsndir;
extern char		*xtrabackup_incremental_dir;
extern ulint		xtrabackup_log_copy_interval;
extern my_bool		xtrabackup_rebuild_indexes;
extern char		*xtrabackup_stream_str;
extern long		xtrabackup_throttle;
extern longlong		xtrabackup_use_memory;

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

/************************************************************************
Checks if a table specified as a name in the form "database/name" (InnoDB 5.6)
or "./database/name.ibd" (InnoDB 5.5-) should be skipped from backup based on
the --tables or --tables-file options.

@return TRUE if the table should be skipped. */
my_bool
check_if_skip_table(
/******************/
	const char*	name);	/*!< in: path to the table */

void
xtrabackup_backup_func(void);

my_bool
xb_get_one_option(int optid,
		  const struct my_option *opt __attribute__((unused)),
		  char *argument);

const char*
xb_get_copy_action(const char *dflt = "Copying");

#endif /* XB_XTRABACKUP_H */
