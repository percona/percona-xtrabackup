/******************************************************
Copyright (c) 2011-2023 Percona LLC and/or its affiliates.

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
#include "changed_page_tracking.h"
#include "datasink.h"
#include "mysql.h"
#include "xb_dict.h"
#include "xb_regex.h"
#include "xbstream.h"
#include "xtrabackup_config.h"

#define XB_LOG_FILENAME "xtrabackup_logfile"

#ifdef __WIN__
#define XB_FILE_UNDEFINED {NULL};
#else
const pfs_os_file_t XB_FILE_UNDEFINED = {NULL, OS_FILE_CLOSED};
#endif
inline bool operator==(const pfs_os_file_t &lhs, const pfs_os_file_t &rhs) {
  return lhs.m_file == rhs.m_file;
}
inline bool operator!=(const pfs_os_file_t &lhs, const pfs_os_file_t &rhs) {
  return !(lhs == rhs);
}

typedef struct {
  ulint page_size;
  ulint zip_size;
  ulint space_id;
  ulint space_flags;
} xb_delta_info_t;

/* ======== Datafiles iterator ======== */
typedef struct {
  std::vector<fil_node_t *> nodes;
  std::vector<fil_node_t *>::iterator i;
  ib_mutex_t mutex;
} datafiles_iter_t;

enum xtrabackup_compress_t {
  XTRABACKUP_COMPRESS_NONE,
  XTRABACKUP_COMPRESS_QUICKLZ,
  XTRABACKUP_COMPRESS_LZ4,
  XTRABACKUP_COMPRESS_ZSTD
};

/* value of the --incremental option */
extern lsn_t incremental_lsn;

extern char *xtrabackup_target_dir;
extern char xtrabackup_real_target_dir[FN_REFLEN];
extern char *xtrabackup_incremental_dir;
extern char *xtrabackup_incremental_basedir;
extern char *xtrabackup_redo_log_arch_dir;
extern char *innobase_data_home_dir;
extern char *innobase_buffer_pool_filename;
extern ds_ctxt_t *ds_meta;
extern ds_ctxt_t *ds_data;
extern ds_ctxt_t *ds_uncompressed_data;

extern pagetracking::xb_space_map *changed_page_tracking;

extern ulint xtrabackup_rebuild_threads;

extern char *xtrabackup_incremental;
extern bool xtrabackup_incremental_force_scan;

extern lsn_t metadata_from_lsn;
extern lsn_t metadata_to_lsn;
extern lsn_t metadata_last_lsn;

extern xb_stream_fmt_t xtrabackup_stream_fmt;
extern bool xtrabackup_stream;

extern char *xtrabackup_tables;
extern char *xtrabackup_tables_file;
extern char *xtrabackup_databases;
extern char *xtrabackup_databases_file;
extern char *xtrabackup_tables_exclude;
extern char *xtrabackup_databases_exclude;

extern xtrabackup_compress_t xtrabackup_compress;
extern bool xtrabackup_encrypt;

extern bool xtrabackup_backup;
extern bool xtrabackup_prepare;
extern bool xtrabackup_stats;
extern bool xtrabackup_apply_log_only;
extern bool xtrabackup_copy_back;
extern bool xtrabackup_move_back;
extern bool xtrabackup_decrypt_decompress;

extern char *innobase_data_file_path;
extern char *xtrabackup_encrypt_key;
extern char *xtrabackup_encrypt_key_file;
extern longlong innobase_log_file_size;
extern long innobase_log_files_in_group;
extern longlong innobase_page_size;

extern const char *xtrabackup_encrypt_algo_names[];
extern TYPELIB xtrabackup_encrypt_algo_typelib;

extern int xtrabackup_parallel;

extern bool xb_close_files;
extern const char *xtrabackup_compress_alg;
extern uint xtrabackup_compress_threads;
extern ulonglong xtrabackup_compress_chunk_size;
extern uint xtrabackup_compress_zstd_level;
extern ulong xtrabackup_encrypt_algo;
extern uint xtrabackup_encrypt_threads;
extern ulonglong xtrabackup_encrypt_chunk_size;
extern bool xtrabackup_export;
extern char *xtrabackup_incremental_basedir;
extern char *xtrabackup_extra_lsndir;
extern char *xtrabackup_incremental_dir;
extern ulint xtrabackup_log_copy_interval;
extern char *xtrabackup_stream_str;
extern long xtrabackup_throttle;
extern longlong xtrabackup_use_memory;

extern bool opt_galera_info;
extern bool opt_slave_info;
extern bool opt_page_tracking;
extern bool opt_no_lock;
extern bool opt_safe_slave_backup;
extern bool opt_rsync;
extern bool opt_force_non_empty_dirs;
#ifdef HAVE_VERSION_CHECK
extern bool opt_noversioncheck;
#endif
extern bool opt_no_backup_locks;
extern bool opt_decompress;
extern bool opt_remove_original;
extern bool opt_no_tables_compatibility_check;

extern char *opt_incremental_history_name;
extern char *opt_incremental_history_uuid;

extern char *opt_user;
extern char *opt_password;
extern char *opt_host;
extern char *opt_defaults_group;
extern char *opt_socket;
extern uint opt_port;
extern char *opt_login_path;
extern char *opt_log_bin;
extern char *opt_binlog_index_name;

extern char *opt_rocksdb_datadir;
extern char *opt_rocksdb_wal_dir;
extern int opt_rocksdb_checkpoint_max_age;
extern int opt_rocksdb_checkpoint_max_count;

