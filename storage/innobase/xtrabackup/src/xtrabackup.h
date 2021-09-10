/******************************************************
Copyright (c) 2011-2015 Percona LLC and/or its affiliates.

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/

#ifndef XB_XTRABACKUP_H
#define XB_XTRABACKUP_H

#include <my_getopt.h>
#include "datasink.h"
#include "xbstream.h"
#include "changed_page_bitmap.h"
#include "xtrabackup_config.h"
#include "xb_regex.h"

#ifdef __WIN__
#define XB_FILE_UNDEFINED { NULL };
#else
const pfs_os_file_t XB_FILE_UNDEFINED = { OS_FILE_CLOSED };
#endif
inline bool operator==(const pfs_os_file_t& lhs, const pfs_os_file_t& rhs) {
	return lhs.m_file == rhs.m_file;
}
inline bool operator!=(const pfs_os_file_t& lhs, const pfs_os_file_t& rhs) {
	return !(lhs == rhs);
}

typedef struct {
	ulint	page_size;
	ulint	zip_size;
	ulint	space_id;
	ulint	space_flags;
} xb_delta_info_t;

/* ======== Datafiles iterator ======== */
typedef struct {
	fil_system_t	*system;
	fil_space_t	*space;
	fil_node_t	*node;
	ibool		started;
	ib_mutex_t	mutex;
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
extern char		*xtrabackup_tables_exclude;
extern char		*xtrabackup_databases_exclude;

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
extern longlong		innobase_page_size;

extern const char	*xtrabackup_encrypt_algo_names[];
extern TYPELIB		xtrabackup_encrypt_algo_typelib;

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

extern my_bool		opt_galera_info;
extern my_bool		opt_slave_info;
extern my_bool		opt_no_lock;
extern my_bool		opt_safe_slave_backup;
extern my_bool		opt_rsync;
extern my_bool		opt_force_non_empty_dirs;
#ifdef HAVE_VERSION_CHECK
extern my_bool		opt_noversioncheck;
#endif
extern my_bool		opt_no_backup_locks;
extern my_bool		opt_decompress;
extern my_bool		opt_remove_original;
extern my_bool		opt_no_tables_compatibility_check;

extern char		*opt_incremental_history_name;
extern char		*opt_incremental_history_uuid;

extern char		*opt_user;
extern char		*opt_password;
extern char		*opt_host;
extern char		*opt_defaults_group;
extern char		*opt_socket;
extern uint		opt_port;
extern char		*opt_login_path;
extern char		*opt_log_bin;
extern char 	*srv_log_bin;
extern char 	*opt_binlog_index_name;
extern bool  	opt_skip_log_bin;
extern bool  	srv_skip_log_bin;

extern const char 	*query_type_names[];

enum query_type_t {QUERY_TYPE_ALL, QUERY_TYPE_UPDATE,
			QUERY_TYPE_SELECT};

extern TYPELIB		query_type_typelib;

extern ulong		opt_lock_wait_query_type;
extern ulong		opt_kill_long_query_type;

extern ulong		opt_decrypt_algo;

extern uint		opt_kill_long_queries_timeout;
extern uint		opt_lock_wait_timeout;
extern uint		opt_lock_wait_threshold;
extern uint		opt_debug_sleep_before_unlock;
extern uint		opt_safe_slave_backup_timeout;

extern uint		opt_backup_lock_timeout;
extern uint		opt_backup_lock_retry_count;

extern const char	*opt_history;
extern my_bool		opt_decrypt;

extern uint		opt_read_buffer_size;

extern char		*opt_xtra_plugin_dir;
extern char		*opt_transition_key;
extern my_bool		opt_generate_new_master_key;

extern uint		opt_dump_innodb_buffer_pool_timeout;
extern uint		opt_dump_innodb_buffer_pool_pct;
extern my_bool		opt_dump_innodb_buffer_pool;

#if defined(HAVE_OPENSSL)
extern uint opt_ssl_mode;
extern my_bool opt_use_ssl_arg;
extern my_bool opt_ssl_verify_server_cert_arg;
extern my_bool ssl_mode_set_explicitly;
#if !defined(HAVE_YASSL)
extern char *opt_server_public_key;
#endif
#endif

enum binlog_info_enum { BINLOG_INFO_OFF, BINLOG_INFO_LOCKLESS, BINLOG_INFO_ON,
			BINLOG_INFO_AUTO};

extern ulong opt_binlog_info;
extern
bool compile_regex(
	const char* regex_string,
	const char* error_context,
	xb_regex_t* compiled_re);

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
const page_size_t xb_get_zip_size(pfs_os_file_t file, bool *success);

/************************************************************************
Checks if a table specified as a name in the form "database/name" (InnoDB 5.6)
or "./database/name.ibd" (InnoDB 5.5-) should be skipped from backup based on
the --tables or --tables-file options.

@return TRUE if the table should be skipped. */
bool
check_if_skip_table(
/******************/
	const char*	name);	/*!< in: path to the table */


/************************************************************************
Checks if a database specified by path should be skipped from backup based on
the --databases, --databases_file or --databases_exclude options.

@return TRUE if the table should be skipped. */
my_bool
check_if_skip_database_by_path(
	const char* path /*!< in: path to the db directory. */
);

/************************************************************************
Check if parameter is set in defaults file or via command line argument
@return true if parameter is set. */
bool
check_if_param_set(const char *param);


void
xtrabackup_backup_func(void);

my_bool
xb_get_one_option(int optid,
		  const struct my_option *opt __attribute__((unused)),
		  char *argument);

const char*
xb_get_copy_action(const char *dflt = "Copying");

#endif /* XB_XTRABACKUP_H */