extern const char *query_type_names[];

enum query_type_t { QUERY_TYPE_ALL, QUERY_TYPE_UPDATE, QUERY_TYPE_SELECT };

extern TYPELIB query_type_typelib;

extern ulong opt_lock_wait_query_type;
extern ulong opt_kill_long_query_type;

extern ulong opt_decrypt_algo;

extern uint opt_kill_long_queries_timeout;
extern uint opt_lock_wait_timeout;
extern uint opt_lock_wait_threshold;
extern uint opt_debug_sleep_before_unlock;
extern uint opt_safe_slave_backup_timeout;
extern uint opt_backup_lock_timeout;
extern uint opt_backup_lock_retry_count;

extern const char *opt_history;
extern char *opt_history_user;
extern char *opt_history_password;
extern char *opt_history_host;
extern uint opt_history_port;

extern bool opt_decrypt;

extern uint opt_read_buffer_size;

extern char *opt_xtra_plugin_dir;
extern char *server_plugin_dir;
extern char *opt_transition_key;
extern char *opt_keyring_file_data;
extern char *opt_component_keyring_config;
extern bool opt_generate_transition_key;
extern bool opt_generate_new_master_key;

extern uint opt_dump_innodb_buffer_pool_timeout;
extern uint opt_dump_innodb_buffer_pool_pct;
extern bool opt_dump_innodb_buffer_pool;

extern bool punch_hole_supported;
extern bool compile_regex(const char *regex_string, const char *error_context,
                          xb_regex_t *compiled_re);
/* sslopt-vars.h */
extern uint opt_ssl_mode;
extern char *opt_ssl_ca;
extern char *opt_ssl_capath;
extern char *opt_ssl_cert;
extern char *opt_ssl_cipher;
extern char *opt_ssl_key;
extern char *opt_ssl_crl;
extern char *opt_ssl_crlpath;
extern char *opt_tls_version;
extern bool ssl_mode_set_explicitly;
extern int set_client_ssl_options(MYSQL *mysql);

extern bool xtrabackup_register_redo_log_consumer;
extern std::atomic<bool> redo_log_consumer_can_advance;

enum binlog_info_enum {
  BINLOG_INFO_OFF,
  BINLOG_INFO_LOCKLESS,
  BINLOG_INFO_ON,
  BINLOG_INFO_AUTO
};

void xtrabackup_io_throttling(void);
bool xb_write_delta_metadata(const char *filename, const xb_delta_info_t *info);

datafiles_iter_t *datafiles_iter_new(
    const std::shared_ptr<const xb::backup::dd_space_ids>);
fil_node_t *datafiles_iter_next(datafiles_iter_t *it);
void datafiles_iter_free(datafiles_iter_t *it);

/************************************************************************
Initialize the tablespace memory cache and populate it by scanning for and
opening data files */
ulint xb_data_files_init(void);

/************************************************************************
Destroy the tablespace memory cache. */
void xb_data_files_close(void);

/************************************************************************
Checks if a database specified by path should be skipped from backup based on
the --databases, --databases_file or --databases_exclude options.

@return true if the table should be skipped. */
bool check_if_skip_database_by_path(
    const char *path /*!< in: path to the db directory. */
);

/************************************************************************
Check if parameter is set in defaults file or via command line argument
@return true if parameter is set. */
bool check_if_param_set(const char *param);

void xtrabackup_backup_func(void);

bool xb_get_one_option(int optid,
                       const struct my_option *opt __attribute__((unused)),
                       char *argument);

const char *xb_get_copy_action(const char *dflt = "Copying");

struct datadir_entry_t {
  std::string datadir;
  std::string path;
  std::string db_name;
  std::string file_name;
  std::string rel_path;
  bool is_empty_dir;
  ssize_t file_size;

  datadir_entry_t()
      : datadir(), path(), db_name(), file_name(), rel_path(), is_empty_dir() {}

  datadir_entry_t(const datadir_entry_t &) = default;

  datadir_entry_t &operator=(const datadir_entry_t &) = default;

  datadir_entry_t(const char *datadir, const char *path, const char *db_name,
                  const char *file_name, bool is_empty_dir,
                  ssize_t file_size = -1)
      : datadir(datadir),
        path(path),
        db_name(db_name),
        file_name(file_name),
        is_empty_dir(is_empty_dir),
        file_size(file_size) {
    if (db_name != nullptr && *db_name != 0) {
      rel_path =
          std::string(db_name) + std::string("/") + std::string(file_name);
    } else {
      rel_path = std::string(file_name);
    }
  }
};

/************************************************************************
Callback to handle datadir entry. Function of this type will be called
for each entry which matches the mask by xb_process_datadir.
@return should return true on success */
typedef std::function<bool(
    /*=========================================*/
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void *arg)>                   /*!<in: caller-provided data */
    handle_datadir_entry_func_t;

/************************************************************************
Function enumerates files in datadir (provided by path) which are matched
by provided suffix. For each entry callback is called.
@return false if callback for some entry returned false */
bool xb_process_datadir(const char *path,   /*!<in: datadir path */
                        const char *suffix, /*!<in: suffix to match
                                            against */
                        handle_datadir_entry_func_t func, /*!<in: callback */
                        void *data); /*!<in: additional argument for
                                     callback */

/** update the checkpoint and recalculate the checksum of log header
@param[in,out]	buf		log header buffer
@param[in]	lsn		lsn to update */
void update_log_temp_checkpoint(byte *buf, lsn_t lsn);
#endif /* XB_XTRABACKUP_H */
