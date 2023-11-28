/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009-2023 Percona LLC and/or its affiliates
Originally Created 3/3/2009 Yasufumi Kinoshita
Written by Alexey Kopytov, Aleksandr Kuzminsky, Stewart Smith, Vadim Tkachenko,
Yasufumi Kinoshita, Ignacio Nin and Baron Schwartz.

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

*******************************************************

This file incorporates work covered by the following copyright and
permission notice:

Copyright (c) 2000, 2011, MySQL AB & Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#include <my_base.h>
#include <my_default.h>
#include <my_getopt.h>
#include <my_rnd.h>
#include <mysql_version.h>
#include <mysqld.h>
#include "log0files_io.h"
#include "log0types.h"
#include "row0quiesce.h"

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include "os0event.h"
#include "xb_dict.h"

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include <sys/resource.h>

#include <btr0sea.h>
#include <buf0dblwr.h>
#include <dict0dd.h>
#include <dict0priv.h>
#include <dict0stats.h>
#include <lob0lob.h>
#include <lock0lock.h>
#include <log0recv.h>
#include <my_aes.h>
#include <row0mysql.h>
#include <row0quiesce.h>
#include <sql_locale.h>
#include <srv0srv.h>
#include <srv0start.h>
#include "sql/xa/transaction_cache.h"

#include <clone0api.h>
#include <mysql.h>
#include <mysqld_thd_manager.h>
#include <sql/current_thd.h>
#include <sql/srv_session.h>
#include <table_cache.h>
#include <list>
#include <set>
#include <sstream>

#include <api0api.h>
#include <api0misc.h>

#include "sql_thd_internal_api.h"

#define G_PTR uchar *

#include "common.h"
#include "datasink.h"
#include "xtrabackup_version.h"

#include "backup_copy.h"
#include "backup_mysql.h"
#include "changed_page_tracking.h"
#include "crc_glue.h"
#include "ddl_tracker.h"
#include "ds_buffer.h"
#include "ds_encrypt.h"
#include "ds_tmpfile.h"
#include "fil_cur.h"
#include "keyring_components.h"
#include "keyring_plugins.h"
#include "read_filt.h"
#include "redo_log.h"
#include "space_map.h"
#include "utils.h"
#include "write_filt.h"
#include "wsrep.h"
#include "xb0xb.h"
#include "xb_regex.h"
#include "xbcrypt_common.h"
#include "xbstream.h"
#include "xtrabackup.h"
#include "xtrabackup_config.h"

/* TODO: replace with appropriate macros used in InnoDB 5.6 */
#define PAGE_ZIP_MIN_SIZE_SHIFT 10
#define DICT_TF_ZSSIZE_SHIFT 1
#define DICT_TF_FORMAT_ZIP 1
#define DICT_TF_FORMAT_SHIFT 5
static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

/* we cannot include sql/log.h because it conflicts with innodb headers */
bool init_error_log();
void destroy_error_log();
int sys_var_init();

bool innodb_inited = 0;

/* This tablespace name is reserved by InnoDB for the system tablespace
which uses space_id 0 and stores extra types of system pages like UNDO
and doublewrite. */
const char reserved_system_space_name[] = "innodb_system";

/* This tablespace name is reserved by InnoDB for the predefined temporary
tablespace. */
const char reserved_temporary_space_name[] = "innodb_temporary";

/* === xtrabackup specific options === */
char xtrabackup_real_target_dir[FN_REFLEN] = "./xtrabackup_backupfiles/";
char *xtrabackup_target_dir = xtrabackup_real_target_dir;
bool xtrabackup_version = false;
bool xtrabackup_backup = false;
bool xtrabackup_prepare = false;
bool xtrabackup_copy_back = false;
bool xtrabackup_move_back = false;
bool xtrabackup_decrypt_decompress = false;
bool xtrabackup_print_param = false;

bool xtrabackup_export = false;
bool xtrabackup_apply_log_only = false;

longlong xtrabackup_use_memory = 100 * 1024 * 1024L;
bool xtrabackup_use_memory_set = false;
uint xtrabackup_use_free_memory_pct = 0;
bool estimate_memory = false;
bool xtrabackup_estimate_memory = false;

bool xtrabackup_create_ib_logfile = false;

long xtrabackup_throttle = 0; /* 0:unlimited */
lint io_ticket;
os_event_t wait_throttle = NULL;

char *xtrabackup_incremental = NULL;
lsn_t incremental_lsn;
lsn_t incremental_start_checkpoint_lsn;
lsn_t incremental_to_lsn;
lsn_t incremental_last_lsn;
lsn_t incremental_flushed_lsn;
size_t incremental_redo_memory = 0;
ulint incremental_redo_frames = 0;

pagetracking::xb_space_map *changed_page_tracking = nullptr;

char *xtrabackup_incremental_basedir = NULL; /* for --backup */
char *xtrabackup_extra_lsndir = NULL;    /* for --backup with --extra-lsndir */
char *xtrabackup_incremental_dir = NULL; /* for --prepare */
char *xtrabackup_redo_log_arch_dir = NULL;

char xtrabackup_real_incremental_basedir[FN_REFLEN];
char xtrabackup_real_extra_lsndir[FN_REFLEN];
char xtrabackup_real_incremental_dir[FN_REFLEN];

lsn_t xtrabackup_archived_to_lsn = 0; /* for --archived-to-lsn */

char *xtrabackup_tables = NULL;
char *xtrabackup_tables_file = NULL;
char *xtrabackup_tables_exclude = NULL;

lsn_t xtrabackup_start_checkpoint;

bool xtrabackup_register_redo_log_consumer = false;
std::atomic<bool> redo_log_consumer_can_advance = false;

typedef std::list<xb_regex_t> regex_list_t;
static regex_list_t regex_include_list;
static regex_list_t regex_exclude_list;

static hash_table_t *tables_include_hash = NULL;
static hash_table_t *tables_exclude_hash = NULL;

static uint32_t cfg_version = IB_EXPORT_CFG_VERSION_V7;
char *xtrabackup_databases = NULL;
char *xtrabackup_databases_file = NULL;
char *xtrabackup_databases_exclude = NULL;
static hash_table_t *databases_include_hash = NULL;
static hash_table_t *databases_exclude_hash = NULL;

static hash_table_t *inc_dir_tables_hash;

struct xb_filter_entry_struct {
  char *name;
  bool has_tables;
  hash_node_t name_hash;
};
typedef struct xb_filter_entry_struct xb_filter_entry_t;

bool io_watching_thread_stop = false;
bool io_watching_thread_running = false;

bool xtrabackup_logfile_is_renamed = false;

int xtrabackup_parallel;
int xtrabackup_fifo_streams;
bool xtrabackup_fifo_streams_set = false;
uint xtrabackup_fifo_timeout = 60;
char *xtrabackup_fifo_dir = NULL;
bool opt_strict = true;

char *xtrabackup_stream_str = NULL;
xb_stream_fmt_t xtrabackup_stream_fmt = XB_STREAM_FMT_NONE;
bool xtrabackup_stream = false;

const char *xtrabackup_compress_alg = NULL;
xtrabackup_compress_t xtrabackup_compress = XTRABACKUP_COMPRESS_NONE;
ds_type_t xtrabackup_compress_ds;
uint xtrabackup_compress_threads;
ulonglong xtrabackup_compress_chunk_size = 0;
uint xtrabackup_compress_zstd_level = 1;

const char *xtrabackup_encrypt_algo_names[] = {"NONE", "AES128", "AES192",
                                               "AES256", NullS};
TYPELIB xtrabackup_encrypt_algo_typelib = {
    array_elements(xtrabackup_encrypt_algo_names) - 1, "",
    xtrabackup_encrypt_algo_names, NULL};

bool xtrabackup_encrypt = false;
ulong xtrabackup_encrypt_algo;
char *xtrabackup_encrypt_key = NULL;
char *xtrabackup_encrypt_key_file = NULL;
uint xtrabackup_encrypt_threads;
ulonglong xtrabackup_encrypt_chunk_size = 0;

size_t redo_memory = 0;
ulint redo_frames = 0;
size_t real_redo_memory = 0;
ulint real_redo_frames = UINT64_MAX;

ulint xtrabackup_rebuild_threads = 1;

/* sleep interval beetween log copy iterations in log copying thread
in milliseconds (default is 1 second) */
ulint xtrabackup_log_copy_interval = 1000;

/* Ignored option (--log) for MySQL option compatibility */
char *log_ignored_opt = NULL;

/* === metadata of backup === */
#define XTRABACKUP_METADATA_FILENAME "xtrabackup_checkpoints"
static char metadata_type_str[30] = ""; /*[full-backuped|log-applied|
                                        full-prepared|incremental]*/
static enum {
  METADATA_FULL_BACKUP,
  METADATA_INCREMENTAL_BACKUP,
  METADATA_LOG_APPLIED,
  METADATA_FULL_PREPARED
} metadata_type;
lsn_t metadata_from_lsn = 0;
lsn_t metadata_to_lsn = 0;
lsn_t metadata_last_lsn = 0;

ds_file_t *dst_log_file = NULL;

static char mysql_data_home_buff[2];

const char *defaults_group = "mysqld";

/* === static parameters in ha_innodb.cc */

#define HA_INNOBASE_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_INNOBASE_RANGE_COUNT 100

ulong innobase_large_page_size = 0;

/* The default values for the following, type long or longlong, start-up
parameters are declared in mysqld.cc: */

long innobase_buffer_pool_awe_mem_mb = 0;
long innobase_read_io_threads = 4;
long innobase_write_io_threads = 4;
long innobase_force_recovery = 0;
long innobase_log_buffer_size = 16 * 1024 * 1024L;
long innobase_log_files_in_group = 2;
long innobase_open_files = 300L;

longlong innobase_page_size = (1LL << 14); /* 16KB */
static ulong innobase_log_block_size = 512;
char *innobase_buffer_pool_filename = NULL;

longlong innobase_buffer_pool_size = 8 * 1024 * 1024L;
longlong innobase_log_file_size = 48 * 1024 * 1024L;

static ulong innodb_flush_method;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

char *innobase_ignored_opt = NULL;
char *innobase_data_home_dir = NULL;
char *innobase_data_file_path = NULL;
char *innobase_temp_data_file_path = NULL;

/* Below we have boolean-valued start-up parameters, and their default
values */

ulong innobase_fast_shutdown = 1;
bool innobase_use_checksums = true;
bool innobase_use_large_pages = false;
bool innobase_file_per_table = false;
bool innobase_rollback_on_timeout = false;
bool innobase_create_status_file = false;
bool innobase_adaptive_hash_index = true;

static char *internal_innobase_data_file_path = NULL;

char *opt_transition_key = NULL;
char *opt_xtra_plugin_dir = NULL;
char *server_plugin_dir = NULL;
char *opt_xtra_plugin_load = NULL;
char *opt_keyring_file_data = nullptr;
char *opt_component_keyring_config = nullptr;
char *opt_component_keyring_file_config = nullptr;

bool opt_generate_new_master_key = false;
bool opt_generate_transition_key = false;

bool use_dumped_tablespace_keys = false;

/* The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL 32
ulong innobase_active_counter = 0;

char *xtrabackup_debug_sync = NULL;
static const char *dbug_setting = nullptr;

bool xtrabackup_incremental_force_scan = false;

/* The flushed lsn which is read from data files */
lsn_t min_flushed_lsn = 0;
lsn_t max_flushed_lsn = 0;

/* The size of archived log file */
size_t xtrabackup_arch_file_size = 0ULL;
/* The minimal LSN of found archived log files */
lsn_t xtrabackup_arch_first_file_lsn = 0ULL;
/* The maximum LSN of found archived log files */
lsn_t xtrabackup_arch_last_file_lsn = 0ULL;

ulong xb_open_files_limit = 0;
bool xb_close_files = false;

/* Datasinks */
ds_ctxt_t *ds_data = nullptr;
ds_ctxt_t *ds_meta = nullptr;
ds_ctxt_t *ds_redo = nullptr;
ds_ctxt_t *ds_uncompressed_data = nullptr;

static long innobase_log_files_in_group_save;
static char *srv_log_group_home_dir_save;
static longlong innobase_log_file_size_save;

static char *srv_temp_dir = nullptr;

/* set true if corresponding variable set as option config file or
command argument */
bool innodb_log_checksums_specified = false;

/* set true if corresponding variable set as option config file or
command argument */
bool innodb_checksum_algorithm_specified = false;

/* String buffer used by --print-param to accumulate server options as they are
parsed from the defaults file */
static std::ostringstream print_param_str;
static std::ostringstream param_str;

/* Set of specified parameters */
std::set<std::string> param_set;

static ulonglong global_max_value;

extern "C" void handle_fatal_signal(int sig);

bool opt_galera_info = false;
bool opt_slave_info = false;
bool opt_page_tracking = false;
bool opt_no_lock = false;
bool opt_safe_slave_backup = false;
bool opt_rsync = false;
bool opt_force_non_empty_dirs = false;
#ifdef HAVE_VERSION_CHECK
bool opt_noversioncheck = false;
#endif
bool opt_no_server_version_check = false;
bool opt_no_backup_locks = false;
bool opt_decompress = false;
bool opt_remove_original = false;
bool opt_tables_compatibility_check = true;
static bool opt_check_privileges = false;

char *opt_incremental_history_name = NULL;
char *opt_incremental_history_uuid = NULL;

char *opt_user = NULL;
char *opt_password = NULL;
char *opt_host = NULL;
char *opt_defaults_group = NULL;
char *opt_socket = NULL;
uint opt_port = 0;
char *opt_login_path = NULL;
char *opt_log_bin = NULL;

bool tty_password = false;
bool tty_transition_key = false;

const char *query_type_names[] = {"ALL", "UPDATE", "SELECT", NullS};

TYPELIB query_type_typelib = {array_elements(query_type_names) - 1, "",
                              query_type_names, NULL};

ulong opt_lock_wait_query_type;
ulong opt_kill_long_query_type;

ulong opt_decrypt_algo = 0;

uint opt_kill_long_queries_timeout = 0;
uint opt_lock_wait_timeout = 0;
uint opt_lock_wait_threshold = 0;
uint opt_debug_sleep_before_unlock = 0;
uint opt_safe_slave_backup_timeout = 0;
uint opt_dump_innodb_buffer_pool_timeout = 10;
uint opt_dump_innodb_buffer_pool_pct = 0;
bool opt_dump_innodb_buffer_pool = false;

bool punch_hole_supported = false;

const char *xtrabackup_lock_ddl_str = NULL;
lock_ddl_type_t opt_lock_ddl = LOCK_DDL_ON;
bool opt_lock_ddl_per_table = false;
uint opt_lock_ddl_timeout = 0;
uint opt_backup_lock_timeout = 0;
uint opt_backup_lock_retry_count = 0;

const char *opt_history = NULL;
bool opt_decrypt = false;
uint opt_read_buffer_size = 0;

char *opt_rocksdb_datadir = nullptr;
char *opt_rocksdb_wal_dir = nullptr;

int opt_rocksdb_checkpoint_max_age = 0;
int opt_rocksdb_checkpoint_max_count = 0;

/** Possible values for system variable "innodb_checksum_algorithm". */
extern const char *innodb_checksum_algorithm_names[];

/** Used to define an enumerate type of the system variable
innodb_checksum_algorithm. */
extern TYPELIB innodb_checksum_algorithm_typelib;

/** Names of allowed values of innodb_flush_method */
extern const char *innodb_flush_method_names[];

/** List of tablespaces missing encryption data when we validated its first
page. We test again in the end of backup. */
std::vector<ulint> invalid_encrypted_tablespace_ids;

/** Enumeration of innodb_flush_method */
extern TYPELIB innodb_flush_method_typelib;

/* version variables populated by xtrabackup_read_info */
char mysql_server_version_str[30] = ""; /* 8.0.20.debug| 8.0.20 */
char xtrabackup_version_str[30] = "";   /* 8.0.20.debug| 8.0.20 */
char mysql_server_flavor[256] = "";     /* Percona|MariaDB|Oracle */
unsigned long xb_server_version;

#include "caching_sha2_passwordopt-vars.h"
#include "sslopt-vars.h"

bool redo_catchup_completed = false;
extern struct rand_struct sql_rand;
extern mysql_mutex_t LOCK_sql_rand;
bool xb_generated_redo = false;
ddl_tracker_t *ddl_tracker = nullptr;

static void check_all_privileges();
static bool validate_options(const char *file, int argc, char **argv);
/** all tables in mysql databases. These should be closed at the end.
We have to keep them open (table->n_ref_count >0) because we don't want
them to be evicted */
static std::vector<dict_table_t *> mysql_ibd_tables;

#define CLIENT_WARN_DEPRECATED(opt, new_opt)                              \
  xb::warn() << opt                                                       \
             << " is deprecated and will be removed in a future version." \
             << " Use " << new_opt << " instead"

/* Simple datasink creation tracking...add datasinks in the reverse order you
want them destroyed. */
#define XTRABACKUP_MAX_DATASINKS 10
static ds_ctxt_t *datasinks[XTRABACKUP_MAX_DATASINKS];
static uint actual_datasinks = 0;
static inline void xtrabackup_add_datasink(ds_ctxt_t *ds) {
  xb_ad(actual_datasinks < XTRABACKUP_MAX_DATASINKS);
  datasinks[actual_datasinks] = ds;
  actual_datasinks++;
}

/* ======== Datafiles iterator ======== */
datafiles_iter_t *datafiles_iter_new(
    const std::shared_ptr<const xb::backup::dd_space_ids> dd_spaces) {
  datafiles_iter_t *it = new datafiles_iter_t();

  mutex_create(LATCH_ID_XTRA_DATAFILES_ITER_MUTEX, &it->mutex);

  Fil_iterator::for_each_file([&](fil_node_t *file) {
    auto space_id = file->space->id;

    ut_ad(dd_spaces == nullptr || (dd_spaces != nullptr && srv_backup_mode &&
                                   opt_lock_ddl == LOCK_DDL_ON));

    if (dd_spaces != nullptr && fsp_is_ibd_tablespace(space_id) &&
        !fsp_is_system_or_temp_tablespace(space_id) &&
        !fsp_is_undo_tablespace(space_id) && dd_spaces->count(space_id) == 0) {
      xb::warn() << "Skipping, Tablespace " << file->name
                 << " space_id: " << space_id << " does not exists"
                 << " in INFORMATION_SCHEMA.INNODB_TABLESPACES";
    } else {
      it->nodes.push_back(file);
    }
    return (DB_SUCCESS);
  });

  it->i = it->nodes.begin();

  return it;
}

fil_node_t *datafiles_iter_next(datafiles_iter_t *it) {
  fil_node_t *node = NULL;

  mutex_enter(&it->mutex);

  if (it->i != it->nodes.end()) {
    node = *it->i;
    it->i++;
  }

  mutex_exit(&it->mutex);

  return node;
}

void datafiles_iter_free(datafiles_iter_t *it) {
  mutex_free(&it->mutex);
  delete it;
}

/* ======== Date copying thread context ======== */

typedef struct {
  datafiles_iter_t *it;
  uint num;
  uint *count;
  ib_mutex_t *count_mutex;
  bool *error;
  std::thread::id id;
} data_thread_ctxt_t;

/* ======== for option and variables ======== */

enum options_xtrabackup {
  OPT_XTRA_TARGET_DIR = 1000, /* make sure it is larger
                                 than OPT_MAX_CLIENT_OPTION */
  OPT_XTRA_BACKUP,
  OPT_XTRA_PREPARE,
  OPT_XTRA_EXPORT,
  OPT_XTRA_APPLY_LOG_ONLY,
  OPT_XTRA_PRINT_PARAM,
  OPT_XTRA_USE_MEMORY,
  OPT_XTRA_USE_FREE_MEMORY_PCT,
  OPT_XTRA_ESTIMATE_MEMORY,
  OPT_XTRA_THROTTLE,
  OPT_XTRA_LOG_COPY_INTERVAL,
  OPT_XTRA_INCREMENTAL,
  OPT_XTRA_INCREMENTAL_BASEDIR,
  OPT_XTRA_EXTRA_LSNDIR,
  OPT_XTRA_INCREMENTAL_DIR,
  OPT_XTRA_ARCHIVED_TO_LSN,
  OPT_XTRA_TABLES,
  OPT_XTRA_TABLES_FILE,
  OPT_XTRA_DATABASES,
  OPT_XTRA_DATABASES_FILE,
  OPT_XTRA_CREATE_IB_LOGFILE,
  OPT_XTRA_PARALLEL,
  OPT_XTRA_FIFO_STREAMS,
  OPT_XTRA_STREAM,
  OPT_XTRA_FIFO_DIR,
  OPT_XTRA_FIFO_TIMEOUT,
  OPT_XTRA_STRICT,
  OPT_XTRA_COMPRESS,
  OPT_XTRA_COMPRESS_THREADS,
  OPT_XTRA_COMPRESS_CHUNK_SIZE,
  OPT_XTRA_COMPRESS_ZSTD_LEVEL,
  OPT_XTRA_ENCRYPT,
  OPT_XTRA_ENCRYPT_KEY,
  OPT_XTRA_ENCRYPT_KEY_FILE,
  OPT_XTRA_ENCRYPT_THREADS,
  OPT_XTRA_ENCRYPT_CHUNK_SIZE,
  OPT_XTRA_SERVER_ID,
  OPT_LOG,
  OPT_LOG_BIN_INDEX,
  OPT_INNODB,
  OPT_INNODB_CHECKSUMS,
  OPT_INNODB_DATA_FILE_PATH,
  OPT_INNODB_DATA_HOME_DIR,
  OPT_INNODB_ADAPTIVE_HASH_INDEX,
  OPT_INNODB_FAST_SHUTDOWN,
  OPT_INNODB_FILE_PER_TABLE,
  OPT_INNODB_FLUSH_LOG_AT_TRX_COMMIT,
  OPT_INNODB_REDO_LOG_ARCHIVE_DIRS,
  OPT_INNODB_FLUSH_METHOD,
  OPT_INNODB_LOG_ARCH_DIR,
  OPT_INNODB_LOG_ARCHIVE,
  OPT_INNODB_LOG_GROUP_HOME_DIR,
  OPT_INNODB_MAX_DIRTY_PAGES_PCT,
  OPT_INNODB_MAX_PURGE_LAG,
  OPT_INNODB_ROLLBACK_ON_TIMEOUT,
  OPT_INNODB_STATUS_FILE,
  OPT_INNODB_ADDITIONAL_MEM_POOL_SIZE,
  OPT_INNODB_AUTOEXTEND_INCREMENT,
  OPT_INNODB_BUFFER_POOL_SIZE,
  OPT_INNODB_COMMIT_CONCURRENCY,
  OPT_INNODB_CONCURRENCY_TICKETS,
  OPT_INNODB_FILE_IO_THREADS,
  OPT_INNODB_IO_CAPACITY,
  OPT_INNODB_READ_IO_THREADS,
  OPT_INNODB_WRITE_IO_THREADS,
  OPT_INNODB_USE_NATIVE_AIO,
  OPT_INNODB_PAGE_SIZE,
  OPT_INNODB_LOG_BLOCK_SIZE,
  OPT_INNODB_EXTRA_UNDOSLOTS,
  OPT_INNODB_BUFFER_POOL_FILENAME,
  OPT_INNODB_FORCE_RECOVERY,
  OPT_INNODB_LOCK_WAIT_TIMEOUT,
  OPT_INNODB_LOG_BUFFER_SIZE,
  OPT_INNODB_LOG_FILE_SIZE,
  OPT_INNODB_LOG_FILES_IN_GROUP,
  OPT_INNODB_MIRRORED_LOG_GROUPS,
  OPT_INNODB_OPEN_FILES,
  OPT_INNODB_SYNC_SPIN_LOOPS,
  OPT_INNODB_THREAD_CONCURRENCY,
  OPT_INNODB_THREAD_SLEEP_DELAY,
  OPT_INNODB_REDO_LOG_ENCRYPT,
  OPT_INNODB_UNDO_LOG_ENCRYPT,
  OPT_XTRA_DEBUG_SYNC,
  OPT_XTRA_COMPACT,
  OPT_XTRA_REBUILD_INDEXES,
  OPT_XTRA_REBUILD_THREADS,
  OPT_INNODB_CHECKSUM_ALGORITHM,
  OPT_INNODB_UNDO_DIRECTORY,
  OPT_INNODB_DIRECTORIES,
  OPT_INNODB_TEMP_TABLESPACE_DIRECTORY,
  OPT_INNODB_UNDO_TABLESPACES,
  OPT_INNODB_LOG_CHECKSUMS,
  OPT_XTRA_INCREMENTAL_FORCE_SCAN,
  OPT_DEFAULTS_GROUP,
  OPT_OPEN_FILES_LIMIT,
  OPT_CLOSE_FILES,
  OPT_CORE_FILE,

  OPT_ROCKSDB_DATADIR,
  OPT_ROCKSDB_WAL_DIR,
  OPT_ROCKSDB_CHECKPOINT_MAX_AGE,
  OPT_ROCKSDB_CHECKPOINT_MAX_COUNT,

  OPT_COPY_BACK,
  OPT_MOVE_BACK,
  OPT_GALERA_INFO,
  OPT_PAGE_TRACKING,
  OPT_SLAVE_INFO,
  OPT_NO_LOCK,
  OPT_LOCK_DDL,
  OPT_LOCK_DDL_TIMEOUT,
  OPT_LOCK_DDL_PER_TABLE,
  OPT_BACKUP_LOCK_TIMEOUT,
  OPT_BACKUP_LOCK_RETRY,
  OPT_DUMP_INNODB_BUFFER,
  OPT_DUMP_INNODB_BUFFER_TIMEOUT,
  OPT_DUMP_INNODB_BUFFER_PCT,
  OPT_SAFE_SLAVE_BACKUP,
  OPT_RSYNC,
  OPT_FORCE_NON_EMPTY_DIRS,
#ifdef HAVE_VERSION_CHECK
  OPT_NO_VERSION_CHECK,
#endif
  OPT_NO_SERVER_VERSION_CHECK,
  OPT_NO_BACKUP_LOCKS,
  OPT_ROLLBACK_PREPARED_TRX,
  OPT_DECOMPRESS,
  OPT_INCREMENTAL_HISTORY_NAME,
  OPT_INCREMENTAL_HISTORY_UUID,
  OPT_DECRYPT,
  OPT_REMOVE_ORIGINAL,
  OPT_LOCK_WAIT_QUERY_TYPE,
  OPT_KILL_LONG_QUERY_TYPE,
  OPT_HISTORY,
  OPT_KILL_LONG_QUERIES_TIMEOUT,
  OPT_LOCK_WAIT_TIMEOUT,
  OPT_LOCK_WAIT_THRESHOLD,
  OPT_DEBUG_SLEEP_BEFORE_UNLOCK,
  OPT_SAFE_SLAVE_BACKUP_TIMEOUT,
  OPT_XB_SECURE_AUTH,
  OPT_TRANSITION_KEY,
  OPT_KEYRING_FILE_DATA,
  OPT_COMPONENT_KEYRING_CONFIG,
  OPT_COMPONENT_KEYRING_FILE_CONFIG,
  OPT_GENERATE_TRANSITION_KEY,
  OPT_XTRA_PLUGIN_DIR,
  OPT_XTRA_PLUGIN_LOAD,
  OPT_GENERATE_NEW_MASTER_KEY,
  OPT_REGISTER_REDO_LOG_CONSUMER,

  OPT_SSL_SSL,
  OPT_SSL_KEY,
  OPT_SSL_CERT,
  OPT_SSL_CA,
  OPT_SSL_CAPATH,
  OPT_SSL_CIPHER,
  OPT_SSL_CRL,
  OPT_SSL_CRLPATH,
  OPT_TLS_VERSION,
  OPT_SSL_MODE,
  OPT_SSL_FIPS_MODE,
  OPT_TLS_CIPHERSUITES,
  OPT_SSL_SESSION_DATA,
  OPT_SSL_SESSION_DATA_CONTINUE_ON_FAILED_REUSE,
  OPT_TLS_SNI_SERVERNAME,
  OPT_SERVER_PUBLIC_KEY,

  OPT_XTRA_TABLES_EXCLUDE,
  OPT_XTRA_DATABASES_EXCLUDE,

  OPT_XTRA_TABLES_COMPATIBILITY_CHECK,
  OPT_XTRA_CHECK_PRIVILEGES,
  OPT_XTRA_READ_BUFFER_SIZE,
};

struct my_option xb_client_options[] = {
    {"version", 'v', "print xtrabackup version information",
     (G_PTR *)&xtrabackup_version, (G_PTR *)&xtrabackup_version, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"target-dir", OPT_XTRA_TARGET_DIR, "destination directory",
     (G_PTR *)&xtrabackup_target_dir, (G_PTR *)&xtrabackup_target_dir, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"backup", OPT_XTRA_BACKUP, "take backup to target-dir",
     (G_PTR *)&xtrabackup_backup, (G_PTR *)&xtrabackup_backup, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"prepare", OPT_XTRA_PREPARE,
     "prepare a backup for starting mysql server on the backup.",
     (G_PTR *)&xtrabackup_prepare, (G_PTR *)&xtrabackup_prepare, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"export", OPT_XTRA_EXPORT,
     "create files to import to another database when prepare.",
     (G_PTR *)&xtrabackup_export, (G_PTR *)&xtrabackup_export, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"apply-log-only", OPT_XTRA_APPLY_LOG_ONLY,
     "stop recovery process not to progress LSN after applying log when "
     "prepare.",
     (G_PTR *)&xtrabackup_apply_log_only, (G_PTR *)&xtrabackup_apply_log_only,
     0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"print-param", OPT_XTRA_PRINT_PARAM,
     "print parameter of mysqld needed for copyback.",
     (G_PTR *)&xtrabackup_print_param, (G_PTR *)&xtrabackup_print_param, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"use-memory", OPT_XTRA_USE_MEMORY,
     "The value is used instead of buffer_pool_size",
     (G_PTR *)&xtrabackup_use_memory, (G_PTR *)&xtrabackup_use_memory, 0,
     GET_LL, REQUIRED_ARG, 100 * 1024 * 1024L, 1024 * 1024L, LLONG_MAX, 0,
     1024 * 1024L, 0},
    {"use-free-memory-pct", OPT_XTRA_USE_FREE_MEMORY_PCT,
     "This option specifies the percentage of free memory to be used by"
     " buffer pool at --prepare stage (default is 0% - disabled). ",
     (G_PTR *)&xtrabackup_use_free_memory_pct,
     (G_PTR *)&xtrabackup_use_free_memory_pct, 0, GET_UINT, REQUIRED_ARG, 0, 0,
     100, 0, 1, 0},
    {"estimate-memory", OPT_XTRA_ESTIMATE_MEMORY,
     "This option enable/disable the estimation of memory required to prepare "
     "the backup. The estimation happens during backup. (Default OFF)",
     (G_PTR *)&xtrabackup_estimate_memory, (G_PTR *)&xtrabackup_estimate_memory,
     0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"throttle", OPT_XTRA_THROTTLE,
     "limit count of IO operations (pairs of read&write) per second to IOS "
     "values (for '--backup')",
     (G_PTR *)&xtrabackup_throttle, (G_PTR *)&xtrabackup_throttle, 0, GET_LONG,
     REQUIRED_ARG, 0, 0, LONG_MAX, 0, 1, 0},
    {"log", OPT_LOG, "Ignored option for MySQL option compatibility",
     (G_PTR *)&log_ignored_opt, (G_PTR *)&log_ignored_opt, 0, GET_STR, OPT_ARG,
     0, 0, 0, 0, 0, 0},
    {"log-copy-interval", OPT_XTRA_LOG_COPY_INTERVAL,
     "time interval between checks done by log copying thread in milliseconds "
     "(default is 1 second).",
     (G_PTR *)&xtrabackup_log_copy_interval,
     (G_PTR *)&xtrabackup_log_copy_interval, 0, GET_LONG, REQUIRED_ARG, 1000, 0,
     LONG_MAX, 0, 1, 0},
    {"extra-lsndir", OPT_XTRA_EXTRA_LSNDIR,
     "(for --backup): save an extra copy of the xtrabackup_checkpoints file in "
     "this directory.",
     (G_PTR *)&xtrabackup_extra_lsndir, (G_PTR *)&xtrabackup_extra_lsndir, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"incremental-lsn", OPT_XTRA_INCREMENTAL,
     "(for --backup): copy only .ibd pages newer than specified LSN "
     "'high:low'. ##ATTENTION##: If a wrong LSN value is specified, it is "
     "impossible to diagnose this, causing the backup to be unusable. Be "
     "careful!",
     (G_PTR *)&xtrabackup_incremental, (G_PTR *)&xtrabackup_incremental, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"incremental-basedir", OPT_XTRA_INCREMENTAL_BASEDIR,
     "(for --backup): copy only .ibd pages newer than backup at specified "
     "directory.",
     (G_PTR *)&xtrabackup_incremental_basedir,
     (G_PTR *)&xtrabackup_incremental_basedir, 0, GET_STR, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},
    {"redo_log_arch_dir", OPT_INNODB_REDO_LOG_ARCHIVE_DIRS,
     "Set redo log archive destination directory if not already set in the "
     "server",
     (G_PTR *)&xtrabackup_redo_log_arch_dir,
     (G_PTR *)&xtrabackup_redo_log_arch_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},
    {"incremental-dir", OPT_XTRA_INCREMENTAL_DIR,
     "(for --prepare): apply .delta files and logfile in the specified "
     "directory.",
     (G_PTR *)&xtrabackup_incremental_dir, (G_PTR *)&xtrabackup_incremental_dir,
     0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"to-archived-lsn", OPT_XTRA_ARCHIVED_TO_LSN,
     "Don't apply archived logs with bigger log sequence number.",
     (G_PTR *)&xtrabackup_archived_to_lsn, (G_PTR *)&xtrabackup_archived_to_lsn,
     0, GET_LL, REQUIRED_ARG, 0, 0, LLONG_MAX, 0, 0, 0},
    {"tables", OPT_XTRA_TABLES, "filtering by regexp for table names.",
     (G_PTR *)&xtrabackup_tables, (G_PTR *)&xtrabackup_tables, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"tables_file", OPT_XTRA_TABLES_FILE,
     "filtering by list of the exact database.table name in the file.",
     (G_PTR *)&xtrabackup_tables_file, (G_PTR *)&xtrabackup_tables_file, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"databases", OPT_XTRA_DATABASES, "filtering by list of databases.",
     (G_PTR *)&xtrabackup_databases, (G_PTR *)&xtrabackup_databases, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"databases_file", OPT_XTRA_TABLES_FILE,
     "filtering by list of databases in the file.",
     (G_PTR *)&xtrabackup_databases_file, (G_PTR *)&xtrabackup_databases_file,
     0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"tables-exclude", OPT_XTRA_TABLES_EXCLUDE,
     "filtering by regexp for table names. "
     "Operates the same way as --tables, but matched names are excluded from "
     "backup. "
     "Note that this option has a higher priority than --tables.",
     (G_PTR *)&xtrabackup_tables_exclude, (G_PTR *)&xtrabackup_tables_exclude,
     0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"databases-exclude", OPT_XTRA_DATABASES_EXCLUDE,
     "Excluding databases based on name, "
     "Operates the same way as --databases, but matched names are excluded "
     "from backup. "
     "Note that this option has a higher priority than --databases.",
     (G_PTR *)&xtrabackup_databases_exclude,
     (G_PTR *)&xtrabackup_databases_exclude, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},
    {"create-ib-logfile", OPT_XTRA_CREATE_IB_LOGFILE,
     "** not work for now** creates ib_logfile* also after '--prepare'. ### If "
     "you want create ib_logfile*, only re-execute this command in same "
     "options. ###",
     (G_PTR *)&xtrabackup_create_ib_logfile,
     (G_PTR *)&xtrabackup_create_ib_logfile, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
     0},

    {"stream", OPT_XTRA_STREAM,
     "Stream all backup files using xbstream format. Files are streamed to "
     "STDOUT or FIFO files depending on --fifo-streams option. The only "
     "supported stream format is 'xbstream'.",
     (G_PTR *)&xtrabackup_stream_str, (G_PTR *)&xtrabackup_stream_str, 0,
     GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"compress", OPT_XTRA_COMPRESS,
     "Compress individual backup files using the specified compression "
     "algorithm. Supported algorithms are 'lz4' and 'zstd'. The "
     "default algorithm is 'zstd'.",
     (G_PTR *)&xtrabackup_compress_alg, (G_PTR *)&xtrabackup_compress_alg, 0,
     GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"compress-threads", OPT_XTRA_COMPRESS_THREADS,
     "Number of threads for parallel data compression. The default value is 1.",
     (G_PTR *)&xtrabackup_compress_threads,
     (G_PTR *)&xtrabackup_compress_threads, 0, GET_UINT, REQUIRED_ARG, 1, 1,
     UINT_MAX, 0, 0, 0},

    {"compress-chunk-size", OPT_XTRA_COMPRESS_CHUNK_SIZE,
     "Size of working buffer(s) for compression threads in bytes. The default "
     "value is 64K.",
     (G_PTR *)&xtrabackup_compress_chunk_size,
     (G_PTR *)&xtrabackup_compress_chunk_size, 0, GET_ULL, REQUIRED_ARG,
     (1 << 16), 1024, ULLONG_MAX, 0, 0, 0},

    {"compress-zstd-level", OPT_XTRA_COMPRESS_ZSTD_LEVEL,
     "Zstandard compression level. from 1 - 19. The default value is 1.",
     (G_PTR *)&xtrabackup_compress_zstd_level,
     (G_PTR *)&xtrabackup_compress_zstd_level, 0, GET_UINT, REQUIRED_ARG, 1, 1,
     19, 0, 0, 0},

    {"encrypt", OPT_XTRA_ENCRYPT,
     "Encrypt individual backup files using the "
     "specified encryption algorithm.",
     &xtrabackup_encrypt_algo, &xtrabackup_encrypt_algo,
     &xtrabackup_encrypt_algo_typelib, GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0,
     0},

    {"encrypt-key", OPT_XTRA_ENCRYPT_KEY, "Encryption key to use.", 0, 0, 0,
     GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"encrypt-key-file", OPT_XTRA_ENCRYPT_KEY_FILE,
     "File which contains encryption key to use.",
     (G_PTR *)&xtrabackup_encrypt_key_file,
     (G_PTR *)&xtrabackup_encrypt_key_file, 0, GET_STR_ALLOC, REQUIRED_ARG, 0,
     0, 0, 0, 0, 0},

    {"encrypt-threads", OPT_XTRA_ENCRYPT_THREADS,
     "Number of threads for parallel data encryption. The default value is 1.",
     (G_PTR *)&xtrabackup_encrypt_threads, (G_PTR *)&xtrabackup_encrypt_threads,
     0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

    {"encrypt-chunk-size", OPT_XTRA_ENCRYPT_CHUNK_SIZE,
     "Size of working buffer(S) for encryption threads in bytes. The default "
     "value is 64K.",
     (G_PTR *)&xtrabackup_encrypt_chunk_size,
     (G_PTR *)&xtrabackup_encrypt_chunk_size, 0, GET_ULL, REQUIRED_ARG,
     (1 << 16), 1024, ULLONG_MAX, 0, 0, 0},

    {"rebuild_threads", OPT_XTRA_REBUILD_THREADS,
     "Use this number of threads to rebuild indexes in a compact backup. "
     "Only has effect with --prepare and --rebuild-indexes.",
     (G_PTR *)&xtrabackup_rebuild_threads, (G_PTR *)&xtrabackup_rebuild_threads,
     0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

    {"incremental-force-scan", OPT_XTRA_INCREMENTAL_FORCE_SCAN,
     "Perform a full-scan incremental backup even if page tracking is enabled "
     "on server",
     (G_PTR *)&xtrabackup_incremental_force_scan,
     (G_PTR *)&xtrabackup_incremental_force_scan, 0, GET_BOOL, NO_ARG, 0, 0, 0,
     0, 0, 0},

    {"close_files", OPT_CLOSE_FILES,
     "do not keep files opened. Use at your own "
     "risk.",
     (G_PTR *)&xb_close_files, (G_PTR *)&xb_close_files, 0, GET_BOOL, NO_ARG, 0,
     0, 0, 0, 0, 0},

    {"core-file", OPT_CORE_FILE, "Write core on fatal signals", 0, 0, 0,
     GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},

    {"copy-back", OPT_COPY_BACK,
     "Copy all the files in a previously made "
     "backup from the backup directory to their original locations.",
     (uchar *)&xtrabackup_copy_back, (uchar *)&xtrabackup_copy_back, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

    {"move-back", OPT_MOVE_BACK,
     "Move all the files in a previously made "
     "backup from the backup directory to the actual datadir location. "
     "Use with caution, as it removes backup files.",
     (uchar *)&xtrabackup_move_back, (uchar *)&xtrabackup_move_back, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

    {"galera-info", OPT_GALERA_INFO,
     "This options creates the "
     "xtrabackup_galera_info file which contains the local node state at "
     "the time of the backup. Option should be used when performing the "
     "backup of Percona-XtraDB-Cluster. Has no effect when backup locks "
     "are used to create the backup.",
     (uchar *)&opt_galera_info, (uchar *)&opt_galera_info, 0, GET_BOOL, NO_ARG,
     0, 0, 0, 0, 0, 0},

    {"slave-info", OPT_SLAVE_INFO,
     "This option is useful when backing "
     "up a replication slave server. It prints the binary log position "
     "and name of the master server. It also writes this information to "
     "the \"xtrabackup_slave_info\" file as a \"CHANGE MASTER\" command. "
     "A new slave for this master can be set up by starting a slave server "
     "on this backup and issuing a \"CHANGE MASTER\" command with the "
     "binary log position saved in the \"xtrabackup_slave_info\" file.",
     (uchar *)&opt_slave_info, (uchar *)&opt_slave_info, 0, GET_BOOL, NO_ARG, 0,
     0, 0, 0, 0, 0},

    {"page-tracking", OPT_PAGE_TRACKING,
     "use the page-tracking server feature for incremental backup instead of "
     "the full scan of data files. With --page-tracking, xtrabackup will "
     "start page tracking on the server during backup by "
     "invoking mysqlbackup component service API. When --page-tracking is "
     "used during subsequent incremental backup, it will use the "
     "page-tracking file generated by the server to copy delta pages modified "
     "since the last backup.",
     (uchar *)&opt_page_tracking, (uchar *)&opt_page_tracking, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},

    {"no-lock", OPT_NO_LOCK,
     "Use this option to disable lock-ddl and table lock "
     "with \"FLUSH TABLES WITH READ LOCK\". Use it only if ALL your "
     "tables are InnoDB and you DO NOT CARE about the binary log "
     "position of the backup. This option shouldn't be used if there "
     "are any DDL statements being executed or if any updates are "
     "happening on non-InnoDB tables (this includes the system MyISAM "
     "tables in the mysql database), otherwise it could lead to an "
     "inconsistent backup. If you are considering to use --no-lock "
     "because your backups are failing to acquire the lock, this could "
     "be because of incoming replication events preventing the lock "
     "from succeeding. Please try using --safe-slave-backup to "
     "momentarily stop the replication slave thread, this may help "
     "the backup to succeed and you then don't need to resort to "
     "using this option.",
     (uchar *)&opt_no_lock, (uchar *)&opt_no_lock, 0, GET_BOOL, NO_ARG, 0, 0, 0,
     0, 0, 0},

    {"lock-ddl", OPT_LOCK_DDL,
     "Issue LOCK TABLES/LOCK INSTANCE FOR BACKUP if it is "
     "supported by server. Possible options are: "
     "ON - LTFB/LIFB is executed at the beginning of the backup to block all "
     "DDLs;"
     "OFF- LTFB/LIFB is not executed;"
     "REDUCED - PXB does a copy of InnoDB tables without taking "
     "any lock, while keeping track of tables affected by DDL. Before starting "
     "to copy Non-InnoDB tables, LTFB/LIFB is executed and all InnoDB tables "
     "affected by DDL are handled(either recopied or a special file is placed "
     "in backup dir to handle the DDL operation during --prepare);"
     "Default is ON.",
     (uchar *)&xtrabackup_lock_ddl_str, (uchar *)&xtrabackup_lock_ddl_str, 0,
     GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"lock-ddl-timeout", OPT_LOCK_DDL_TIMEOUT,
     "If LOCK TABLES FOR BACKUP does not return within given timeout, abort "
     "the backup.",
     (uchar *)&opt_lock_ddl_timeout, (uchar *)&opt_lock_ddl_timeout, 0,
     GET_UINT, REQUIRED_ARG, 31536000, 1, 31536000, 0, 1, 0},

    {"lock-ddl-per-table", OPT_LOCK_DDL_PER_TABLE,
     "Lock DDL for each table "
     "before xtrabackup starts the copy phase and until the backup is "
     "completed.",
     (uchar *)&opt_lock_ddl_per_table, (uchar *)&opt_lock_ddl_per_table, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

    {"backup-lock-timeout", OPT_BACKUP_LOCK_TIMEOUT,
     "Timeout in seconds for attempts to acquire metadata locks.",
     (uchar *)&opt_backup_lock_timeout, (uchar *)&opt_backup_lock_timeout, 0,
     GET_UINT, REQUIRED_ARG, 31536000, 1, 31536000, 0, 1, 0},

    {"backup-lock-retry-count", OPT_BACKUP_LOCK_RETRY,
     "Number of attempts to acquire metadata locks.",
     (uchar *)&opt_backup_lock_retry_count,
     (uchar *)&opt_backup_lock_retry_count, 0, GET_UINT, REQUIRED_ARG, 0, 0,
     UINT_MAX, 0, 1, 0},

    {"dump-innodb-buffer-pool", OPT_DUMP_INNODB_BUFFER,
     "Instruct MySQL server to dump innodb buffer pool by issuing a "
     "SET GLOBAL innodb_buffer_pool_dump_now=ON ",
     (uchar *)&opt_dump_innodb_buffer_pool,
     (uchar *)&opt_dump_innodb_buffer_pool, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
     0},

    {"dump-innodb-buffer-pool-timeout", OPT_DUMP_INNODB_BUFFER_TIMEOUT,
     "This option specifies the number of seconds xtrabackup waits "
     "for innodb buffer pool dump to complete",
     (uchar *)&opt_dump_innodb_buffer_pool_timeout,
     (uchar *)&opt_dump_innodb_buffer_pool_timeout, 0, GET_UINT, REQUIRED_ARG,
     10, 0, 0, 0, 0, 0},

    {"dump-innodb-buffer-pool-pct", OPT_DUMP_INNODB_BUFFER_PCT,
     "This option specifies the percentage of buffer pool "
     "to be dumped ",
     (uchar *)&opt_dump_innodb_buffer_pool_pct,
     (uchar *)&opt_dump_innodb_buffer_pool_pct, 0, GET_UINT, REQUIRED_ARG, 0, 0,
     100, 0, 1, 0},

    {"safe-slave-backup", OPT_SAFE_SLAVE_BACKUP,
     "This option stops slave SQL thread at the start of the backup if "
     "lock-ddl=ON or before copying non-InnoDB tables if lock-ddl=OFF and "
     "waits until Slave_open_temp_tables in \"SHOW STATUS\" is zero. If there "
     "are no open temporary tables, "
     "the backup will take place, otherwise the SQL thread will be "
     "started and stopped until there are no open temporary tables. "
     "The backup will fail if Slave_open_temp_tables does not become "
     "zero after --safe-slave-backup-timeout seconds. The slave SQL "
     "thread will be restarted when the backup finishes.",
     (uchar *)&opt_safe_slave_backup, (uchar *)&opt_safe_slave_backup, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

    {"rsync", OPT_RSYNC,
     "Uses the rsync utility to optimize local file "
     "transfers. When this option is specified, innobackupex uses rsync "
     "to copy all non-InnoDB files instead of spawning a separate cp for "
     "each file, which can be much faster for servers with a large number "
     "of databases or tables.  This option cannot be used together with "
     "--stream.",
     (uchar *)&opt_rsync, (uchar *)&opt_rsync, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
     0, 0},

    {"force-non-empty-directories", OPT_FORCE_NON_EMPTY_DIRS,
     "This "
     "option, when specified, makes --copy-back or --move-back transfer "
     "files to non-empty directories. Note that no existing files will be "
     "overwritten. If --copy-back or --nove-back has to copy a file from "
     "the backup directory which already exists in the destination "
     "directory, it will still fail with an error.",
     (uchar *)&opt_force_non_empty_dirs, (uchar *)&opt_force_non_empty_dirs, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

    {"no-server-version-check", OPT_NO_SERVER_VERSION_CHECK,
     "This option allows backup to proceed when the server version is greater "
     "(newer) than the PXB supported version.",
     (uchar *)&opt_no_server_version_check,
     (uchar *)&opt_no_server_version_check, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
     0},

#ifdef HAVE_VERSION_CHECK
    {"no-version-check", OPT_NO_VERSION_CHECK,
     "This option disables the "
     "version check which is enabled by the --version-check option.",
     (uchar *)&opt_noversioncheck, (uchar *)&opt_noversioncheck, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
#endif

    {"tables-compatibility-check", OPT_XTRA_TABLES_COMPATIBILITY_CHECK,
     "This option enables engine compatibility warning.",
     (uchar *)&opt_tables_compatibility_check,
     (uchar *)&opt_tables_compatibility_check, 0, GET_BOOL, NO_ARG, true, 0, 0,
     0, 0, 0},

    {"no-backup-locks", OPT_NO_BACKUP_LOCKS,
     "This option controls if "
     "backup locks should be used instead of FLUSH TABLES WITH READ LOCK "
     "on the backup stage. It will disable lock-ddl. The option has no effect "
     "when backup locks are "
     "not supported by the server. This option is disabled by default, "
     "disable with --no-backup-locks.",
     (uchar *)&opt_no_backup_locks, (uchar *)&opt_no_backup_locks, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},

    {"rollback-prepared-trx", OPT_ROLLBACK_PREPARED_TRX,
     "Force rollback prepared InnoDB transactions.",
     (uchar *)&srv_rollback_prepared_trx, (uchar *)&srv_rollback_prepared_trx,
     0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

    {"decompress", OPT_DECOMPRESS,
     "Decompresses all the compressed files made with the --compress option.",
     (uchar *)&opt_decompress, (uchar *)&opt_decompress, 0, GET_BOOL, NO_ARG, 0,
     0, 0, 0, 0, 0},

    {"user", 'u',
     "This option specifies the MySQL username used "
     "when connecting to the server, if that's not the current user. "
     "The option accepts a string argument. See mysql --help for details.",
     (uchar *)&opt_user, (uchar *)&opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},

    {"host", 'H',
     "This option specifies the host to use when "
     "connecting to the database server with TCP/IP.  The option accepts "
     "a string argument. See mysql --help for details.",
     (uchar *)&opt_host, (uchar *)&opt_host, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},

    {"port", 'P',
     "This option specifies the port to use when "
     "connecting to the database server with TCP/IP.  The option accepts "
     "a string argument. See mysql --help for details.",
     &opt_port, &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"password", 'p',
     "This option specifies the password to use "
     "when connecting to the database. It accepts a string argument.  "
     "See mysql --help for details.",
     0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"socket", 'S',
     "This option specifies the socket to use when "
     "connecting to the local database server with a UNIX domain socket.  "
     "The option accepts a string argument. See mysql --help for details.",
     (uchar *)&opt_socket, (uchar *)&opt_socket, 0, GET_STR, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},

    {"incremental-history-name", OPT_INCREMENTAL_HISTORY_NAME,
     "This option specifies the name of the backup series stored in the "
     "PERCONA_SCHEMA.xtrabackup_history history record to base an "
     "incremental backup on. Xtrabackup will search the history table "
     "looking for the most recent (highest innodb_to_lsn), successful "
     "backup in the series and take the to_lsn value to use as the "
     "starting lsn for the incremental backup. This will be mutually "
     "exclusive with --incremental-history-uuid, --incremental-basedir "
     "and --incremental-lsn. If no valid lsn can be found (no series by "
     "that name, no successful backups by that name) xtrabackup will "
     "return with an error. It is used with the --incremental option.",
     (uchar *)&opt_incremental_history_name,
     (uchar *)&opt_incremental_history_name, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},

    {"incremental-history-uuid", OPT_INCREMENTAL_HISTORY_UUID,
     "This option specifies the UUID of the specific history record "
     "stored in the PERCONA_SCHEMA.xtrabackup_history to base an "
     "incremental backup on. --incremental-history-name, "
     "--incremental-basedir and --incremental-lsn. If no valid lsn can be "
     "found (no success record with that uuid) xtrabackup will return "
     "with an error. It is used with the --incremental option.",
     (uchar *)&opt_incremental_history_uuid,
     (uchar *)&opt_incremental_history_uuid, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},

    {"decrypt", OPT_DECRYPT,
     "Decrypts all files with the .xbcrypt "
     "extension in a backup previously made with --encrypt option.",
     &opt_decrypt_algo, &opt_decrypt_algo, &xtrabackup_encrypt_algo_typelib,
     GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"remove-original", OPT_REMOVE_ORIGINAL,
     "Remove .qp, .zst, .lz4 and .xbcrypt files "
     "after decryption and decompression.",
     (uchar *)&opt_remove_original, (uchar *)&opt_remove_original, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},

    {"ftwrl-wait-query-type", OPT_LOCK_WAIT_QUERY_TYPE,
     "This option specifies which types of queries are allowed to complete "
     "before innobackupex will issue the global lock. Default is all.",
     (uchar *)&opt_lock_wait_query_type, (uchar *)&opt_lock_wait_query_type,
     &query_type_typelib, GET_ENUM, REQUIRED_ARG, QUERY_TYPE_ALL, 0, 0, 0, 0,
     0},

    {"kill-long-query-type", OPT_KILL_LONG_QUERY_TYPE,
     "This option specifies which types of queries should be killed to "
     "unblock the global lock. Default is \"SELECT\".",
     (uchar *)&opt_kill_long_query_type, (uchar *)&opt_kill_long_query_type,
     &query_type_typelib, GET_ENUM, REQUIRED_ARG, QUERY_TYPE_SELECT, 0, 0, 0, 0,
     0},

    {"history", OPT_HISTORY,
     "This option enables the tracking of backup history in the "
     "PERCONA_SCHEMA.xtrabackup_history table. An optional history "
     "series name may be specified that will be placed with the history "
     "record for the current backup being taken.",
     NULL, NULL, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"kill-long-queries-timeout", OPT_KILL_LONG_QUERIES_TIMEOUT,
     "This option specifies the number of seconds innobackupex waits "
     "between starting FLUSH TABLES WITH READ LOCK and killing those "
     "queries that block it. Default is 0 seconds, which means "
     "innobackupex will not attempt to kill any queries.",
     (uchar *)&opt_kill_long_queries_timeout,
     (uchar *)&opt_kill_long_queries_timeout, 0, GET_UINT, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},

    {"ftwrl-wait-timeout", OPT_LOCK_WAIT_TIMEOUT,
     "This option specifies time in seconds that innobackupex should wait "
     "for queries that would block FTWRL before running it. If there are "
     "still such queries when the timeout expires, innobackupex terminates "
     "with an error. Default is 0, in which case innobackupex does not "
     "wait for queries to complete and starts FTWRL immediately.",
     (uchar *)&opt_lock_wait_timeout, (uchar *)&opt_lock_wait_timeout, 0,
     GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"ftwrl-wait-threshold", OPT_LOCK_WAIT_THRESHOLD,
     "This option specifies the query run time threshold which is used by "
     "innobackupex to detect long-running queries with a non-zero value "
     "of --ftwrl-wait-timeout. FTWRL is not started until such "
     "long-running queries exist. This option has no effect if "
     "--ftwrl-wait-timeout is 0. Default value is 60 seconds.",
     (uchar *)&opt_lock_wait_threshold, (uchar *)&opt_lock_wait_threshold, 0,
     GET_UINT, REQUIRED_ARG, 60, 0, 0, 0, 0, 0},

    {"debug-sleep-before-unlock", OPT_DEBUG_SLEEP_BEFORE_UNLOCK,
     "This is a debug-only option used by the XtraBackup test suite.",
     (uchar *)&opt_debug_sleep_before_unlock,
     (uchar *)&opt_debug_sleep_before_unlock, 0, GET_UINT, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},

    {"safe-slave-backup-timeout", OPT_SAFE_SLAVE_BACKUP_TIMEOUT,
     "How many seconds --safe-slave-backup should wait for "
     "Slave_open_temp_tables to become zero. (default 300)",
     (uchar *)&opt_safe_slave_backup_timeout,
     (uchar *)&opt_safe_slave_backup_timeout, 0, GET_UINT, REQUIRED_ARG, 300, 0,
     0, 0, 0, 0},

    {"check-privileges", OPT_XTRA_CHECK_PRIVILEGES,
     "Check database user "
     "privileges before performing any query.",
     &opt_check_privileges, &opt_check_privileges, 0, GET_BOOL, NO_ARG, 0, 0, 0,
     0, 0, 0},

    {"read_buffer_size", OPT_XTRA_READ_BUFFER_SIZE,
     "Set datafile read buffer size, given value is scaled up to page size."
     " Default is 10Mb.",
     &opt_read_buffer_size, &opt_read_buffer_size, 0, GET_UINT, OPT_ARG,
     10 * 1024 * 1024, 2 * UNIV_PAGE_SIZE_MAX, UINT_MAX, 0, UNIV_PAGE_SIZE_MAX,
     0},

#include "caching_sha2_passwordopt-longopts.h"
#include "sslopt-longopts.h"

#if !defined(HAVE_YASSL)
    {"server-public-key-path", OPT_SERVER_PUBLIC_KEY,
     "File path to the server public RSA key in PEM format.",
     &opt_server_public_key, &opt_server_public_key, 0, GET_STR, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},
#endif

    {"transition-key", OPT_TRANSITION_KEY,
     "Transition key to encrypt tablespace keys with.", 0, 0, 0, GET_STR,
     OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"xtrabackup-plugin-dir", OPT_XTRA_PLUGIN_DIR,
     "Directory for xtrabackup plugins.", &opt_xtra_plugin_dir,
     &opt_xtra_plugin_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"plugin-load", OPT_XTRA_PLUGIN_LOAD, "List of plugins to load.",
     &opt_xtra_plugin_load, &opt_xtra_plugin_load, 0, GET_STR, REQUIRED_ARG, 0,
     0, 0, 0, 0, 0},

    {"generate-new-master-key", OPT_GENERATE_NEW_MASTER_KEY,
     "Generate new master key when doing copy-back.",
     &opt_generate_new_master_key, &opt_generate_new_master_key, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},

    {"generate-transition-key", OPT_GENERATE_TRANSITION_KEY,
     "Generate transition key and store it into keyring.",
     &opt_generate_transition_key, &opt_generate_transition_key, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},

    {"keyring-file-data", OPT_KEYRING_FILE_DATA, "Path to keyring file.",
     &opt_keyring_file_data, &opt_keyring_file_data, 0, GET_STR, OPT_ARG, 0, 0,
     0, 0, 0, 0},

    {"component-keyring-config", OPT_COMPONENT_KEYRING_CONFIG,
     "Path to load component config. Used for --prepare, --move-back,"
     " --copy-back.",
     &opt_component_keyring_config, &opt_component_keyring_config, 0, GET_STR,
     OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"component-keyring-file-config", OPT_COMPONENT_KEYRING_FILE_CONFIG,
     "Path to load keyring component config. Used for --prepare, --move-back,"
     " --copy-back. (Deprecated, please use "
     "--component-keyring-config instead)",
     &opt_component_keyring_file_config, &opt_component_keyring_file_config, 0,
     GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"parallel", OPT_XTRA_PARALLEL,
     "Number of threads to use for parallel datafiles transfer. "
     "The default value is 1.",
     (G_PTR *)&xtrabackup_parallel, (G_PTR *)&xtrabackup_parallel, 0, GET_INT,
     REQUIRED_ARG, 1, 1, INT_MAX, 0, 0, 0},

    {"fifo-streams", OPT_XTRA_FIFO_STREAMS,
     "Number of FIFO files to use for parallel datafiles stream. Setting this "
     "parameter to 1 disables FIFO and stream is sent to STDOUT.",
     (G_PTR *)&xtrabackup_fifo_streams, (G_PTR *)&xtrabackup_fifo_streams, 0,
     GET_INT, REQUIRED_ARG, 1, 1, INT_MAX, 0, 0, 0},

    {"fifo-dir", OPT_XTRA_FIFO_DIR,
     "Directory to write Named Pipe. If ommited, we use --target-dir",
     &xtrabackup_fifo_dir, &xtrabackup_fifo_dir, 0, GET_STR_ALLOC, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},

    {"fifo-timeout", OPT_XTRA_FIFO_TIMEOUT,
     "How many seconds to wait for other end to open the fifo stream for "
     "reading. Default 60 seconds",
     (G_PTR *)&xtrabackup_fifo_timeout, (G_PTR *)&xtrabackup_fifo_timeout, 0,
     GET_INT, REQUIRED_ARG, 60, 1, INT_MAX, 0, 0, 0},

    {"strict", OPT_XTRA_STRICT,
     "Fail with error when invalid arguments were passed to the xtrabackup.",
     (uchar *)&opt_strict, (uchar *)&opt_strict, 0, GET_BOOL, NO_ARG, 1, 0, 0,
     0, 0, 0},

    {"rocksdb-checkpoint-max-age", OPT_ROCKSDB_CHECKPOINT_MAX_AGE,
     "Maximum ROCKSB checkpoint age in seconds.",
     &opt_rocksdb_checkpoint_max_age, &opt_rocksdb_checkpoint_max_age, 0,
     GET_INT, REQUIRED_ARG, 0, 0, INT_MAX, 0, 0, 0},

    {"rocksdb-checkpoint-max-count", OPT_ROCKSDB_CHECKPOINT_MAX_COUNT,
     "Maximum count of ROCKSB checkpoints.", &opt_rocksdb_checkpoint_max_count,
     &opt_rocksdb_checkpoint_max_count, 0, GET_INT, REQUIRED_ARG, 0, 0, INT_MAX,
     0, 0, 0},

    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

uint xb_client_options_count = array_elements(xb_client_options);

struct my_option xb_server_options[] = {
    {"datadir", 'h', "Path to the database root.", (G_PTR *)&mysql_data_home,
     (G_PTR *)&mysql_data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"tmpdir", 't',
     "Path for temporary files. Several paths may be specified, separated by a "
#if defined(__WIN__) || defined(OS2) || defined(__NETWARE__)
     "semicolon (;)"
#else
     "colon (:)"
#endif
     ", in this case they are used in a round-robin fashion.",
     (G_PTR *)&opt_mysql_tmpdir, (G_PTR *)&opt_mysql_tmpdir, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"log", OPT_LOG, "Ignored option for MySQL option compatibility",
     (G_PTR *)&log_ignored_opt, (G_PTR *)&log_ignored_opt, 0, GET_STR, OPT_ARG,
     0, 0, 0, 0, 0, 0},

    {"log_bin", OPT_LOG, "Base name for the log sequence", &opt_log_bin,
     &opt_log_bin, 0, GET_STR_ALLOC, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"log-bin-index", OPT_LOG_BIN_INDEX,
     "File that holds the names for binary log files.", &opt_binlog_index_name,
     &opt_binlog_index_name, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"innodb", OPT_INNODB, "Ignored option for MySQL option compatibility",
     (G_PTR *)&innobase_ignored_opt, (G_PTR *)&innobase_ignored_opt, 0, GET_STR,
     OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"innodb_adaptive_hash_index", OPT_INNODB_ADAPTIVE_HASH_INDEX,
     "Enable InnoDB adaptive hash index (enabled by default).  "
     "Disable with --skip-innodb-adaptive-hash-index.",
     (G_PTR *)&innobase_adaptive_hash_index,
     (G_PTR *)&innobase_adaptive_hash_index, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0,
     0},
    {"innodb_autoextend_increment", OPT_INNODB_AUTOEXTEND_INCREMENT,
     "Data file autoextend increment in megabytes",
     (G_PTR *)&sys_tablespace_auto_extend_increment,
     (G_PTR *)&sys_tablespace_auto_extend_increment, 0, GET_ULONG, REQUIRED_ARG,
     8L, 1L, 1000L, 0, 1L, 0},
    {"innodb_buffer_pool_size", OPT_INNODB_BUFFER_POOL_SIZE,
     "The size of the memory buffer InnoDB uses to cache data and indexes of "
     "its tables.",
     (G_PTR *)&innobase_buffer_pool_size, (G_PTR *)&innobase_buffer_pool_size,
     0, GET_LL, REQUIRED_ARG, 8 * 1024 * 1024L, 1024 * 1024L, LLONG_MAX, 0,
     1024 * 1024L, 0},
    {"innodb_checksums", OPT_INNODB_CHECKSUMS,
     "Enable InnoDB checksums validation (enabled by default). \
Disable with --skip-innodb-checksums.",
     (G_PTR *)&innobase_use_checksums, (G_PTR *)&innobase_use_checksums, 0,
     GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
    {"innodb_data_file_path", OPT_INNODB_DATA_FILE_PATH,
     "Path to individual files and their sizes.", &innobase_data_file_path,
     &innobase_data_file_path, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0,
     0},
    {"innodb_data_home_dir", OPT_INNODB_DATA_HOME_DIR,
     "The common part for InnoDB table spaces.", &innobase_data_home_dir,
     &innobase_data_home_dir, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"innodb_io_capacity", OPT_INNODB_IO_CAPACITY,
     "Number of IOPs the server can do. Tunes the background IO rate",
     (G_PTR *)&srv_io_capacity, (G_PTR *)&srv_io_capacity, 0, GET_ULONG,
     OPT_ARG, 200, 100, ~0UL, 0, 0, 0},
    {"innodb_read_io_threads", OPT_INNODB_READ_IO_THREADS,
     "Number of background read I/O threads in InnoDB.",
     (G_PTR *)&innobase_read_io_threads, (G_PTR *)&innobase_read_io_threads, 0,
     GET_LONG, REQUIRED_ARG, 4, 4, 64, 0, 1, 0},
    {"innodb_write_io_threads", OPT_INNODB_WRITE_IO_THREADS,
     "Number of background write I/O threads in InnoDB.",
     (G_PTR *)&innobase_write_io_threads, (G_PTR *)&innobase_write_io_threads,
     0, GET_LONG, REQUIRED_ARG, 4, 4, 64, 0, 1, 0},
    {"innodb_file_per_table", OPT_INNODB_FILE_PER_TABLE,
     "Stores each InnoDB table to an .ibd file in the database dir.",
     (G_PTR *)&innobase_file_per_table, (G_PTR *)&innobase_file_per_table, 0,
     GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0},
    {"innodb_flush_log_at_trx_commit", OPT_INNODB_FLUSH_LOG_AT_TRX_COMMIT,
     "Set to 0 (write and flush once per second), 1 (write and flush at each "
     "commit) or 2 (write at commit, flush once per second).",
     (G_PTR *)&srv_flush_log_at_trx_commit,
     (G_PTR *)&srv_flush_log_at_trx_commit, 0, GET_ULONG, OPT_ARG, 1, 0, 2, 0,
     0, 0},
    {"innodb_flush_method", OPT_INNODB_FLUSH_METHOD,
     "With which method to flush data.", &innodb_flush_method,
     &innodb_flush_method, &innodb_flush_method_typelib, GET_ENUM, REQUIRED_ARG,
     ISO_REPEATABLE_READ, 0, 0, 0, 0, 0},

    /* ####### Should we use this option? ####### */
    {"innodb_force_recovery", OPT_INNODB_FORCE_RECOVERY,
     "Helps to save your data in case the disk image of the database becomes "
     "corrupt.",
     (G_PTR *)&innobase_force_recovery, (G_PTR *)&innobase_force_recovery, 0,
     GET_LONG, REQUIRED_ARG, 0, 0, 6, 0, 1, 0},

    {"innodb_log_buffer_size", OPT_INNODB_LOG_BUFFER_SIZE,
     "The size of the buffer which InnoDB uses to write log to the log files "
     "on disk.",
     (G_PTR *)&innobase_log_buffer_size, (G_PTR *)&innobase_log_buffer_size, 0,
     GET_LONG, REQUIRED_ARG, 16 * 1024 * 1024L, 256 * 1024L, LONG_MAX, 0, 1024,
     0},
    {"innodb_log_file_size", OPT_INNODB_LOG_FILE_SIZE,
     "Size of each log file in a log group.", (G_PTR *)&innobase_log_file_size,
     (G_PTR *)&innobase_log_file_size, 0, GET_LL, REQUIRED_ARG,
     48 * 1024 * 1024L, 1 * 1024 * 1024L, LLONG_MAX, 0, 1024 * 1024L, 0},
    {"innodb_log_files_in_group", OPT_INNODB_LOG_FILES_IN_GROUP,
     "Number of log files in the log group. InnoDB writes to the files in a "
     "circular fashion. Value 3 is recommended here.",
     &innobase_log_files_in_group, &innobase_log_files_in_group, 0, GET_LONG,
     REQUIRED_ARG, 2, 2, 100, 0, 1, 0},
    {"innodb_log_group_home_dir", OPT_INNODB_LOG_GROUP_HOME_DIR,
     "Path to InnoDB log files.", &srv_log_group_home_dir,
     &srv_log_group_home_dir, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"innodb_max_dirty_pages_pct", OPT_INNODB_MAX_DIRTY_PAGES_PCT,
     "Percentage of dirty pages allowed in bufferpool.",
     (G_PTR *)&srv_max_buf_pool_modified_pct,
     (G_PTR *)&srv_max_buf_pool_modified_pct, 0, GET_DOUBLE, REQUIRED_ARG,
     (longlong)getopt_double2ulonglong(75),
     (longlong)getopt_double2ulonglong(0), getopt_double2ulonglong(100), 0, 0,
     0},
    {"innodb_open_files", OPT_INNODB_OPEN_FILES,
     "How many files at the maximum InnoDB keeps open at the same time.",
     (G_PTR *)&innobase_open_files, (G_PTR *)&innobase_open_files, 0, GET_LONG,
     REQUIRED_ARG, 300L, 10L, LONG_MAX, 0, 1L, 0},
    {"innodb_use_native_aio", OPT_INNODB_USE_NATIVE_AIO,
     "Use native AIO if supported on this platform.",
     (G_PTR *)&srv_use_native_aio, (G_PTR *)&srv_use_native_aio, 0, GET_BOOL,
     NO_ARG, false, 0, 0, 0, 0, 0},
    {"innodb_page_size", OPT_INNODB_PAGE_SIZE,
     "The universal page size of the database.", (G_PTR *)&innobase_page_size,
     (G_PTR *)&innobase_page_size, 0,
     /* Use GET_LL to support numeric suffixes in 5.6 */
     GET_LL, REQUIRED_ARG, (1LL << 14), (1LL << 12),
     (1LL << UNIV_PAGE_SIZE_SHIFT_MAX), 0, 1L, 0},
    {"innodb_log_block_size", OPT_INNODB_LOG_BLOCK_SIZE,
     "The log block size of the transaction log file. "
     "Changing for created log file is not supported. Use on your own risk!",
     (G_PTR *)&innobase_log_block_size, (G_PTR *)&innobase_log_block_size, 0,
     GET_ULONG, REQUIRED_ARG, 512, 512, 1 << UNIV_PAGE_SIZE_SHIFT_MAX, 0, 1L,
     0},
    {"innodb_buffer_pool_filename", OPT_INNODB_BUFFER_POOL_FILENAME,
     "Filename to/from which to dump/load the InnoDB buffer pool",
     (G_PTR *)&innobase_buffer_pool_filename,
     (G_PTR *)&innobase_buffer_pool_filename, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},

#ifndef __WIN__
    {"debug-sync", OPT_XTRA_DEBUG_SYNC,
     "Debug sync point. This is only used by the xtrabackup test suite",
     (G_PTR *)&xtrabackup_debug_sync, (G_PTR *)&xtrabackup_debug_sync, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif

#ifndef NDEBUG
    {"debug", '#',
     "Output debug log. See " REFMAN "dbug-package.html"
     " Default all ib_log output to stderr. To redirect all ib_log output"
     " to separate file, use --debug=d,ib_log:o,/tmp/xtrabackup.trace",
     &dbug_setting, &dbug_setting, nullptr, GET_STR, OPT_ARG, 0, 0, 0, nullptr,
     0, nullptr},
#endif /* !NDEBUG */
    {"innodb_checksum_algorithm", OPT_INNODB_CHECKSUM_ALGORITHM,
     "The algorithm InnoDB uses for page checksumming. [CRC32, STRICT_CRC32, "
     "INNODB, STRICT_INNODB, NONE, STRICT_NONE]",
     &srv_checksum_algorithm, &srv_checksum_algorithm,
     &innodb_checksum_algorithm_typelib, GET_ENUM, REQUIRED_ARG,
     SRV_CHECKSUM_ALGORITHM_CRC32, 0, 0, 0, 0, 0},
    {"innodb_log_checksums", OPT_INNODB_LOG_CHECKSUMS,
     "Whether to compute and require checksums for InnoDB redo log blocks",
     &srv_log_checksums, &srv_log_checksums, &innodb_checksum_algorithm_typelib,
     GET_BOOL, REQUIRED_ARG, true, 0, 0, 0, 0, 0},
    {"innodb_undo_directory", OPT_INNODB_UNDO_DIRECTORY,
     "Directory where undo tablespace files live, this path can be absolute.",
     &srv_undo_dir, &srv_undo_dir, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0,
     0, 0},
    {"innodb_directories", OPT_INNODB_DIRECTORIES,
     "List of directories 'dir1;dir2;..;dirN' to scan for tablespace files. "
     "Default is to scan 'innodb-data-home-dir;innodb-undo-directory;datadir'",
     &srv_innodb_directories, &srv_innodb_directories, 0, GET_STR_ALLOC,

     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"temp_tablespaces_dir", OPT_INNODB_TEMP_TABLESPACE_DIRECTORY,
     "Directory where temp tablespace files live, this path can be absolute.",
     &srv_temp_dir, &srv_temp_dir, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0,
     0, 0},

    {"innodb_undo_tablespaces", OPT_INNODB_UNDO_TABLESPACES,
     "Number of undo tablespaces to use.", (G_PTR *)&srv_undo_tablespaces,
     (G_PTR *)&srv_undo_tablespaces, 0, GET_ULONG, REQUIRED_ARG,
     FSP_IMPLICIT_UNDO_TABLESPACES, FSP_MIN_UNDO_TABLESPACES,
     FSP_MAX_UNDO_TABLESPACES, 0, 1, 0},

    {"innodb_redo_log_encrypt", OPT_INNODB_REDO_LOG_ENCRYPT,
     "Enable or disable Encryption of REDO tablespace.", &srv_redo_log_encrypt,
     &srv_redo_log_encrypt, 0, GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0},

    {"innodb_undo_log_encrypt", OPT_INNODB_UNDO_LOG_ENCRYPT,
     "Enable or disable Encrypt of UNDO tablespace.", &srv_undo_log_encrypt,
     &srv_undo_log_encrypt, 0, GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0},

    {"defaults_group", OPT_DEFAULTS_GROUP,
     "defaults group in config file (default \"mysqld\").",
     (G_PTR *)&defaults_group, (G_PTR *)&defaults_group, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"open_files_limit", OPT_OPEN_FILES_LIMIT,
     "the maximum number of file "
     "descriptors to reserve with setrlimit().",
     (G_PTR *)&xb_open_files_limit, (G_PTR *)&xb_open_files_limit, 0, GET_ULONG,
     REQUIRED_ARG, 0, 0, UINT_MAX, 0, 1, 0},

    {"server-id", OPT_XTRA_SERVER_ID, "The server instance being backed up",
     &server_id, &server_id, 0, GET_UINT, REQUIRED_ARG, 0, 0, UINT_MAX32, 0, 0,
     0},

    {"rocksdb_datadir", OPT_ROCKSDB_DATADIR, "RocksDB data directory",
     &opt_rocksdb_datadir, &opt_rocksdb_datadir, 0, GET_STR_ALLOC, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},

    {"rocksdb_wal_dir", OPT_ROCKSDB_WAL_DIR, "RocksDB WAL directory",
     &opt_rocksdb_wal_dir, &opt_rocksdb_wal_dir, 0, GET_STR_ALLOC, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},

    {"register_redo_log_consumer", OPT_REGISTER_REDO_LOG_CONSUMER,
     "Register a redo log consumer in the start of the backup. If this option "
     "is enabled, it will block the server from purging redo log if PXB redo "
     "follow thread is still copying it and will stall DMLs on the server.",
     &xtrabackup_register_redo_log_consumer,
     &xtrabackup_register_redo_log_consumer, 0, GET_BOOL, NO_ARG, false, 0, 0,
     0, 0, 0},

    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

uint xb_server_options_count = array_elements(xb_server_options);

/* Following definitions are to avoid linking with unused datasinks
   and their link dependencies */
datasink_t datasink_decrypt;
datasink_t datasink_decompress;
datasink_t datasink_decompress_lz4;
datasink_t datasink_decompress_zstd;

#ifndef __WIN__
static int debug_sync_resumed;

static void sigcont_handler(int sig);

static void sigcont_handler(int sig __attribute__((unused))) {
  debug_sync_resumed = 1;
}
#endif

void debug_sync_point(const char *name) {
#ifndef __WIN__
  FILE *fp;
  pid_t pid;
  char pid_path[FN_REFLEN];

  if (xtrabackup_debug_sync == NULL) {
    return;
  }

  if (strcmp(xtrabackup_debug_sync, name)) {
    return;
  }

  pid = getpid();

  snprintf(pid_path, sizeof(pid_path), "%s/xtrabackup_debug_sync",
           xtrabackup_target_dir);
  fp = fopen(pid_path, "w");
  if (fp == NULL) {
    xb::error() << "cannot open " << pid_path;
    exit(EXIT_FAILURE);
  }
  fprintf(fp, "%u\n", (uint)pid);
  fclose(fp);

  xb::info() << "DEBUG: Suspending at debug sync point " << SQUOTE(name)
             << "Resume with 'kill -SIGCONT " << pid << "'";

  debug_sync_resumed = 0;
  kill(pid, SIGSTOP);
  while (!debug_sync_resumed) {
    sleep(1);
  }

  /* On resume */
  xb::info() << "DEBUG: removing the pid file";
  my_delete(pid_path, MYF(MY_WME));
#endif
}

static const char *xb_client_default_groups[] = {"xtrabackup", "client", 0, 0,
                                                 0};

static const char *xb_server_default_groups[] = {"xtrabackup", "mysqld", 0, 0,
                                                 0};

static void print_version(void) {
  fprintf(stderr,
          "%s version %s based on MySQL server %s %s (%s) (revision id: %s)\n",
          my_progname, XTRABACKUP_VERSION, MYSQL_SERVER_VERSION, SYSTEM_TYPE,
          MACHINE_TYPE, XTRABACKUP_REVISION);
}

static void usage(void) {
  puts(
      "Open source backup tool for InnoDB and XtraDB\n\
\n\
Copyright (C) 2009-2019 Percona LLC and/or its affiliates.\n\
Portions Copyright (C) 2000, 2011, MySQL AB & Innobase Oy. All Rights Reserved.\n\
\n\
This program is free software; you can redistribute it and/or\n\
modify it under the terms of the GNU General Public License\n\
as published by the Free Software Foundation version 2\n\
of the License.\n\
\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\
\n\
You can download full text of the license on http://www.gnu.org/licenses/gpl-2.0.txt\n");

  printf(
      "Usage: [%s [--defaults-file=#] --backup | %s [--defaults-file=#] "
      "--prepare] [OPTIONS]\n",
      my_progname, my_progname);
  print_defaults("my", xb_server_default_groups);
  my_print_help(xb_client_options);
  my_print_help(xb_server_options);
  my_print_variables(xb_server_options);
  my_print_variables(xb_client_options);
}

#define ADD_PRINT_PARAM_OPT(value) \
  { print_param_str << opt->name << "=" << value << "\n"; }

/************************************************************************
Check if parameter is set in defaults file or via command line argument
@return true if parameter is set. */
bool check_if_param_set(const char *param) {
  return param_set.find(param) != param_set.end();
}

bool xb_get_one_option(int optid, const struct my_option *opt, char *argument) {
  static const char *hide_value[] = {"password", "encrypt-key",
                                     "transition-key"};

  param_str << "--" << opt->name;
  if (argument) {
    bool param_handled = false;
    for (unsigned i = 0; i < sizeof(hide_value) / sizeof(char *); ++i) {
      if (strcmp(opt->name, hide_value[i]) == 0) {
        param_handled = true;
        param_str << "=*";
        break;
      }
    }
    if (!param_handled) {
      param_str << "=" << argument;
    }
  }
  param_str << " ";
  param_set.insert(opt->name);
  switch (optid) {
    case '#':
      dbug_setting = argument ? argument : "d,ib_log";
      DBUG_SET_INITIAL(dbug_setting);
      break;

    case 'h':
      strmake(mysql_real_data_home, argument, FN_REFLEN - 1);
      mysql_data_home = mysql_real_data_home;

      ADD_PRINT_PARAM_OPT(mysql_real_data_home);
      break;

    case 't':

      ADD_PRINT_PARAM_OPT(opt_mysql_tmpdir);
      break;

    case OPT_INNODB_DATA_HOME_DIR:

      ADD_PRINT_PARAM_OPT(innobase_data_home_dir);
      break;

    case OPT_INNODB_DATA_FILE_PATH:

      ADD_PRINT_PARAM_OPT(innobase_data_file_path);
      break;

    case OPT_INNODB_LOG_GROUP_HOME_DIR:

      ADD_PRINT_PARAM_OPT(srv_log_group_home_dir);
      break;

    case OPT_INNODB_LOG_FILES_IN_GROUP:

      ADD_PRINT_PARAM_OPT(innobase_log_files_in_group);
      break;

    case OPT_INNODB_LOG_FILE_SIZE:

      ADD_PRINT_PARAM_OPT(innobase_log_file_size);
      break;

    case OPT_INNODB_FLUSH_METHOD:

      ADD_PRINT_PARAM_OPT(innodb_flush_method_names[innodb_flush_method]);
      break;

    case OPT_INNODB_PAGE_SIZE:

      ADD_PRINT_PARAM_OPT(innobase_page_size);
      break;

    case OPT_INNODB_LOG_BLOCK_SIZE:

      ADD_PRINT_PARAM_OPT(innobase_log_block_size);
      break;

    case OPT_INNODB_UNDO_DIRECTORY:

      ADD_PRINT_PARAM_OPT(srv_undo_dir);
      break;

    case OPT_INNODB_UNDO_TABLESPACES:

      ADD_PRINT_PARAM_OPT(srv_undo_tablespaces);
      break;

    case OPT_INNODB_CHECKSUM_ALGORITHM:

      ut_a(srv_checksum_algorithm <= SRV_CHECKSUM_ALGORITHM_STRICT_NONE);

      ADD_PRINT_PARAM_OPT(
          innodb_checksum_algorithm_names[srv_checksum_algorithm]);
      innodb_checksum_algorithm_specified = true;
      break;

    case OPT_INNODB_LOG_CHECKSUMS:

      ADD_PRINT_PARAM_OPT(srv_log_checksums);
      innodb_log_checksums_specified = true;
      break;

    case OPT_INNODB_BUFFER_POOL_FILENAME:

      ADD_PRINT_PARAM_OPT(innobase_buffer_pool_filename);
      break;

    case OPT_XTRA_TARGET_DIR:
      strmake(xtrabackup_real_target_dir, argument,
              sizeof(xtrabackup_real_target_dir) - 1);
      xtrabackup_target_dir = xtrabackup_real_target_dir;
      break;
    case OPT_XTRA_STREAM:
      if (argument == NULL || !strcasecmp(argument, "xbstream"))
        xtrabackup_stream_fmt = XB_STREAM_FMT_XBSTREAM;
      else {
        xb::error() << "Invalid --stream argument: " << argument;
        return 1;
      }
      xtrabackup_stream = true;
      break;
    case OPT_XTRA_FIFO_STREAMS:
      xtrabackup_fifo_streams_set = true;
      break;
    case OPT_XTRA_COMPRESS:
      if (argument == NULL) {
        xtrabackup_compress = XTRABACKUP_COMPRESS_ZSTD;
        xtrabackup_compress_ds = DS_TYPE_COMPRESS_ZSTD;
      } else if (strcasecmp(argument, "quicklz") == 0) {
        /* leaving this as an option to show proper error message */
        xtrabackup_compress = XTRABACKUP_COMPRESS_QUICKLZ;
        xtrabackup_compress_ds = DS_TYPE_COMPRESS_QUICKLZ;
      } else if (strcasecmp(argument, "lz4") == 0) {
        xtrabackup_compress = XTRABACKUP_COMPRESS_LZ4;
        xtrabackup_compress_ds = DS_TYPE_COMPRESS_LZ4;
      } else if (strcasecmp(argument, "zstd") == 0) {
        xtrabackup_compress = XTRABACKUP_COMPRESS_ZSTD;
        xtrabackup_compress_ds = DS_TYPE_COMPRESS_ZSTD;
      } else {
        xb::error() << "Invalid --compress argument: " << argument;
        return 1;
      }
      break;
    case OPT_XTRA_ENCRYPT:
      if (argument == NULL) {
        xb::error()
            << "Missing --encrypt argument, must specify a valid encryption"
            << " algorithm.";
        return 1;
      }
      xtrabackup_encrypt = true;
      break;
    case OPT_DECRYPT:
      if (argument == NULL) {
        xb::error() << "Missing --decrypt argument, must specify a"
                       " valid encryption algorithm.";
        return (1);
      }
      opt_decrypt = true;
      xtrabackup_decrypt_decompress = true;
      break;
    case OPT_DECOMPRESS:
      opt_decompress = true;
      xtrabackup_decrypt_decompress = true;
      break;
    case (int)OPT_CORE_FILE:
      test_flags |= TEST_CORE_ON_SIGNAL;
      break;
    case OPT_HISTORY:
      if (argument) {
        opt_history = argument;
      } else {
        opt_history = "";
      }
      break;
    case OPT_LOCK_DDL:
      if (argument == NULL || strcasecmp(argument, "on") == 0 ||
          strcasecmp(argument, "1") == 0 || strcasecmp(argument, "true") == 0) {
        opt_lock_ddl = LOCK_DDL_ON;
      } else if (strcasecmp(argument, "off") == 0 ||
                 strcasecmp(argument, "0") == 0 ||
                 strcasecmp(argument, "false") == 0) {
        opt_lock_ddl = LOCK_DDL_OFF;
      } else if (strcasecmp(argument, "reduced") == 0) {
        opt_lock_ddl = LOCK_DDL_REDUCED;
      } else {
        xb::error() << "Invalid --lock-ddl argument: " << argument;
        return 1;
      }
      break;
    case 'p':
      if (argument == disabled_my_option)
        argument = (char *)""; /* Don't require password */
      if (argument) {
        hide_option(argument, &opt_password);
        tty_password = false;
      } else
        tty_password = true;
      break;
    case OPT_TRANSITION_KEY:
      if (argument == disabled_my_option)
        argument = (char *)""; /* Don't require password */
      if (argument) {
        hide_option(argument, &opt_transition_key);
        tty_transition_key = false;
      } else
        tty_transition_key = true;
      use_dumped_tablespace_keys = true;
      break;
    case OPT_GENERATE_TRANSITION_KEY:
      use_dumped_tablespace_keys = true;
      break;
    case OPT_XTRA_ENCRYPT_KEY:
      hide_option(argument, &xtrabackup_encrypt_key);
      break;
    case OPT_XTRA_USE_MEMORY:
      xtrabackup_use_memory_set = true;
      break;

#include "sslopt-case.h"

    case '?':
      usage();
      exit(EXIT_SUCCESS);
      break;
    case 'v':
      print_version();
      exit(EXIT_SUCCESS);
      break;
    default:
      break;
  }
  return 0;
}

/** Check that a page_size is correct for InnoDB.
If correct, set the associated page_size_shift which is the power of 2
for this page size.
@param[in]      page_size       Page Size to evaluate
@return an associated page_size_shift if valid, 0 if invalid. */
inline ulong innodb_page_size_validate(ulong page_size) {
  ulong n;

  for (n = UNIV_PAGE_SIZE_SHIFT_MIN; n <= UNIV_PAGE_SIZE_SHIFT_MAX; n++) {
    if (page_size == static_cast<ulong>(1 << n)) {
      return (n);
    }
  }

  return (0);
}

static bool innodb_init_param(void) {
  /* innobase_init */
  static char current_dir[3]; /* Set if using current lib */
  char *default_path;
  ulint fsp_flags;

  /* === some variables from mysqld === */
  memset((G_PTR)&mysql_tmpdir_list, 0, sizeof(mysql_tmpdir_list));

  if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir)) exit(EXIT_FAILURE);

  /* dummy for initialize all_charsets[] */
  get_collation_name(0);

  /* Check that the value of system variable innodb_page_size was
  set correctly.  Its value was put into srv_page_size. If valid,
  return the associated srv_page_size_shift. */
  srv_page_size_shift = innodb_page_size_validate(innobase_page_size);
  if (!srv_page_size_shift) {
    xb::error() << "Invalid page size=" << innobase_page_size;
    goto error;
  }
  srv_page_size = innobase_page_size;

  /* Check that values don't overflow on 32-bit systems. */
  if (sizeof(ulint) == 4) {
    if (xtrabackup_use_memory > UINT_MAX32) {
      xb::error() << "use-memory can't be over 4GB"
                     " on 32-bit systems";
      goto error;
    }

    if (innobase_buffer_pool_size > UINT_MAX32) {
      xb::error() << "innobase_buffer_pool_size can't be"
                     " over 4GB on 32-bit systems";
      goto error;
    }

    if (innobase_log_file_size > UINT_MAX32) {
      xb::error() << "innobase_log_file_size can't be "
                     "over 4GB on 32-bit systems";
      goto error;
    }
  }

  os_innodb_umask = (ulint)0664;

  os_file_set_umask(my_umask);

  /* Setup the memory alloc/free tracing mechanisms before calling
  any functions that could possibly allocate memory. */
  ut_new_boot();

  /* First calculate the default path for innodb_data_home_dir etc.,
  in case the user has not given any value.

  Note that when using the embedded server, the datadirectory is not
  necessarily the current directory of this program. */

  /* It's better to use current lib, to keep paths short */
  current_dir[0] = FN_CURLIB;
  current_dir[1] = 0;
  default_path = current_dir;

  ut_a(default_path);
  {
    std::string mysqld_datadir{default_path};
    MySQL_datadir_path = Fil_path{mysqld_datadir};
  }
  /* Set InnoDB initialization parameters according to the values
  read from MySQL .cnf file */

  if (xtrabackup_backup) {
    xb::info() << "using the following InnoDB configuration:";
  } else {
    xb::info() << "using the following InnoDB configuration "
                  "for recovery:";
  }

  /*--------------- Data files -------------------------*/

  /* The default dir for data files is the datadir of MySQL */

  srv_data_home = xtrabackup_backup && (innobase_data_home_dir != nullptr &&
                                        *innobase_data_home_dir != '\0')
                      ? innobase_data_home_dir
                      : default_path;
  Fil_path::normalize(srv_data_home);
  xb::info() << "innodb_data_home_dir = " << srv_data_home;

  /*--------------- Shared tablespaces -------------------------*/

  /* Set default InnoDB data file size to 10 MB and let it be
  auto-extending. Thus users can use InnoDB in >= 4.0 without having
  to specify any startup options. */

  if (!innobase_data_file_path) {
    innobase_data_file_path = (char *)"ibdata1:10M:autoextend";
  }
  xb::info() << "innodb_data_file_path = " << innobase_data_file_path;

  /* This is the first time univ_page_size is used.
  It was initialized to 16k pages before srv_page_size was set */
  univ_page_size.copy_from(page_size_t(srv_page_size, srv_page_size, false));

  srv_sys_space.set_space_id(TRX_SYS_SPACE);

  /* Create the filespace flags. */
  fsp_flags = fsp_flags_init(univ_page_size, false, false, false, false);
  srv_sys_space.set_flags(fsp_flags);

  srv_sys_space.set_name(reserved_system_space_name);
  srv_sys_space.set_path(srv_data_home);

  /* Supports raw devices */
  if (!srv_sys_space.parse_params(innobase_data_file_path, true,
                                  xtrabackup_prepare)) {
    goto error;
  }

  /* Set default InnoDB temp data file size to 12 MB and let it be
  auto-extending. */

  if (!innobase_temp_data_file_path) {
    innobase_temp_data_file_path = (char *)"ibtmp1:12M:autoextend";
  }

  /* We set the temporary tablspace id later, after recovery.
  The temp tablespace doesn't support raw devices.
  Set the name and path. */
  srv_tmp_space.set_name(reserved_temporary_space_name);
  srv_tmp_space.set_path(srv_data_home);

  /* Create the filespace flags with the temp flag set. */
  fsp_flags = fsp_flags_init(univ_page_size, false, false, false, true);
  srv_tmp_space.set_flags(fsp_flags);

  if (!srv_tmp_space.parse_params(innobase_temp_data_file_path, false,
                                  xtrabackup_prepare)) {
    goto error;
  }

  /* Perform all sanity check before we take action of deleting files*/
  if (srv_sys_space.intersection(&srv_tmp_space)) {
    xb::error() << srv_tmp_space.name() << " and " << srv_sys_space.name()
                << " file names seem to be the same";
    goto error;
  }

  /* -------------- Log files ---------------------------*/

  /* The default dir for log files is the datadir of MySQL */

  if (!(xtrabackup_backup && srv_log_group_home_dir)) {
    srv_log_group_home_dir = default_path;
  }
  if (xtrabackup_prepare && xtrabackup_incremental_dir) {
    srv_log_group_home_dir = xtrabackup_incremental_dir;
  }

  xb::info() << "innodb_log_group_home_dir = " << srv_log_group_home_dir;

  Fil_path::normalize(srv_log_group_home_dir);

  if (strchr(srv_log_group_home_dir, ';')) {
    xb::error() << "syntax error in innodb_log_group_home_dir";

    goto error;
  }

  srv_adaptive_flushing = false;
  /* --------------------------------------------------*/

  srv_log_n_files = (ulint)innobase_log_files_in_group;
  srv_log_file_size = (ulint)innobase_log_file_size;
  xb::info() << "innodb_log_files_in_group = " << srv_log_n_files;
  xb::info() << "innodb_log_file_size = " << srv_log_file_size;

  srv_log_buffer_size = (ulint)innobase_log_buffer_size;
  srv_log_write_ahead_size = INNODB_LOG_WRITE_AHEAD_SIZE_DEFAULT;
  srv_log_flush_events = INNODB_LOG_EVENTS_DEFAULT;
  srv_log_write_events = INNODB_LOG_EVENTS_DEFAULT;
  srv_log_recent_written_size = INNODB_LOG_RECENT_WRITTEN_SIZE_DEFAULT;
  srv_log_recent_closed_size = INNODB_LOG_RECENT_CLOSED_SIZE_DEFAULT;
  srv_log_write_max_size = INNODB_LOG_WRITE_MAX_SIZE_DEFAULT;
  log_checksum_algorithm_ptr = srv_log_checksums ? log_block_calc_checksum_crc32
                                                 : log_block_calc_checksum_none;

  /* We set srv_pool_size here in units of 1 kB. InnoDB internally
  changes the value so that it becomes the number of database pages. */

  srv_buf_pool_chunk_unit = 134217728;
  srv_buf_pool_instances = 1;
  if (xtrabackup_incremental_dir) {
    real_redo_memory = incremental_redo_memory;
    real_redo_frames = incremental_redo_frames;
  } else {
    real_redo_memory = redo_memory;
    real_redo_frames = redo_frames;
  }

  estimate_memory = xtrabackup_prepare && real_redo_memory != 0 &&
                    real_redo_frames != 0 && !xtrabackup_use_memory_set &&
                    xtrabackup_use_free_memory_pct != 0;

  if (estimate_memory) {
    ut_ad(real_redo_frames != UINT64_MAX);

    ulint free_memory_total = xtrabackup::utils::host_free_memory();
    ulint free_memory_usable =
        (free_memory_total * xtrabackup_use_free_memory_pct) / 100;
    ulint mem = buf_pool_size_align(real_redo_memory +
                                    (real_redo_frames * UNIV_PAGE_SIZE));
    if (mem > (ulint)xtrabackup_use_memory) {
      xb::info() << "Got estimation from backup: " << real_redo_memory
                 << " bytes for parsing and " << real_redo_frames
                 << " frames. Requesting " << mem << " for --prepare.";
      if (mem > free_memory_usable) {
        xb::info() << "Required memory will exceed Free memory configuration. "
                   << "Free memory: " << free_memory_total << ". "
                   << "Free memory %: " << xtrabackup_use_free_memory_pct
                   << ". Allowed usage of Free Memory: " << free_memory_usable;

        /* We might exaust free memory if we pass 100% as parameter.
         * In this case we align usable memory down
         */
        if (buf_pool_size_align(free_memory_usable) > free_memory_total) {
          free_memory_usable = buf_pool_size_align_down(free_memory_usable);
        }
        xtrabackup_use_memory = free_memory_usable;
      } else {
        xtrabackup_use_memory = mem;
      }
      xb::info() << "Setting buffer pool to: " << xtrabackup_use_memory;
    }
    if ((long)real_redo_frames > innobase_open_files) {
      xb::info() << "Setting innobase_open_files to: " << real_redo_frames;
      innobase_open_files = (long)real_redo_frames;
    }
  }

  srv_buf_pool_size = (ulint)xtrabackup_use_memory;
  srv_buf_pool_size = buf_pool_size_align(srv_buf_pool_size);

  srv_n_read_io_threads = (ulint)innobase_read_io_threads;
  srv_n_write_io_threads = (ulint)innobase_write_io_threads;

  srv_force_recovery = (ulint)innobase_force_recovery;

  dblwr::g_mode = dblwr::Mode::OFF;

  if (!innobase_use_checksums) {
    srv_checksum_algorithm = SRV_CHECKSUM_ALGORITHM_NONE;
  }

  btr_search_enabled = (char)innobase_adaptive_hash_index;

  os_use_large_pages = (bool)innobase_use_large_pages;
  os_large_page_size = (ulint)innobase_large_page_size;

  row_rollback_on_timeout = (bool)innobase_rollback_on_timeout;

  srv_file_per_table = (bool)innobase_file_per_table;

  innobase_set_open_files_limit(innobase_open_files);
  srv_innodb_status = (bool)innobase_create_status_file;

  /* Store the default charset-collation number of this MySQL
  installation */

  /* We cannot treat characterset here for now!! */
  data_mysql_default_charset_coll = (ulint)default_charset_info->number;

  /* Since we in this module access directly the fields of a trx
  struct, and due to different headers and flags it might happen that
  mutex_t has a different size in this module and in InnoDB
  modules, we check at run time that the size is the same in
  these compilation modules. */

  /* On 5.5+ srv_use_native_aio is true by default. It is later reset
  if it is not supported by the platform in
  innobase_start_or_create_for_mysql(). As we don't call it in xtrabackup,
  we have to duplicate checks from that function here. */

#ifdef __WIN__
  switch (os_get_os_version()) {
    case OS_WIN95:
    case OS_WIN31:
    case OS_WINNT:
      /* On Win 95, 98, ME, Win32 subsystem for Windows 3.1,
      and NT use simulated aio. In NT Windows provides async i/o,
      but when run in conjunction with InnoDB Hot Backup, it seemed
      to corrupt the data files. */

      srv_use_native_aio = false;
      break;

    case OS_WIN2000:
    case OS_WINXP:
      /* On 2000 and XP, async IO is available. */
      srv_use_native_aio = true;
      break;

    default:
      /* Vista and later have both async IO and condition variables */
      srv_use_native_aio = true;
      srv_use_native_conditions = true;
      break;
  }

#elif defined(LINUX_NATIVE_AIO)

  if (srv_use_native_aio) {
    ib::info() << "Using Linux native AIO";
  }
#else
  /* Currently native AIO is supported only on windows and linux
  and that also when the support is compiled in. In all other
  cases, we ignore the setting of innodb_use_native_aio. */
  srv_use_native_aio = false;

#endif

  /* Assign the default value to srv_undo_dir if it's not specified, as
  my_getopt does not support default values for string options. We also
  ignore the option and override innodb_undo_directory on --prepare,
  because separate undo tablespaces are copied to the root backup
  directory. */

  if (!srv_undo_dir || !xtrabackup_backup) {
    my_free(srv_undo_dir);
    srv_undo_dir = my_strdup(PSI_NOT_INSTRUMENTED, ".", MYF(MY_FAE));
  }

  ut_ad(srv_undo_dir != nullptr);
  Fil_path::normalize(srv_undo_dir);
  MySQL_undo_path = Fil_path{srv_undo_dir};

  /* We want to save original value of srv_temp_dir because InnoDB will
  modify ibt::srv_temp_dir. */
  ibt::srv_temp_dir = srv_temp_dir;

  if (ibt::srv_temp_dir == nullptr) {
    ibt::srv_temp_dir = default_path;
  }

  Fil_path::normalize(ibt::srv_temp_dir);

  return (false);

error:
  xb::error() << "innodb_init_param(): Error occured.";
  return (true);
}

static void xb_scan_for_tablespaces() {
  /* This is the default directory for IBD and IBU files. Put it first
  in the list of known directories. */
  fil_set_scan_dir(MySQL_datadir_path.path());

  /* Add --innodb-data-home-dir as a known location for IBD and IBU files
  if it is not already there. */
  ut_ad(srv_data_home != nullptr && *srv_data_home != '\0');
  fil_set_scan_dir(Fil_path::remove_quotes(srv_data_home));

  /* Add --innodb-directories as known locations for IBD and IBU files. */
  if (srv_innodb_directories != nullptr && *srv_innodb_directories != 0) {
    fil_set_scan_dirs(Fil_path::remove_quotes(srv_innodb_directories));
  }

  /* For the purpose of file discovery at startup, we need to scan
  --innodb-undo-directory also. */
  fil_set_scan_dir(Fil_path::remove_quotes(MySQL_undo_path), true);

  if (fil_scan_for_tablespaces(true) != DB_SUCCESS) {
    exit(EXIT_FAILURE);
  }
}

static bool innodb_init(bool init_dd, bool for_apply_log) {
  os_event_global_init();

  /* Check if the data files exist or not. */
  dberr_t err = srv_sys_space.check_file_spec(false, 5 * 1024 * 1024 /* 5M */);

  if (err != DB_SUCCESS) {
    return (false);
  }

  lsn_t to_lsn = LSN_MAX;

  if (for_apply_log && (metadata_type == METADATA_FULL_BACKUP ||
                        xtrabackup_incremental_dir != nullptr)) {
    to_lsn = (xtrabackup_incremental_dir == nullptr) ? metadata_last_lsn
                                                     : incremental_last_lsn;
  }

  /* If pxb created redo log files (final prepare), the LSNs will be out of
  backup end_LSN. So we cannot use this LSN to verify that if we have applied
  to the target end_LSN (to_lsn). Unset it and let it apply to last recorded
  redo LSN in new redo files */
  if (xb_generated_redo && xtrabackup_incremental_dir == nullptr) {
    to_lsn = LSN_MAX;
  }

  err = srv_start(false, to_lsn);

  if (err != DB_SUCCESS) {
    free(internal_innobase_data_file_path);
    internal_innobase_data_file_path = NULL;
    goto error;
  }

  if (init_dd) {
    fil_open_ibds();

    dberr_t err;
    std::tie(err, mysql_ibd_tables) = xb::prepare::dict_load_from_mysql_ibd();
    if (err != DB_SUCCESS) {
      xb::error() << "Failed to load table using tablespace SDI ";
      goto error;
    }
    if (dict_sys->dynamic_metadata == nullptr)
      dict_sys->dynamic_metadata =
          dd_table_open_on_name(NULL, NULL, "mysql/innodb_dynamic_metadata",
                                false, DICT_ERR_IGNORE_NONE);
    if (dict_persist->table_buffer == nullptr)
      dict_persist->table_buffer =
          ut::new_withkey<DDTableBuffer>(UT_NEW_THIS_FILE_PSI_KEY);
    srv_dict_recover_on_restart();
  }

  srv_start_threads();

  if (srv_thread_is_active(srv_threads.m_trx_recovery_rollback)) {
    srv_threads.m_trx_recovery_rollback.wait();
  }

  innodb_inited = 1;

  return (false);

error:
  xb::error() << "innodb_init(): Error occured";
  return (true);
}

static bool innodb_end(void) {
  srv_fast_shutdown = (ulint)innobase_fast_shutdown;
  innodb_inited = 0;

  xb::info() << "starting shutdown with innodb_fast_shutdown = "
             << srv_fast_shutdown;

  srv_pre_dd_shutdown();

  mutex_free(&master_key_id_mutex);

  for (auto table : mysql_ibd_tables) {
    dict_table_close(table, false, false);
  }
  srv_shutdown();

  xb::prepare::clear_dd_cache_maps();
  os_event_global_destroy();

  free(internal_innobase_data_file_path);
  internal_innobase_data_file_path = NULL;

  return (false);
}

/* Read backup meta info.
@param[in] filename filename to read mysql version
@return true on success, false on failure. */
static bool xtrabackup_read_info(char *filename) {
  FILE *fp;
  bool r = true;

  fp = fopen(filename, "r");
  if (!fp) {
    xb::error() << "cannot open " << filename;
    return (false);
  }
  /* skip uuid, name, tool_name, tool_command, tool_version */
  for (int i = 0; i < 5; i++) {
    char c;
    do {
      c = fgetc(fp);
    } while (c != '\n');
  }

  if (fscanf(fp, "ibbackup_version = %29s\n", xtrabackup_version_str) != 1) {
    r = false;
    goto end;
  }
  xb_backup_version =
      xtrabackup::utils::get_version_number(xtrabackup_version_str);

  DBUG_EXECUTE_IF("simulate_backup_lower_version", xb_backup_version = 80030;);
  if (fscanf(fp, "server_version = %29s\n", mysql_server_version_str) != 1) {
    r = false;
    goto end;
  }

  if (fscanf(fp, "server_flavor = %255[^\n]", mysql_server_flavor) != 1) {
    r = false;
    goto end;
  }

  xb_server_version =
      xtrabackup::utils::get_version_number(mysql_server_version_str);

  ut_ad(xb_server_version > 80000 && xb_server_version < 90000);
  if (xb_server_version < 80019) {
    cfg_version = IB_EXPORT_CFG_VERSION_V3;
  } else if (xb_server_version < 80020) {
    cfg_version = IB_EXPORT_CFG_VERSION_V4;
  } else if (xb_server_version < 80023) {
    cfg_version = IB_EXPORT_CFG_VERSION_V5;
  } else if (xb_server_version < 80029) {
    cfg_version = IB_EXPORT_CFG_VERSION_V6;
  }
  /* skip start_time, end_time, lock_time, binlog_pos, innodb_from_lsn,
   * innodb_to_lsn, partial, incremental, format, compressed, encrypt */
  for (int i = 0; i < 12; i++) {
    char c;
    do {
      c = fgetc(fp);
    } while (c != '\n');
  }

  char lock[8];
  if (fscanf(fp, "lock_ddl_type = %7s\n", lock) != 1) {
    r = false;
    goto end;
  }
  /* used at log0recv.cc to apply or not file operations */
  opt_lock_ddl = ddl_lock_type_from_str(string(lock));
end:
  fclose(fp);
  return (r);
}

/* ================= common ================= */

/***********************************************************************
Read backup meta info.
@return true on success, false on failure. */
static bool xtrabackup_read_metadata(char *filename) {
  FILE *fp;
  bool r = true;

  fp = fopen(filename, "r");
  if (!fp) {
    xb::error() << "cannot open " << filename;
    return (false);
  }

  if (fscanf(fp, "backup_type = %29s\n", metadata_type_str) != 1) {
    r = false;
    goto end;
  }
  /* Use UINT64PF instead of LSN_PF here, as we have to maintain the file
  format. */
  if (fscanf(fp, "from_lsn = " LSN_PF "\n", &metadata_from_lsn) != 1) {
    r = false;
    goto end;
  }
  if (fscanf(fp, "to_lsn = " LSN_PF "\n", &metadata_to_lsn) != 1) {
    r = false;
    goto end;
  }
  if (fscanf(fp, "last_lsn = " LSN_PF "\n", &metadata_last_lsn) != 1) {
    metadata_last_lsn = 0;
  }
  if (fscanf(fp, "flushed_lsn = " LSN_PF "\n", &backup_redo_log_flushed_lsn) !=
      1) {
    backup_redo_log_flushed_lsn = 0;
  }
  if (fscanf(fp, "redo_memory = %lu\n", &redo_memory) != 1) {
    redo_memory = 0;
  }
  if (fscanf(fp, "redo_frames = %lu\n", &redo_frames) != 1) {
    redo_frames = 0;
  }

end:
  fclose(fp);

  return (r);
}

/***********************************************************************
Print backup meta info to a specified buffer. */
static void xtrabackup_print_metadata(char *buf, size_t buf_len) {
  /* Use UINT64PF instead of LSN_PF here, as we have to maintain the file
  format. */
  snprintf(buf, buf_len,
           "backup_type = %s\n"
           "from_lsn = " LSN_PF
           "\n"
           "to_lsn = " LSN_PF
           "\n"
           "last_lsn = " LSN_PF
           "\n"
           "flushed_lsn = " LSN_PF
           "\n"
           "redo_memory = %ld\n"
           "redo_frames = %ld\n",
           metadata_type_str, metadata_from_lsn, metadata_to_lsn,
           metadata_last_lsn,
           opt_lock_ddl == LOCK_DDL_ON ? backup_redo_log_flushed_lsn : 0,
           redo_memory, redo_frames);
}

/***********************************************************************
Stream backup meta info to a specified datasink.
@return true on success, false on failure. */
static bool xtrabackup_stream_metadata(ds_ctxt_t *ds_ctxt) {
  char buf[1024];
  size_t len;
  ds_file_t *stream;
  MY_STAT mystat;
  bool rc = true;

  xtrabackup_print_metadata(buf, sizeof(buf));

  len = strlen(buf);

  mystat.st_size = len;
  mystat.st_mtime = time(nullptr);

  stream = ds_open(ds_ctxt, XTRABACKUP_METADATA_FILENAME, &mystat);
  if (stream == NULL) {
    xb::error() << "cannot open output stream for "
                << XTRABACKUP_METADATA_FILENAME;
    return (false);
  }

  if (ds_write(stream, buf, len)) {
    rc = false;
  }

  if (ds_close(stream)) {
    rc = false;
  }

  return (rc);
}

static bool write_to_file(const char *filepath, const char *data) {
  size_t len = strlen(data);
  FILE *fp = fopen(filepath, "w");
  if (!fp) {
    xb::error() << "cannot open " << filepath;
    return (false);
  }
  if (fwrite(data, len, 1, fp) < 1) {
    fclose(fp);
    return (false);
  }

  fclose(fp);
  return true;
}

/***********************************************************************
Write backup meta info to a specified file.
@return true on success, false on failure. */
static bool xtrabackup_write_metadata(const char *filepath) {
  char buf[1024];

  xtrabackup_print_metadata(buf, sizeof(buf));
  return write_to_file(filepath, buf);
}

/***********************************************************************
Read meta info for an incremental delta.
@return true on success, false on failure. */
static bool xb_read_delta_metadata(const char *filepath,
                                   xb_delta_info_t *info) {
  FILE *fp;
  char key[51];
  char value[51];
  bool r = true;

  /* set defaults */
  info->page_size = ULINT_UNDEFINED;
  info->zip_size = ULINT_UNDEFINED;
  info->space_id = SPACE_UNKNOWN;
  info->space_flags = 0;

  fp = fopen(filepath, "r");
  if (!fp) {
    /* Meta files for incremental deltas are optional */
    return (true);
  }

  while (!feof(fp)) {
    if (fscanf(fp, "%50s = %50s\n", key, value) == 2) {
      if (strcmp(key, "page_size") == 0) {
        info->page_size = strtoul(value, NULL, 10);
      } else if (strcmp(key, "zip_size") == 0) {
        info->zip_size = strtoul(value, NULL, 10);
      } else if (strcmp(key, "space_id") == 0) {
        info->space_id = strtoul(value, NULL, 10);
      } else if (strcmp(key, "space_flags") == 0) {
        info->space_flags = strtoul(value, NULL, 10);
      }
    }
  }

  fclose(fp);

  if (info->page_size == ULINT_UNDEFINED) {
    xb::error() << "page_size is required in " << filepath;
    r = false;
  }
  if (info->space_id == SPACE_UNKNOWN) {
    xb::warn()
        << "This backup was taken with XtraBackup 2.0.1 "
           "or earlier, some DDL operations between full and incremental "
           "backups may be handled incorrectly";
  }

  return (r);
}

/***********************************************************************
Write meta info for an incremental delta.
@return true on success, false on failure. */
bool xb_write_delta_metadata(const char *filename,
                             const xb_delta_info_t *info) {
  ds_file_t *f;
  char buf[200];
  bool ret;
  size_t len;
  MY_STAT mystat;

  snprintf(buf, sizeof(buf),
           "page_size = %lu\n"
           "zip_size = %lu\n"
           "space_id = %lu\n"
           "space_flags = %lu\n",
           info->page_size, info->zip_size, info->space_id, info->space_flags);
  len = strlen(buf);

  mystat.st_size = len;
  mystat.st_mtime = time(nullptr);

  f = ds_open(ds_meta, filename, &mystat);
  if (f == NULL) {
    xb::error() << "cannot open output stream for " << filename;
    return (false);
  }

  ret = (ds_write(f, buf, len) == 0);

  if (ds_close(f)) {
    ret = false;
  }

  return (ret);
}

static bool xtrabackup_write_info(const char *filepath) {
  char *xtrabackup_info_data = get_xtrabackup_info(mysql_connection);
  if (!xtrabackup_info_data) {
    return false;
  }

  bool result = write_to_file(filepath, xtrabackup_info_data);

  free(xtrabackup_info_data);
  return result;
}

std::string ddl_lock_type_to_str(lock_ddl_type_t type) {
  switch (type) {
    case LOCK_DDL_ON:
      return "ON";
    case LOCK_DDL_OFF:
      return "OFF";
    case LOCK_DDL_REDUCED:
      return "REDUCED";
    default:
      ut_error;
  }
}

lock_ddl_type_t ddl_lock_type_from_str(std::string type) {
  if (type == "ON") {
    return LOCK_DDL_ON;
  } else if (type == "OFF") {
    return LOCK_DDL_OFF;
  } else if (type == "REDUCED") {
    return LOCK_DDL_REDUCED;
  } else {
    ut_error;
  }
}
/* ================= backup ================= */
void xtrabackup_io_throttling(void) {
  if (xtrabackup_throttle && (--io_ticket) < 0) {
    os_event_reset(wait_throttle);
    os_event_wait(wait_throttle);
  }
}

static bool regex_list_check_match(const regex_list_t &list, const char *name) {
  xb_regmatch_t tables_regmatch[1];
  for (regex_list_t::const_iterator i = list.begin(), end = list.end();
       i != end; ++i) {
    const xb_regex_t &regex = *i;
    int regres = xb_regexec(&regex, name, 1, tables_regmatch, 0);

    if (regres != REG_NOMATCH) {
      return (true);
    }
  }
  return (false);
}

static bool find_filter_in_hashtable(const char *name, hash_table_t *table,
                                     xb_filter_entry_t **result) {
  xb_filter_entry_t *found = NULL;
  HASH_SEARCH(name_hash, table, ut::hash_string(name), xb_filter_entry_t *,
              found, (void)0, !strcmp(found->name, name));

  if (found && result) {
    *result = found;
  }
  return (found != NULL);
}

/************************************************************************
Checks if a given table name matches any of specifications given in
regex_list or tables_hash.

@return true on match or both regex_list and tables_hash are empty.*/
static bool check_if_table_matches_filters(const char *name,
                                           const regex_list_t &regex_list,
                                           hash_table_t *tables_hash) {
  if (regex_list.empty() && !tables_hash) {
    return (false);
  }

  if (regex_list_check_match(regex_list, name)) {
    return (true);
  }

  if (tables_hash && find_filter_in_hashtable(name, tables_hash, NULL)) {
    return (true);
  }

  return false;
}

enum skip_database_check_result {
  DATABASE_SKIP,
  DATABASE_SKIP_SOME_TABLES,
  DATABASE_DONT_SKIP,
  DATABASE_DONT_SKIP_UNLESS_EXPLICITLY_EXCLUDED,
};

/************************************************************************
Checks if a database specified by name should be skipped from backup based on
the --databases, --databases_file or --databases_exclude options.

@return true if entire database should be skipped,
        false otherwise.
*/
static skip_database_check_result check_if_skip_database(
    const char *name /*!< in: path to the database */
) {
  /* There are some filters for databases, check them */
  xb_filter_entry_t *database = NULL;

  if (databases_exclude_hash &&
      find_filter_in_hashtable(name, databases_exclude_hash, &database) &&
      !database->has_tables) {
    /* Database is found and there are no tables specified,
       skip entire db. */
    return DATABASE_SKIP;
  }

  if (databases_include_hash) {
    if (!find_filter_in_hashtable(name, databases_include_hash, &database)) {
      /* Database isn't found, skip the database */
      return DATABASE_SKIP;
    } else if (database->has_tables) {
      return DATABASE_SKIP_SOME_TABLES;
    } else {
      return DATABASE_DONT_SKIP_UNLESS_EXPLICITLY_EXCLUDED;
    }
  }

  return DATABASE_DONT_SKIP;
}

/************************************************************************
Checks if a database specified by path should be skipped from backup based on
the --databases, --databases_file or --databases_exclude options.

@return true if the table should be skipped. */
bool check_if_skip_database_by_path(
    const char *path /*!< in: path to the db directory. */
) {
  if (databases_include_hash == NULL && databases_exclude_hash == NULL) {
    return (false);
  }

  const char *db_name = strrchr(path, OS_PATH_SEPARATOR);
  if (db_name == NULL) {
    db_name = path;
  } else {
    ++db_name;
  }

  return check_if_skip_database(db_name) == DATABASE_SKIP;
}

/************************************************************************
Checks if a table specified as a name in the form "database/name" (InnoDB 5.6)
or "./database/name.ibd" (InnoDB 5.5-) should be skipped from backup based on
the --tables or --tables-file options.

@return true if the table should be skipped. */
bool check_if_skip_table(
    /******************/
    const char *name) /*!< in: path to the table */
{
  char buf[FN_REFLEN];
  const char *dbname, *tbname;
  const char *ptr;
  char *eptr;

  if (regex_exclude_list.empty() && regex_include_list.empty() &&
      tables_include_hash == NULL && tables_exclude_hash == NULL &&
      databases_include_hash == NULL && databases_exclude_hash == NULL) {
    return (false);
  }

  dbname = NULL;
  tbname = name;
  while ((ptr = strchr(tbname, OS_PATH_SEPARATOR)) != NULL) {
    dbname = tbname;
    tbname = ptr + 1;
  }

  if (dbname == NULL) {
    return (false);
  }

  strncpy(buf, dbname, FN_REFLEN - 1);
  buf[tbname - 1 - dbname] = 0;

  const skip_database_check_result skip_database = check_if_skip_database(buf);
  if (skip_database == DATABASE_SKIP) {
    return (true);
  }

  buf[FN_REFLEN - 1] = '\0';
  buf[tbname - 1 - dbname] = '.';

  /* Check if there's a suffix in the table name. If so, truncate it. We
  rely on the fact that a dot cannot be a part of a table name (it is
  encoded by the server with the @NNNN syntax). */
  if ((eptr = strchr(&buf[tbname - dbname], '.')) != NULL) {
    *eptr = '\0';
  }

  /* For partitioned tables first try to match against the regexp
  without truncating the #P#... suffix so we can backup individual
  partitions with regexps like '^test[.]t#P#p5' */
  if (check_if_table_matches_filters(buf, regex_exclude_list,
                                     tables_exclude_hash)) {
    return (true);
  }
  if (check_if_table_matches_filters(buf, regex_include_list,
                                     tables_include_hash)) {
    return (false);
  }
  if ((eptr = strcasestr(buf, "#P#")) != NULL) {
    *eptr = 0;

    if (check_if_table_matches_filters(buf, regex_exclude_list,
                                       tables_exclude_hash)) {
      return (true);
    }
    if (check_if_table_matches_filters(buf, regex_include_list,
                                       tables_include_hash)) {
      return (false);
    }
  }

  if (skip_database == DATABASE_DONT_SKIP_UNLESS_EXPLICITLY_EXCLUDED) {
    /* Database is in include-list, and qualified name wasn't
       found in any of exclusion filters.*/
    return (false);
  }

  if (skip_database == DATABASE_SKIP_SOME_TABLES ||
      !regex_include_list.empty() || tables_include_hash) {
    /* Include lists are present, but qualified name
       failed to match any.*/
    return (true);
  }

  return (false);
}

const char *xb_get_copy_action(const char *dflt) {
  const char *action;

  if (xtrabackup_stream) {
    if (xtrabackup_compress != XTRABACKUP_COMPRESS_NONE) {
      if (xtrabackup_encrypt) {
        action = "Compressing, encrypting and streaming";
      } else {
        action = "Compressing and streaming";
      }
    } else if (xtrabackup_encrypt) {
      action = "Encrypting and streaming";
    } else {
      action = "Streaming";
    }
  } else {
    if (xtrabackup_compress != XTRABACKUP_COMPRESS_NONE) {
      if (xtrabackup_encrypt) {
        action = "Compressing and encrypting";
      } else {
        action = "Compressing";
      }
    } else if (xtrabackup_encrypt) {
      action = "Encrypting";
    } else {
      action = dflt;
    }
  }

  return (action);
}

/* TODO: We may tune the behavior (e.g. by fil_aio)*/

static bool xtrabackup_copy_datafile(fil_node_t *node, uint thread_n) {
  return xtrabackup_copy_datafile(node, thread_n, nullptr);
}

bool xtrabackup_copy_datafile(fil_node_t *node, uint thread_n,
                              const char *dest_name) {
  char dst_name[FN_REFLEN];
  ds_file_t *dstfile = NULL;
  xb_fil_cur_t cursor;
  xb_fil_cur_result_t res;
  xb_write_filt_t *write_filter = NULL;
  xb_write_filt_ctxt_t write_filt_ctxt;
  const char *action;
  xb_read_filt_t *read_filter;
  bool rc = false;

  /* Get the name and the path for the tablespace. node->name always
  contains the path (which may be absolute for remote tablespaces in
  5.6+). space->name contains the tablespace name in the form
  "./database/table.ibd" (in 5.5-) or "database/table" (in 5.6+). For a
  multi-node shared tablespace, space->name contains the name of the first
  node, but that's irrelevant, since we only need node_name to match them
  against filters, and the shared tablespace is always copied regardless
  of the filters value. */

  const char *const node_name = node->space->name;
  const char *const node_path = node->name;

  bool is_system = !fsp_is_ibd_tablespace(node->space->id);

  if (!is_system && check_if_skip_table(node_name)) {
    xb::info() << " Skipping " << node_name;
    return (false);
  }

  if (changed_page_tracking) {
    read_filter = &rf_page_tracking;
  } else {
    read_filter = &rf_pass_through;
  }
  res = xb_fil_cur_open(&cursor, read_filter, node, thread_n);
  if (res == XB_FIL_CUR_SKIP) {
    goto skip;
  } else if (res == XB_FIL_CUR_ERROR) {
    goto error;
  }

  strncpy(dst_name, dest_name ? dest_name : cursor.rel_path,
          sizeof dst_name - 1);
  dst_name[sizeof(dst_name) - 1] = '\0';

  /* Setup the page write filter */
  if (xtrabackup_incremental) {
    write_filter = &wf_incremental;
  } else {
    write_filter = &wf_write_through;
  }

  memset(&write_filt_ctxt, 0, sizeof(xb_write_filt_ctxt_t));
  ut_a(write_filter->process != NULL);

  if (write_filter->init != NULL &&
      !write_filter->init(&write_filt_ctxt, dst_name, &cursor)) {
    xb::error() << "failed to initialize page write filter.";
    goto error;
  }

  /* do not compress encrypted tablespaces */
  if (cursor.is_encrypted) {
    dstfile = ds_open(ds_uncompressed_data, dst_name, &cursor.statinfo);
  } else {
    dstfile = ds_open(ds_data, dst_name, &cursor.statinfo);
  }
  if (dstfile == NULL) {
    xb::error() << "cannot open the destination stream for " << dst_name;
    goto error;
  }

  action = xb_get_copy_action();

  if (xtrabackup_stream) {
    xb::info() << action << " " << node_path;
  } else {
    xb::info() << action << " " << node_path << " to " << dstfile->path;
  }

  /* The main copy loop */
  while ((res = xb_fil_cur_read(&cursor)) == XB_FIL_CUR_SUCCESS) {
    if (!write_filter->process(&write_filt_ctxt, dstfile)) {
      goto error;
    }
  }

  if (res == XB_FIL_CUR_ERROR) {
    goto error;
  }

  if (write_filter->finalize &&
      !write_filter->finalize(&write_filt_ctxt, dstfile)) {
    goto error;
  }

  /* close */
  if (xtrabackup_stream) {
    xb::info() << "Done: " << action << " " << node_path;
  } else {
    xb::info() << "Done: " << action << " " << node_path << " to "
               << dstfile->path;
  }

  xb_fil_cur_close(&cursor);
  if (ds_close(dstfile)) {
    rc = true;
  }
  if (write_filter && write_filter->deinit) {
    write_filter->deinit(&write_filt_ctxt);
  }
  return (rc);

error:
  xb_fil_cur_close(&cursor);
  if (dstfile != NULL) {
    ds_close(dstfile);
  }
  if (write_filter && write_filter->deinit) {
    write_filter->deinit(&write_filt_ctxt);
    ;
  }
  xb::error() << "xtrabackup_copy_datafile() failed";
  return (true); /*ERROR*/

skip:

  if (dstfile != NULL) {
    ds_close(dstfile);
  }
  if (write_filter && write_filter->deinit) {
    write_filter->deinit(&write_filt_ctxt);
  }

  if (opt_lock_ddl != LOCK_DDL_ON) {
    xb::warn() << "We assume the "
               << "table was dropped during xtrabackup execution "
               << "and ignore the file.";
  }
  xb::warn() << "skipping tablespace " << node_name;
  return (false);
}

/* io throttle watching (rough) */
void io_watching_thread() {
  /* currently, for --backup only */
  ut_a(xtrabackup_backup);

  io_watching_thread_running = true;

  while (!io_watching_thread_stop) {
    std::this_thread::sleep_for(std::chrono::seconds(1)); /*1 sec*/
    io_ticket = xtrabackup_throttle;
    os_event_set(wait_throttle);
  }

  /* stop io throttle */
  xtrabackup_throttle = 0;
  os_event_set(wait_throttle);

  std::atomic_thread_fence(std::memory_order_seq_cst);
  io_watching_thread_running = false;
}


/**************************************************************************
Datafiles copying thread.*/
static void data_copy_thread_func(data_thread_ctxt_t *ctxt) {
  uint num = ctxt->num;
  fil_node_t *node;

  /*
    Initialize mysys thread-specific memory so we can
    use mysys functions in this thread.
  */
  my_thread_init();

  /* create THD to get thread number in the error log */
  THD *thd = create_thd(false, false, true, 0, 0);
  debug_sync_point("data_copy_thread_func");

  while ((node = datafiles_iter_next(ctxt->it)) != NULL && !*(ctxt->error)) {
    /* copy the datafile */
    if (xtrabackup_copy_datafile(node, num)) {
      xb::error() << "failed to copy datafile " << node->name;
      *(ctxt->error) = true;
    }
  }

  mutex_enter(ctxt->count_mutex);
  (*ctxt->count)--;
  mutex_exit(ctxt->count_mutex);

  destroy_thd(thd);
  my_thread_end();
}

/************************************************************************
Validate by the end of the backup if we have parsed required encryption data
for tablespaces that had an empty page0 */
bool validate_missing_encryption_tablespaces() {
  bool ret = true;
  bool found = false;
  if (invalid_encrypted_tablespace_ids.size() > 0) {
    for (auto m_space_id : invalid_encrypted_tablespace_ids) {
      found = false;
      mutex_enter(&recv_sys->mutex);
      if (recv_sys->keys != nullptr) {
        for (const auto &recv_key : *recv_sys->keys) {
          if (recv_key.space_id == m_space_id) {
            found = true;
          }
        }
      }
      mutex_exit(&recv_sys->mutex);
      if (!found) {
        xb::error() << "Space ID " << m_space_id
                    << " is missing encryption "
                       "information.";
        ret = false;
      }
    }
  }
  return ret;
}

/************************************************************************
Initialize the appropriate datasink(s). Both local backups and streaming in the
'xbstream' format allow parallel writes so we can write directly.

Otherwise (i.e. when streaming in the 'tar' format) we need 2 separate datasinks
for the data stream (and don't allow parallel data copying) and for metainfo
files (including xtrabackup_logfile). The second datasink writes to temporary
files first, and then streams them in a serialized way when closed. */
static void xtrabackup_init_datasinks(void) {
  /* Start building out the pipelines from the terminus back */
  if (xtrabackup_stream) {
    if (xtrabackup_fifo_streams > 1) {
      /* Use Named PIPEs */
      xb::info() << "Creating " << xtrabackup_fifo_streams
                 << " Named Pipes(FIFO) at folder " << xtrabackup_target_dir
                 << ". Waiting up to " << xtrabackup_fifo_timeout
                 << " second(s) for xbstream/xbcloud to open the "
                    "files for reading.\n";
      ds_data = ds_meta = ds_redo =
          ds_create(xtrabackup_target_dir, DS_TYPE_FIFO);
    } else {
      /* All streaming goes to stdout */
      ds_data = ds_meta = ds_redo =
          ds_create(xtrabackup_target_dir, DS_TYPE_STDOUT);
    }
  } else {
    /* Local filesystem */
    ds_data = ds_meta = ds_redo =
        ds_create(xtrabackup_target_dir, DS_TYPE_LOCAL);
    punch_hole_supported = ds_data->fs_support_punch_hole;
  }

  /* Track it for destruction */
  xtrabackup_add_datasink(ds_data);

  /* Stream formatting */
  if (xtrabackup_stream) {
    ds_ctxt_t *ds;
    if (xtrabackup_stream_fmt == XB_STREAM_FMT_XBSTREAM) {
      ds = ds_create(xtrabackup_target_dir, DS_TYPE_XBSTREAM);
    } else {
      /* bad juju... */
      ds = NULL;
    }

    xtrabackup_add_datasink(ds);

    ds_set_pipe(ds, ds_data);
    ds_data = ds;

    if (xtrabackup_stream_fmt != XB_STREAM_FMT_XBSTREAM) {
      /* 'tar' does not allow parallel streams */
      ds_redo = ds_meta = ds_create(xtrabackup_target_dir, DS_TYPE_TMPFILE);
      xtrabackup_add_datasink(ds_meta);
      ds_set_pipe(ds_meta, ds);
    } else {
      ds_redo = ds_meta = ds_data;
    }
  }

  /* Encryption */
  if (xtrabackup_encrypt) {
    ds_ctxt_t *ds;

    ds_encrypt_algo = xtrabackup_encrypt_algo;
    ds_encrypt_key = xtrabackup_encrypt_key;
    ds_encrypt_key_file = xtrabackup_encrypt_key_file;
    ds_encrypt_encrypt_threads = xtrabackup_encrypt_threads;
    ds_encrypt_encrypt_chunk_size = xtrabackup_encrypt_chunk_size;

    ds = ds_create(xtrabackup_target_dir, DS_TYPE_ENCRYPT);
    xtrabackup_add_datasink(ds);

    ds_set_pipe(ds, ds_data);
    if (ds_data != ds_meta) {
      ds_data = ds;
      ds = ds_create(xtrabackup_target_dir, DS_TYPE_ENCRYPT);
      xtrabackup_add_datasink(ds);

      ds_set_pipe(ds, ds_redo);
      ds_redo = ds;
    } else {
      ds_redo = ds_data = ds;
    }
  }

  ds_uncompressed_data = ds_data;

  /* Compression for ds_data and ds_redo */
  if (xtrabackup_compress != XTRABACKUP_COMPRESS_NONE) {
    ds_ctxt_t *ds;

    /* Use a 1 MB buffer for compressed output stream */
    ds = ds_create(xtrabackup_target_dir, DS_TYPE_BUFFER);
    ds_buffer_set_size(ds, opt_read_buffer_size);
    xtrabackup_add_datasink(ds);
    ds_set_pipe(ds, ds_data);
    if (ds_data != ds_redo) {
      ds_data = ds;
      ds = ds_create(xtrabackup_target_dir, DS_TYPE_BUFFER);
      ds_buffer_set_size(ds, opt_read_buffer_size);
      xtrabackup_add_datasink(ds);
      ds_set_pipe(ds, ds_redo);
      ds_redo = ds;
    } else {
      ds_redo = ds_data = ds;
    }

    /* Removal of qpress warning */
    if (xtrabackup_compress == XTRABACKUP_COMPRESS_QUICKLZ) {
      xb::error() << "--compress using quicklz is removed. Please use ZSTD or "
                     "LZ4 instead.";
      exit(EXIT_FAILURE);
    }
    ds = ds_create(xtrabackup_target_dir, xtrabackup_compress_ds);
    xtrabackup_add_datasink(ds);
    ds_set_pipe(ds, ds_data);

    /* disable redo compression if redo log is encrypt */
    if (srv_redo_log_encrypt) {
      ds_data = ds;
    } else {
      if (ds_data != ds_redo) {
        ds_data = ds;
        ds = ds_create(xtrabackup_target_dir, xtrabackup_compress_ds);
        xtrabackup_add_datasink(ds);
        ds_set_pipe(ds, ds_redo);
        ds_redo = ds;
      } else {
        ds_redo = ds_data = ds;
      }
    }
  }
}

/************************************************************************
Destroy datasinks.

Destruction is done in the specific order to not violate their order in the
pipeline so that each datasink is able to flush data down the pipeline. */
static void xtrabackup_destroy_datasinks(void) {
  for (uint i = actual_datasinks; i > 0; i--) {
    ds_destroy(datasinks[i - 1]);
    datasinks[i - 1] = NULL;
  }
  ds_data = NULL;
  ds_meta = NULL;
  ds_redo = NULL;
}

#define SRV_N_PENDING_IOS_PER_THREAD OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS 100

/************************************************************************
Initializes the I/O and tablespace cache subsystems. */
static bool xb_fil_io_init(void)
/*================*/
{

  if (!os_aio_init(srv_n_read_io_threads, srv_n_write_io_threads)) {
    xb::error() << "Cannot initialize AIO sub-system.";
    return false;
  }

  fil_init(LONG_MAX);

  fsp_init();

  return true;
}

/****************************************************************************
Populates the tablespace memory cache by scanning for and opening data files.
@returns DB_SUCCESS or error code.*/
static dberr_t xb_load_tablespaces(void)
/*=====================*/
{
  dberr_t err;
  page_no_t sum_of_new_sizes;
  lsn_t flush_lsn;

  os_aio_start_threads();

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  err = srv_sys_space.check_file_spec(false, 0);

  if (err != DB_SUCCESS) {
    xb::error() << "could not find data files at the specified datadir";
    return (DB_ERROR);
  }

  err =
      srv_sys_space.open_or_create(false, false, &sum_of_new_sizes, &flush_lsn);

  if (err != DB_SUCCESS) {
    xb::error() << "Could not open or create data files.";
    xb::error() << "If you tried to add new data files, and it "
                   "failed here,";
    xb::error() << "you should now edit innodb_data_file_path in "
                   "my.cnf back";
    xb::error() << "to what it was, and remove the new ibdata "
                   "files InnoDB created";
    xb::error() << "in this failed attempt. InnoDB only wrote "
                   "those files full of";
    xb::error() << "zeros, but did not yet use them in any way. "
                   "But be careful: do not";
    xb::error() << "remove old data files which contain your "
                   "precious data!";
    return (err);
  }

  xb::info() << "Generating a list of tablespaces";
  xb_scan_for_tablespaces();

  /* Add separate undo tablespaces to fil_system */

  err = srv_undo_tablespaces_init(false, true);
  if (err != DB_SUCCESS) {
    return (err);
  }

  for (auto tablespace : Tablespace_map::instance().external_files()) {
    if (tablespace.type != Tablespace_map::TABLESPACE) continue;
    fil_open_for_xtrabackup(tablespace.file_name, tablespace.name);
  }

  debug_sync_point("xtrabackup_load_tablespaces_pause");

  return (DB_SUCCESS);
}

/************************************************************************
Initialize the tablespace memory cache and populate it by scanning for and
opening data files.
@returns DB_SUCCESS or error code.*/
ulint xb_data_files_init(void)
/*====================*/
{
  os_create_block_cache();

  if (!xb_fil_io_init()) {
    return DB_IO_ERROR;
  }

  undo_spaces_init();

  return (xb_load_tablespaces());
}

/************************************************************************
Destroy the tablespace memory cache. */
void xb_data_files_close(void)
/*====================*/
{
  ulint i;

  srv_shutdown_state = SRV_SHUTDOWN_EXIT_THREADS;

  /* All threads end up waiting for certain events. Put those events
  to the signaled state. Then the threads will exit themselves after
  os_event_wait(). */
  for (i = 0; i < 1000; i++) {
    if (!buf_flush_page_cleaner_is_active() && os_aio_all_slots_free()) {
      os_aio_wake_all_threads_at_shutdown();
    }

    /* f. dict_stats_thread is signaled from
    logs_empty_and_mark_files_at_shutdown() and should have
    already quit or is quitting right now. */

    bool active = os_thread_any_active();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!active) {
      break;
    }
  }

  if (i == 1000) {
    ib::warn() << os_thread_count
               << " threads created by InnoDB had not exited at shutdown!";
  }

  undo_spaces_deinit();

  os_aio_free();

  fil_close_all_files();

  fil_close();


  srv_shutdown_state = SRV_SHUTDOWN_NONE;
}

/***********************************************************************
Allocate and initialize the entry for databases and tables filtering
hash tables. If memory allocation is not successful, terminate program.
@return pointer to the created entry.  */
static xb_filter_entry_t *xb_new_filter_entry(
    /*================*/
    const char *name) /*!< in: name of table/database */
{
  xb_filter_entry_t *entry;
  ulint namelen = strlen(name);

  ut_a(namelen <= NAME_LEN * 2 + 1);

  entry = static_cast<xb_filter_entry_t *>(ut::zalloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, sizeof(xb_filter_entry_t) + namelen + 1));
  entry->name = ((char *)entry) + sizeof(xb_filter_entry_t);
  strcpy(entry->name, name);
  entry->has_tables = false;

  return entry;
}

/***********************************************************************
Add entry to hash table. If hash table is NULL, allocate and initialize
new hash table */
static xb_filter_entry_t *xb_add_filter(
    /*========================*/
    const char *name,    /*!< in: name of table/database */
    hash_table_t **hash) /*!< in/out: hash to insert into */
{
  xb_filter_entry_t *entry;

  entry = xb_new_filter_entry(name);

  if (*hash == nullptr) {
    *hash = ut::new_<hash_table_t>(1000);
  }
  HASH_INSERT(xb_filter_entry_t, name_hash, *hash, ut::hash_string(entry->name),
              entry);

  return entry;
}

/***********************************************************************
Validate name of table or database. If name is invalid, program will
be finished with error code */
static void xb_validate_name(
    /*=============*/
    const char *name, /*!< in: name */
    size_t len)       /*!< in: length of name */
{
  const char *p;

  /* perform only basic validation. validate length and
  path symbols */
  if (len > NAME_LEN) {
    xb::error() << "name " << name << " is too long";
    exit(EXIT_FAILURE);
  }
  p = strpbrk(name, "/\\~");
  if (p && p - name < NAME_LEN) {
    xb::error() << "name " << name << " is not valid";
    exit(EXIT_FAILURE);
  }
}

/***********************************************************************
Register new filter entry which can be either database
or table name.  */
static void xb_register_filter_entry(
    /*=====================*/
    const char *name, /*!< in: name */
    hash_table_t **databases_hash, hash_table_t **tables_hash) {
  const char *p;
  size_t namelen;
  xb_filter_entry_t *db_entry = NULL;

  namelen = strlen(name);
  if ((p = strchr(name, '.')) != NULL) {
    char dbname[NAME_LEN + 1];

    xb_validate_name(name, p - name);
    xb_validate_name(p + 1, namelen - (p - name));

    strncpy(dbname, name, p - name);
    dbname[p - name] = 0;

    if (*databases_hash) {
      HASH_SEARCH(name_hash, (*databases_hash), ut::hash_string(dbname),
                  xb_filter_entry_t *, db_entry, (void)0,
                  !strcmp(db_entry->name, dbname));
    }
    if (!db_entry) {
      db_entry = xb_add_filter(dbname, databases_hash);
    }
    db_entry->has_tables = true;
    xb_add_filter(name, tables_hash);
  } else {
    xb_validate_name(name, namelen);

    xb_add_filter(name, databases_hash);
  }
}

static void xb_register_include_filter_entry(const char *name) {
  xb_register_filter_entry(name, &databases_include_hash, &tables_include_hash);
}

static void xb_register_exclude_filter_entry(const char *name) {
  xb_register_filter_entry(name, &databases_exclude_hash, &tables_exclude_hash);
}

/***********************************************************************
Register new table for the filter.  */
static void xb_register_table(
    /*==============*/
    const char *name) /*!< in: name of table */
{
  if (strchr(name, '.') == NULL) {
    xb::error() << name << " is not fully qualified name";
    exit(EXIT_FAILURE);
  }

  xb_register_include_filter_entry(name);
}

bool compile_regex(const char *regex_string, const char *error_context,
                   xb_regex_t *compiled_re) {
  char errbuf[100];
  int ret = xb_regcomp(compiled_re, regex_string, REG_EXTENDED);
  if (ret != 0) {
    xb_regerror(ret, compiled_re, errbuf, sizeof(errbuf));
    xb::error() << error_context << " regcomp(" << regex_string
                << "): " << errbuf;
    return false;
  }
  return true;
}

static void xb_add_regex_to_list(
    const char *regex,         /*!< in: regex */
    const char *error_context, /*!< in: context to error message */
    regex_list_t *list)        /*! in: list to put new regex to */
{
  xb_regex_t compiled_regex;
  if (!compile_regex(regex, error_context, &compiled_regex)) {
    exit(EXIT_FAILURE);
  }

  list->push_back(compiled_regex);
}

/***********************************************************************
Register new regex for the include filter.  */
static void xb_register_include_regex(
    /*==============*/
    const char *regex) /*!< in: regex */
{
  xb_add_regex_to_list(regex, "tables", &regex_include_list);
}

/***********************************************************************
Register new regex for the exclude filter.  */
static void xb_register_exclude_regex(
    /*==============*/
    const char *regex) /*!< in: regex */
{
  xb_add_regex_to_list(regex, "tables-exclude", &regex_exclude_list);
}

typedef void (*insert_entry_func_t)(const char *);

/***********************************************************************
Scan string and load filter entries from it.  */
static void xb_load_list_string(
    /*================*/
    char *list,              /*!< in: string representing a list */
    const char *delimiters,  /*!< in: delimiters of entries */
    insert_entry_func_t ins) /*!< in: callback to add entry */
{
  char *p;
  char *saveptr;

  p = strtok_r(list, delimiters, &saveptr);
  while (p) {
    ins(p);

    p = strtok_r(NULL, delimiters, &saveptr);
  }
}

/***********************************************************************
Scan file and load filter entries from it.  */
static void xb_load_list_file(
    /*==============*/
    const char *filename,    /*!< in: name of file */
    insert_entry_func_t ins) /*!< in: callback to add entry */
{
  char name_buf[NAME_LEN * 2 + 2];
  FILE *fp;

  /* read and store the filenames */
  fp = fopen(filename, "r");
  if (!fp) {
    xb::error() << "cannot open " << filename;
    exit(EXIT_FAILURE);
  }
  while (fgets(name_buf, sizeof(name_buf), fp) != NULL) {
    char *p = strchr(name_buf, '\n');
    if (p) {
      *p = '\0';
    } else {
      xb::error() << name_buf << " name is too long";
      exit(EXIT_FAILURE);
    }

    ins(name_buf);
  }

  fclose(fp);
}

static void xb_filters_init() {
  if (xtrabackup_databases) {
    xb_load_list_string(xtrabackup_databases, " \t",
                        xb_register_include_filter_entry);
  }

  if (xtrabackup_databases_file) {
    xb_load_list_file(xtrabackup_databases_file,
                      xb_register_include_filter_entry);
  }

  if (xtrabackup_databases_exclude) {
    xb_load_list_string(xtrabackup_databases_exclude, " \t",
                        xb_register_exclude_filter_entry);
  }

  if (xtrabackup_tables) {
    xb_load_list_string(xtrabackup_tables, ",", xb_register_include_regex);
  }

  if (xtrabackup_tables_file) {
    xb_load_list_file(xtrabackup_tables_file, xb_register_table);
  }

  if (xtrabackup_tables_exclude) {
    xb_load_list_string(xtrabackup_tables_exclude, ",",
                        xb_register_exclude_regex);
  }
}

static void xb_filter_hash_free(hash_table_t *hash) {
  ulint i;

  /* free the hash elements */
  for (i = 0; i < hash_get_n_cells(hash); i++) {
    xb_filter_entry_t *table;

    table = static_cast<xb_filter_entry_t *>(hash_get_first(hash, i));

    while (table) {
      xb_filter_entry_t *prev_table = table;

      table = static_cast<xb_filter_entry_t *>(
          HASH_GET_NEXT(name_hash, prev_table));

      HASH_DELETE(xb_filter_entry_t, name_hash, hash,
                  ut::hash_string(prev_table->name), prev_table);
      ut::free(prev_table);
    }
  }

  /* free hash */
  ut::delete_(hash);
}

static void xb_regex_list_free(regex_list_t *list) {
  while (list->size() > 0) {
    xb_regfree(&list->front());
    list->pop_front();
  }
}

/************************************************************************
Destroy table filters for partial backup. */
static void xb_filters_free() {
  xb_regex_list_free(&regex_include_list);
  xb_regex_list_free(&regex_exclude_list);

  if (tables_include_hash) {
    xb_filter_hash_free(tables_include_hash);
  }

  if (tables_exclude_hash) {
    xb_filter_hash_free(tables_exclude_hash);
  }

  if (databases_include_hash) {
    xb_filter_hash_free(databases_include_hash);
  }

  if (databases_exclude_hash) {
    xb_filter_hash_free(databases_exclude_hash);
  }
}

/*********************************************************************/ /**
 Normalizes init parameter values to use units we use inside InnoDB.
 @return	DB_SUCCESS or error code */
static void xb_normalize_init_values(void)
/*==========================*/
{
  srv_lock_table_size = 5 * (srv_buf_pool_size / UNIV_PAGE_SIZE);
}

/***********************************************************************
Set the open files limit. Based on set_max_open_files().

@return the resulting open files limit. May be less or more than the requested
value.  */
static uint xb_set_max_open_files(
    /*==================*/
    uint max_file_limit) /*!<in: open files limit */
{
#if defined(RLIMIT_NOFILE)
  struct rlimit rlimit;
  uint old_cur;

  if (getrlimit(RLIMIT_NOFILE, &rlimit)) {
    goto end;
  }

  old_cur = (uint)rlimit.rlim_cur;

  if (rlimit.rlim_cur == RLIM_INFINITY) {
    rlimit.rlim_cur = max_file_limit;
  }

  if (rlimit.rlim_cur >= max_file_limit) {
    max_file_limit = rlimit.rlim_cur;
    goto end;
  }

  rlimit.rlim_cur = rlimit.rlim_max = max_file_limit;

  if (setrlimit(RLIMIT_NOFILE, &rlimit)) {
    max_file_limit = old_cur; /* Use original value */
  } else {
    rlimit.rlim_cur = 0; /* Safety if next call fails */

    (void)getrlimit(RLIMIT_NOFILE, &rlimit);

    if (rlimit.rlim_cur) {
      /* If call didn't fail */
      max_file_limit = (uint)rlimit.rlim_cur;
    }
  }

end:
  return (max_file_limit);
#else
  return (0);
#endif
}

/**************************************************************************
Prints a warning for every table that uses unsupported engine and
hence will not be backed up. */
static void xb_tables_compatibility_check() {
  const char *query =
      "SELECT"
      "  CONCAT(table_schema, '/', table_name), engine "
      "FROM information_schema.tables "
      "WHERE engine NOT IN ("
      "'MyISAM', 'InnoDB', 'CSV', 'MRG_MYISAM', 'ROCKSDB') "
      "AND table_schema NOT IN ("
      "  'performance_schema', 'information_schema', "
      "  'mysql');";

  MYSQL_RES *result = xb_mysql_query(mysql_connection, query, true, true);
  MYSQL_ROW row;
  if (!result) {
    return;
  }

  ut_a(mysql_num_fields(result) == 2);
  while ((row = mysql_fetch_row(result))) {
    if (!check_if_skip_table(row[0])) {
      *strchr(row[0], '/') = '.';
      xb::warn() << row[0] << " uses engine " << row[1]
                 << "and will not be backed up.";
    }
  }

  mysql_free_result(result);
}

static void init_mysql_environment() {
  ulong server_start_time = time(nullptr);

  randominit(&sql_rand, server_start_time, server_start_time / 2);

  mysql_mutex_init(PSI_NOT_INSTRUMENTED, &LOCK_status, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(PSI_NOT_INSTRUMENTED, &LOCK_global_system_variables,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(PSI_NOT_INSTRUMENTED, &LOCK_sql_rand, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(PSI_NOT_INSTRUMENTED, &LOCK_keyring_operations,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(PSI_NOT_INSTRUMENTED, &LOCK_replica_list,
                   MY_MUTEX_INIT_FAST);

  Srv_session::module_init();

  Global_THD_manager::create_instance();

  my_thread_stack_size = DEFAULT_THREAD_STACK;
  my_default_lc_messages = &my_locale_en_US;
  global_system_variables.collation_connection = default_charset_info;
  global_system_variables.character_set_results = default_charset_info;
  global_system_variables.character_set_client = default_charset_info;
  global_system_variables.lc_messages = my_default_lc_messages;

  table_cache_instances = Table_cache_manager::DEFAULT_MAX_TABLE_CACHES;

  table_def_init();
  xa::Transaction_cache::initialize();

  mdl_init();
  component_infrastructure_init();
}

static void cleanup_mysql_environment() {
  Global_THD_manager::destroy_instance();

  xa::Transaction_cache::dispose();
  table_def_free();
  mdl_destroy();
  Srv_session::module_deinit();
  component_infrastructure_deinit();

  mysql_mutex_destroy(&LOCK_status);
  mysql_mutex_destroy(&LOCK_global_system_variables);
  mysql_mutex_destroy(&LOCK_sql_rand);
  mysql_mutex_destroy(&LOCK_keyring_operations);
  mysql_mutex_destroy(&LOCK_replica_list);
}

void xtrabackup_backup_func(void) {
  MY_STAT stat_info;
  uint i;
  uint count;
  ib_mutex_t count_mutex;
  data_thread_ctxt_t *data_threads;

  recv_is_making_a_backup = true;
  bool data_copying_error = false;
  std::shared_ptr<xb::backup::dd_space_ids> xb_dd_spaces;
  init_mysql_environment();

  if (opt_dump_innodb_buffer_pool) {
    dump_innodb_buffer_pool(mysql_connection);
  }

#ifdef USE_POSIX_FADVISE
  xb::info() << "uses posix_fadvise().";
#endif

  /* cd to datadir */

  if (my_setwd(mysql_real_data_home, MYF(MY_WME))) {
    xb::error() << "cannot my_setwd " << mysql_real_data_home;
    exit(EXIT_FAILURE);
  }
  xb::info() << "cd to " << mysql_real_data_home;

  xb::info() << "open files limit requested " << xb_open_files_limit
             << ", set to " << xb_set_max_open_files(xb_open_files_limit);

  mysql_data_home = mysql_data_home_buff;
  mysql_data_home[0] = FN_CURLIB;  // all paths are relative from here
  mysql_data_home[1] = 0;

  srv_read_only_mode = true;

  srv_backup_mode = true;

  if (opt_lock_ddl == LOCK_DDL_ON) {
    xb_dd_spaces = xb::backup::build_space_id_set(mysql_connection);
    ut_ad(xb_dd_spaces->size());
  } else if (opt_lock_ddl == LOCK_DDL_REDUCED) {
    ddl_tracker = new ddl_tracker_t;
  }

  /* We can safely close files if we don't allow DDL during the
  backup */
  srv_close_files = xb_close_files || opt_lock_ddl == LOCK_DDL_ON;

  if (xb_close_files)
    xb::warn()
        << "close-files specified. Use it "
           "at your own risk. If there are DDL operations like table DROP "
           "TABLE "
           "or RENAME TABLE during the backup, inconsistent backup will be "
           "produced.";

  /* initialize components */
  if (innodb_init_param()) exit(EXIT_FAILURE);

  xb_normalize_init_values();

#ifndef _WIN32
  srv_unix_file_flush_method =
      static_cast<srv_unix_flush_t>(innodb_flush_method);
  ut_ad(innodb_flush_method <= SRV_UNIX_O_DIRECT_NO_FSYNC);
#else
  srv_win_file_flush_method = static_cast<srv_win_flush_t>(innodb_flush_method);
  ut_ad(innodb_flush_method <= SRV_WIN_IO_NORMAL);
#endif
  switch (srv_unix_file_flush_method) {
    case SRV_UNIX_O_DIRECT:
      xb::info() << "using O_DIRECT";
      break;
    case SRV_UNIX_O_DIRECT_NO_FSYNC:
      xb::info() << "using O_DIRECT_NO_FSYNC";
      break;
    default:
      break;
  }

  if (srv_buf_pool_size >= 1000 * 1024 * 1024) {
    /* Here we still have srv_pool_size counted
    in kilobytes (in 4.0 this was in bytes)
    srv_boot() converts the value to
    pages; if buffer pool is less than 1000 MB,
    assume fewer threads. */
    srv_max_n_threads = 50000;

  } else if (srv_buf_pool_size >= 8 * 1024 * 1024) {
    srv_max_n_threads = 10000;
  } else {
    srv_max_n_threads = 1000; /* saves several MB of memory,
                              especially in 64-bit
                              computers */
  }

  os_event_global_init();
  srv_general_init();
  ut_crc32_init();
  crc_init();

  xb_filters_init();
  if (opt_component_keyring_file_config != nullptr) {
    xb::warn() << "--component-keyring-file-config will be ignored "
                  "for --backup operation";
  }

  if (opt_component_keyring_config != nullptr) {
    xb::warn() << "--component-keyring-config will be ignored "
                  "for --backup operation";
  }

  if (have_keyring_component &&
      !xtrabackup::components::keyring_init_online(mysql_connection)) {
    xb::error() << "failed to init keyring component";
    exit(EXIT_FAILURE);
  }

  if (!xtrabackup::components::keyring_component_initialized &&
      !xb_keyring_init_for_backup(mysql_connection)) {
    xb::error() << "failed to init keyring plugin";
    exit(EXIT_FAILURE);
  }
  xtrabackup::components::inititialize_service_handles();

  if (opt_tables_compatibility_check) {
    xb_tables_compatibility_check();
  }

  srv_is_being_started = true;

  os_create_block_cache();

  if (!xb_fil_io_init()) {
    exit(EXIT_FAILURE);
  }

  Redo_Log_Data_Manager redo_mgr;

  dict_persist_init();

  srv_log_n_files = (ulint)innobase_log_files_in_group;
  srv_log_file_size = (ulint)innobase_log_file_size;

  clone_init();

  lock_sys_create(srv_lock_table_size);

  redo_mgr.set_copy_interval(xtrabackup_log_copy_interval);
  if (!redo_mgr.init()) {
    exit(EXIT_FAILURE);
  }

  debug_sync_point("after_redo_log_manager_init");

  if (!validate_options("my", orig_argc, orig_argv)) {
    exit(EXIT_FAILURE);
  }
  /* create extra LSN dir if it does not exist. */
  if (xtrabackup_extra_lsndir &&
      !my_stat(xtrabackup_extra_lsndir, &stat_info, MYF(0)) &&
      (my_mkdir(xtrabackup_extra_lsndir, 0777, MYF(0)) < 0)) {
    xb::error() << "cannot mkdir: " << my_errno() << " "
                << xtrabackup_extra_lsndir;
    exit(EXIT_FAILURE);
  }

  if (xtrabackup_fifo_streams_set && xtrabackup_fifo_dir != NULL) {
    xtrabackup_target_dir = xtrabackup_fifo_dir;
  }

  if (xtrabackup_fifo_streams_set && !xtrabackup_stream) {
    xb::info() << "Option --fifo-streams require xbstream format. Setting "
                  "--stream to xbstream.";
    xtrabackup_stream_fmt = XB_STREAM_FMT_XBSTREAM;
    xtrabackup_stream = true;
  }

  if (xtrabackup_fifo_streams_set &&
      xtrabackup_fifo_streams > xtrabackup_parallel) {
    xb::info() << "Option --fifo-streams set higer than --parallel. "
                  "Adjusting --parallel to "
               << xtrabackup_fifo_streams;
    xtrabackup_parallel = xtrabackup_fifo_streams;
  }

  /* create target dir if not exist */
  if (!my_stat(xtrabackup_target_dir, &stat_info, MYF(0)) &&
      (my_mkdir(xtrabackup_target_dir, 0777, MYF(0)) < 0)) {
    xb::error() << "cannot mkdir: " << my_errno() << " "
                << xtrabackup_target_dir;
    exit(EXIT_FAILURE);
  }

  xtrabackup_init_datasinks();

  if (!select_history()) {
    exit(EXIT_FAILURE);
  }

  io_ticket = xtrabackup_throttle;
  wait_throttle = os_event_create();
  os_thread_create(PFS_NOT_INSTRUMENTED, 0, io_watching_thread).start();

  if (!redo_mgr.start()) {
    exit(EXIT_FAILURE);
  }

  Tablespace_map::instance().scan(mysql_connection);

  /* Populate fil_system with tablespaces to copy */
  dberr_t err = xb_load_tablespaces();
  if (err != DB_SUCCESS) {
    xb::error() << "xb_load_tablespaces() failed with error code " << err;
    exit(EXIT_FAILURE);
  }

  debug_sync_point("xtrabackup_suspend_at_start");

  lsn_t page_tracking_start_lsn = 0;
  if (opt_page_tracking &&
      pagetracking::start(mysql_connection, &page_tracking_start_lsn)) {
    xb::info() << "pagetracking is started on the server with LSN "
               << page_tracking_start_lsn;
  }

  if (xtrabackup_incremental) {
    incremental_start_checkpoint_lsn = redo_mgr.get_start_checkpoint_lsn();
    if (!xtrabackup_incremental_force_scan && opt_page_tracking) {
      changed_page_tracking = pagetracking::init(
          redo_mgr.get_start_checkpoint_lsn(), mysql_connection);
    }

    if (changed_page_tracking) {
      xb::info() << "Using pagetracking feature for incremental backup";
    } else {
      xb::info() << "using the full scan for incremental backup";
    }
  }

  ut_a(xtrabackup_parallel > 0);

  if (xtrabackup_parallel > 1) {
    xb::info() << "Starting " << xtrabackup_parallel
               << " threads for parallel data files transfer";
  }

  auto it = datafiles_iter_new(xb_dd_spaces);
  if (it == NULL) {
    xb::error() << "datafiles_iter_new() failed.";
    exit(EXIT_FAILURE);
  }

  /* Create data copying threads */
  data_threads = (data_thread_ctxt_t *)ut::malloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY,
      sizeof(data_thread_ctxt_t) * xtrabackup_parallel);
  count = xtrabackup_parallel;
  mutex_create(LATCH_ID_XTRA_COUNT_MUTEX, &count_mutex);

  for (i = 0; i < (uint)xtrabackup_parallel; i++) {
    data_threads[i].it = it;
    data_threads[i].num = i + 1;
    data_threads[i].count = &count;
    data_threads[i].count_mutex = &count_mutex;
    data_threads[i].error = &data_copying_error;
    os_thread_create(PFS_NOT_INSTRUMENTED, i, data_copy_thread_func,
                     data_threads + i)
        .start();
  }

  /* Wait for threads to exit */
  while (1) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    mutex_enter(&count_mutex);
    if (count == 0) {
      mutex_exit(&count_mutex);
      break;
    }
    if (redo_mgr.is_error()) {
      xb::error() << "log copying failed.";
      exit(EXIT_FAILURE);
    }
    mutex_exit(&count_mutex);
  }

  mutex_free(&count_mutex);
  ut::free(data_threads);
  datafiles_iter_free(it);

  if (data_copying_error) {
    exit(EXIT_FAILURE);
  }

  if (changed_page_tracking) {
    pagetracking::deinit(changed_page_tracking);
  }

  Backup_context backup_ctxt;
  backup_ctxt.redo_mgr = &redo_mgr;
  if (!backup_start(backup_ctxt)) {
    exit(EXIT_FAILURE);
  }

  if (opt_debug_sleep_before_unlock) {
    xb::info() << "Debug sleep for " << opt_debug_sleep_before_unlock
               << " seconds";
    std::this_thread::sleep_for(
        std::chrono::seconds(opt_debug_sleep_before_unlock));
  }

  if (redo_mgr.is_error()) {
    xb::error() << "log copying failed.";
    exit(EXIT_FAILURE);
  }

  if (!redo_mgr.stop_at(log_status.lsn, log_status.lsn_checkpoint)) {
    xb::error() << "Error stopping copy thread at LSN " << log_status.lsn;
    exit(EXIT_FAILURE);
  }

  io_watching_thread_stop = true;

  /* smart memory estimation */
  if (xtrabackup_estimate_memory) {
    auto redo_memory_requirements = xtrabackup::recv_backup_heap_used();
    redo_memory = redo_memory_requirements.first;
    redo_frames = redo_memory_requirements.second;
  }

  if (!validate_missing_encryption_tablespaces()) {
    exit(EXIT_FAILURE);
  }

  if (!xtrabackup_incremental) {
    strcpy(metadata_type_str, "full-backuped");
    metadata_from_lsn = 0;
  } else {
    strcpy(metadata_type_str, "incremental");
    metadata_from_lsn = incremental_lsn;
  }
  metadata_to_lsn = redo_mgr.get_last_checkpoint_lsn();
  metadata_last_lsn = redo_mgr.get_stop_lsn();

  if (!xtrabackup_stream_metadata(ds_meta)) {
    xb::error() << "failed to stream metadata.";
    exit(EXIT_FAILURE);
  }

  if (!backup_finish(backup_ctxt)) {
    exit(EXIT_FAILURE);
  }

  if (xtrabackup_extra_lsndir) {
    char filename[FN_REFLEN];

    sprintf(filename, "%s/%s", xtrabackup_extra_lsndir,
            XTRABACKUP_METADATA_FILENAME);
    if (!xtrabackup_write_metadata(filename)) {
      xb::error() << "failed to write metadata to " << filename;
      exit(EXIT_FAILURE);
    }

    sprintf(filename, "%s/%s", xtrabackup_extra_lsndir, XTRABACKUP_INFO);
    if (!xtrabackup_write_info(filename)) {
      xb::error() << "failed to write info to " << filename;
      exit(EXIT_FAILURE);
    }
  }

  if (opt_lock_ddl_per_table) {
    mdl_unlock_all();
  }

  Tablespace_map::instance().serialize(ds_data);

  if (opt_transition_key != NULL || opt_generate_transition_key) {
    if (!xb_tablespace_keys_dump(
            ds_data, opt_transition_key,
            opt_transition_key != NULL ? strlen(opt_transition_key) : 0)) {
      xb::error() << "failed to dump tablespace keys.";
      exit(EXIT_FAILURE);
    }
  }

  xtrabackup_destroy_datasinks();

  if (wait_throttle) {
    /* wait for io_watching_thread completion */
    while (io_watching_thread_running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    os_event_destroy(wait_throttle);
    wait_throttle = NULL;
  }

  xb::info() << "Transaction log of lsn ("
             << redo_mgr.get_start_checkpoint_lsn() << ") to ("
             << redo_mgr.get_scanned_lsn() << ") was copied.";

  xb_filters_free();

  xb_data_files_close();

  recv_sys_free();

  recv_sys_close();

  clone_free();

  trx_pool_close();

  lock_sys_close();

  os_thread_close();

  row_mysql_close();

  redo_mgr.close();

  dict_persist_close();

  sync_check_close();

  os_event_global_destroy();

  xb_keyring_shutdown();

  cleanup_mysql_environment();

  delete ddl_tracker;
}

/* ================= prepare ================= */

void update_log_temp_checkpoint(byte *buf, lsn_t lsn) {
  /* Overwrite the both checkpoint area. */
  mach_write_to_8(buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN, lsn);
  mach_write_to_8(buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN, lsn);

  log_block_set_checksum(buf, log_block_calc_checksum_crc32(buf));
  log_block_set_checksum(buf + LOG_CHECKPOINT_1,
                         log_block_calc_checksum_crc32(buf + LOG_CHECKPOINT_1));
  log_block_set_checksum(buf + LOG_CHECKPOINT_2,
                         log_block_calc_checksum_crc32(buf + LOG_CHECKPOINT_2));
}

static bool xtrabackup_init_temp_log(void) {
  pfs_os_file_t src_file = XB_FILE_UNDEFINED;
  char src_path[FN_REFLEN];
  char dst_path[FN_REFLEN];
  char redo_folder[FN_REFLEN];
  MY_STAT stat_info;
  bool success;
  Log_format log_format;
  ulint field;
  byte *log_buf;

  uint64_t file_size;

  lsn_t max_lsn = 0;
  lsn_t checkpoint_lsn = 0;
  lsn_t start_lsn = 0;

  bool checkpoint_found;

  IORequest read_request(IORequest::READ);
  IORequest write_request(IORequest::WRITE);

  log_buf = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, UNIV_PAGE_SIZE_MAX * 128));
  if (log_buf == NULL) {
    goto error;
  }

  if (!xtrabackup_incremental_dir) {
    sprintf(dst_path, "%s/%s/#ib_redo0", xtrabackup_target_dir,
            LOG_DIRECTORY_NAME);
    sprintf(src_path, "%s/%s", xtrabackup_target_dir, XB_LOG_FILENAME);
    sprintf(redo_folder, "%s/%s", xtrabackup_target_dir, LOG_DIRECTORY_NAME);
  } else {
    sprintf(dst_path, "%s/%s/#ib_redo0", xtrabackup_incremental_dir,
            LOG_DIRECTORY_NAME);
    sprintf(src_path, "%s/%s", xtrabackup_incremental_dir, XB_LOG_FILENAME);
    sprintf(redo_folder, "%s/%s", xtrabackup_incremental_dir,
            LOG_DIRECTORY_NAME);
  }

  /* create #innodb_redo dir if not exist */
  if (!my_stat(redo_folder, &stat_info, MYF(0)) &&
      (my_mkdir(redo_folder, 0777, MYF(0)) < 0)) {
    xb::error() << "cannot mkdir: " << my_errno() << " " << redo_folder;
    exit(EXIT_FAILURE);
  }

  Fil_path::normalize(dst_path);
  Fil_path::normalize(src_path);

  src_file = os_file_create_simple_no_error_handling(
      0, src_path, OS_FILE_OPEN, OS_FILE_READ_WRITE, srv_read_only_mode,
      &success);

  if (!success) {
    /* The following call prints an error message */
    os_file_get_last_error(true);

    xb::warn() << "cannot open " << src_path << " will try to find.";

    success = xb_log_files_validate_creators(!xtrabackup_incremental_dir
                                                 ? xtrabackup_target_dir
                                                 : xtrabackup_incremental_dir);

    if (success) {
      if (log_buf != NULL) {
        ut::free(log_buf);
      }
      return false;
    }

    xb::fatal_or_error(UT_LOCATION_HERE) << "cannot find " << src_path;

    os_file_close(src_file);
    src_file = XB_FILE_UNDEFINED;

    goto error;
  }

  file_size = os_file_get_size(src_file);

  /* read log file header */
  success = os_file_read(read_request, dst_path, src_file, log_buf, 0,
                         LOG_FILE_HDR_SIZE);
  if (!success) {
    goto error;
  }

  log_format =
      static_cast<Log_format>(mach_read_from_4(log_buf + LOG_HEADER_FORMAT));
  xb_log_detected_format = log_format;

  if (ut_memcmp(log_buf + LOG_HEADER_CREATOR, (byte *)LOG_HEADER_CREATOR_PXB,
                (sizeof LOG_HEADER_CREATOR_PXB) - 1) != 0) {
    if (xtrabackup_incremental_dir) {
      xb::error() << "xtrabackup_logfile was already used "
                     "to '--prepare'.";
      goto error;
    }
    xb::info() << "xtrabackup_logfile was already used "
                  "to '--prepare'.";
    goto skip_modify;
  }

  if (log_format < Log_format::CURRENT) {
    xb::error() << "Unsupported redo log format " << to_int(log_format);
    xb::error() << "Please use an older version of Xtrabackup matching your "
                   "redo log format for this database.";
    goto error;
  }

  checkpoint_found = false;

  /* read last checkpoint lsn */
  for (field = LOG_CHECKPOINT_1; field <= LOG_CHECKPOINT_2;
       field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {
    /* InnoDB using CRC32 by default since 5.7.9+ */
    if (log_block_get_checksum(log_buf + field) ==
        log_block_calc_checksum_crc32(log_buf + field)) {
      if (!innodb_checksum_algorithm_specified) {
        srv_checksum_algorithm = SRV_CHECKSUM_ALGORITHM_CRC32;
      }
    } else {
      goto not_consistent;
    }

    checkpoint_lsn = mach_read_from_8(log_buf + field + LOG_CHECKPOINT_LSN);

    if (checkpoint_lsn >= max_lsn) {
      max_lsn = checkpoint_lsn;
      checkpoint_found = true;
    }
  not_consistent:;
  }

  if (!checkpoint_found) {
    xb::error() << "No valid checkpoint found.";
    goto error;
  }

  /* write start_lsn header */
  start_lsn = ut_uint64_align_down(max_lsn, OS_FILE_LOG_BLOCK_SIZE);
  mach_write_to_8(log_buf + LOG_HEADER_START_LSN, start_lsn);

  update_log_temp_checkpoint(log_buf, max_lsn);

  success = os_file_write(write_request, src_path, src_file, log_buf, 0,
                          LOG_FILE_HDR_SIZE);
  if (!success) {
    goto error;
  }

  /* expand file size (9/8) and align to UNIV_PAGE_SIZE_MAX */

  if (file_size % UNIV_PAGE_SIZE_MAX) {
    ulint n = UNIV_PAGE_SIZE_MAX - (ulint)(file_size % UNIV_PAGE_SIZE_MAX);
    memset(log_buf, 0, UNIV_PAGE_SIZE_MAX);
    success =
        os_file_write(write_request, src_path, src_file, log_buf, file_size, n);
    if (!success) {
      goto error;
    }

    file_size = os_file_get_size(src_file);
  }

  /* TODO: We should judge whether the file is already expanded or not... */
  {
    ulint expand;

    memset(log_buf, 0, UNIV_PAGE_SIZE_MAX * 128);
    expand = (ulint)(file_size / UNIV_PAGE_SIZE_MAX / 8);

    for (; expand > 128; expand -= 128) {
      success = os_file_write(write_request, src_path, src_file, log_buf,
                              file_size, UNIV_PAGE_SIZE_MAX * 128);
      if (!success) {
        goto error;
      }
      file_size += UNIV_PAGE_SIZE_MAX * 128;
    }

    if (expand) {
      success = os_file_write(write_request, src_path, src_file, log_buf,
                              file_size, expand * UNIV_PAGE_SIZE_MAX);
      if (!success) {
        goto error;
      }
      file_size += UNIV_PAGE_SIZE_MAX * expand;
    }
  }

  /* make larger than 128 * UNIV_PAGE_SIZE_MAX */
  if (file_size < 128 * UNIV_PAGE_SIZE_MAX) {
    memset(log_buf, 0, UNIV_PAGE_SIZE_MAX);
    while (file_size < 128 * UNIV_PAGE_SIZE_MAX) {
      success = os_file_write(write_request, src_path, src_file, log_buf,
                              file_size, UNIV_PAGE_SIZE_MAX);
      if (!success) {
        goto error;
      }
      file_size += UNIV_PAGE_SIZE_MAX;
    }
    file_size = os_file_get_size(src_file);
  }

  xb::info() << "xtrabackup_logfile detected: size=" << file_size
             << ", start_lsn=(" << max_lsn << ")";

  os_file_close(src_file);
  src_file = XB_FILE_UNDEFINED;

  /* fake InnoDB */
  innobase_log_files_in_group_save = innobase_log_files_in_group;
  srv_log_group_home_dir_save = srv_log_group_home_dir;
  innobase_log_file_size_save = innobase_log_file_size;
  srv_log_group_home_dir = NULL;
  innobase_log_file_size = file_size;
  innobase_log_files_in_group = 1;

  srv_thread_concurrency = 0;

  /* rename 'xtrabackup_logfile' to '#ib_redo0' */
  success = os_file_rename(0, src_path, dst_path);
  if (!success) {
    goto error;
  }
  xtrabackup_logfile_is_renamed = true;

  if (log_buf != NULL) {
    ut::free(log_buf);
  }

  return (false);

skip_modify:
  if (log_buf != NULL) {
    ut::free(log_buf);
  }
  os_file_close(src_file);
  src_file = XB_FILE_UNDEFINED;
  return (false);

error:
  if (log_buf != NULL) {
    ut::free(log_buf);
  }
  if (src_file != XB_FILE_UNDEFINED) os_file_close(src_file);
  return (true); /*ERROR*/
}

/***********************************************************************
Generates path to the meta file path from a given path to an incremental .delta
by replacing trailing ".delta" with ".meta", or returns error if 'delta_path'
does not end with the ".delta" character sequence.
@return true on success, false on error. */
static bool get_meta_path(
    const char *delta_path, /* in: path to a .delta file */
    char *meta_path)        /* out: path to the corresponding .meta
                            file */
{
  size_t len = strlen(delta_path);

  if (len <= 6 || strcmp(delta_path + len - 6, ".delta")) {
    return false;
  }
  memcpy(meta_path, delta_path, len - 6);
  strcpy(meta_path + len - 6, XB_DELTA_INFO_SUFFIX);

  return true;
}

/****************************************************************/ /**
 Create a new tablespace on disk and return the handle to its opened
 file. Code adopted from fiL_ibd_create with
 the main difference that only disk file is created without updating
 the InnoDB in-memory dictionary data structures.

 @return true on success, false on error.  */
static bool xb_space_create_file(
    /*==================*/
    const char *path,    /*!<in: path to tablespace */
    ulint space_id,      /*!<in: space id */
    ulint flags,         /*!<in: tablespace flags */
    fil_space_t *space,  /*!<in: space object */
    pfs_os_file_t *file) /*!<out: file handle */
{
  const ulint size = FIL_IBD_FILE_INITIAL_SIZE;
  dberr_t err;
  byte *buf2;
  byte *page;
  bool success;

  IORequest write_request(IORequest::WRITE);

  ut_ad(!fsp_is_system_or_temp_tablespace(space_id));
  ut_ad(!srv_read_only_mode);
  ut_a(space_id < dict_sys_t::s_log_space_id);
  ut_a(fsp_flags_is_valid(flags));

  /* Create the subdirectories in the path, if they are
  not there already. */
  err = os_file_create_subdirs_if_needed(path);
  if (err != DB_SUCCESS) {
    return (false);
  }

  *file = os_file_create(
      innodb_data_file_key, path, OS_FILE_CREATE | OS_FILE_ON_ERROR_NO_EXIT,
      OS_FILE_NORMAL, OS_DATA_FILE, srv_read_only_mode, &success);

  if (!success) {
    /* The following call will print an error message */
    ulint error = os_file_get_last_error(true);

    ib::error() << "Cannot create file '" << path << "'";

    if (error == OS_FILE_ALREADY_EXISTS) {
      ib::error() << "The file '" << path
                  << "'"
                     " already exists though the"
                     " corresponding table did not exist"
                     " in the InnoDB data dictionary."
                     " Have you moved InnoDB .ibd files"
                     " around without using the SQL commands"
                     " DISCARD TABLESPACE and IMPORT TABLESPACE,"
                     " or did mysqld crash in the middle of"
                     " CREATE TABLE?"
                     " You can resolve the problem by removing"
                     " the file '"
                  << path << "' under the 'datadir' of MySQL.";

      return (false);
    }

    return (false);
  }

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
  if (fil_fusionio_enable_atomic_write(*file)) {
    /* This is required by FusionIO HW/Firmware */
    int ret = posix_fallocate(file->m_file, 0, size * UNIV_PAGE_SIZE);

    if (ret != 0) {
      ib::error() << "posix_fallocate(): Failed to preallocate"
                     " data for file "
                  << path << ", desired size " << size * UNIV_PAGE_SIZE
                  << " Operating system error number " << ret
                  << ". Check"
                     " that the disk is not full or a disk quota"
                     " exceeded. Make sure the file system supports"
                     " this function. Some operating system error"
                     " numbers are described at " REFMAN
                     " operating-system-error-codes.html";

      success = false;
    } else {
      success = true;
    }
  } else {
    success = os_file_set_size(path, *file, 0, size * UNIV_PAGE_SIZE, false);
  }
#else
  success = os_file_set_size(path, *file, 0, size * UNIV_PAGE_SIZE, false);
#endif /* !NO_FALLOCATE && UNIV_LINUX */

  if (!success) {
    os_file_close(*file);
    os_file_delete(innodb_data_file_key, path);
    return (false);
  }

  /* printf("Creating tablespace %s id %lu\n", path, space_id); */

  /* We have to write the space id to the file immediately and flush the
  file to disk. This is because in crash recovery we must be aware what
  tablespaces exist and what are their space id's, so that we can apply
  the log records to the right file. It may take quite a while until
  buffer pool flush algorithms write anything to the file and flush it to
  disk. If we would not write here anything, the file would be filled
  with zeros from the call of os_file_set_size(), until a buffer pool
  flush would write to it. */

  buf2 = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, 3 * UNIV_PAGE_SIZE));
  /* Align the memory for file i/o if we might have O_DIRECT set */
  page = static_cast<byte *>(ut_align(buf2, UNIV_PAGE_SIZE));

  memset(page, '\0', UNIV_PAGE_SIZE);

  /* Add the UNIV_PAGE_SIZE to the table flags and write them to the
  tablespace header. */
  flags = fsp_flags_set_page_size(flags, univ_page_size);
  fsp_header_init_fields(page, space_id, flags);
  mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

  const page_size_t page_size(flags);

  if (!page_size.is_compressed()) {
    buf_flush_init_for_writing(NULL, page, NULL, 0,
                               fsp_is_checksum_disabled(space_id), true);
    success = os_file_write(write_request, path, *file, page, 0,
                            page_size.physical());
  } else {
    page_zip_des_t page_zip;

    page_zip_set_size(&page_zip, page_size.physical());
    page_zip.data = page + UNIV_PAGE_SIZE;
#ifdef UNIV_DEBUG
    page_zip.m_start =
#endif /* UNIV_DEBUG */
        page_zip.m_end = (page_zip.m_nonempty = (page_zip.n_blobs = 0));

    buf_flush_init_for_writing(NULL, page, &page_zip, 0,
                               fsp_is_checksum_disabled(space_id), true);
    success = os_file_write(write_request, path, *file, page_zip.data, 0,
                            page_size.physical());
  }

  ut::free(buf2);

  if (!success) {
    ib::error() << "Could not write the first page to"
                << " tablespace '" << path << "'";
    os_file_close(*file);
    os_file_delete(innodb_data_file_key, path);
    return (false);
  }

  success = os_file_flush(*file);

  if (!success) {
    ib::error() << "File flush of tablespace '" << path << "' failed";
    os_file_close(*file);
    os_file_delete(innodb_data_file_key, path);
    return (false);
  }

  if (fil_node_create(path, size, space, false, false) == nullptr) {
    ib::fatal(UT_LOCATION_HERE) << "Unable to add tablespace node '" << path
                                << "' to the tablespace cache.";
  }

  return (true);
}

/* Retreive space_id from page 0 of tablespace
@param[in] file_name tablespace file path
@return space_id or SPACE_UNKOWN */
static space_id_t get_space_id_from_page_0(const char *file_name) {
  bool ok;
  space_id_t space_id = SPACE_UNKNOWN;

  auto buf = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, 2 * srv_page_size));

  auto file = os_file_create_simple_no_error_handling(
      0, file_name, OS_FILE_OPEN, OS_FILE_READ_ONLY, srv_read_only_mode, &ok);

  if (ok) {
    auto *page = static_cast<buf_frame_t *>(ut_align(buf, srv_page_size));

    IORequest request(IORequest::READ);
    dberr_t err =
        os_file_read_first_page(request, file_name, file, page, UNIV_PAGE_SIZE);

    if (err == DB_SUCCESS) {
      space_id = fsp_header_get_space_id(page);
    } else {
      xb::error() << "error reading first page on file" << file_name;
    }
    os_file_close(file);

  } else {
    xb::error() << "Cannot open file to read first page " << file_name;
  }

  ut::free(buf);

  return (space_id);
}

/***********************************************************************
Searches for matching tablespace file for given .delta file and space_id
in given directory. When matching tablespace found, renames it to match the
name of .delta file. If there was a tablespace with matching name and
mismatching ID, renames it to xtrabackup_tmp_#ID.ibd. If there was no
matching file, creates a new tablespace.
@return file handle of matched or created file */
static pfs_os_file_t xb_delta_open_matching_space(
    const char *dbname,   /* in: path to destination database dir */
    const char *name,     /* in: name of delta file (without .delta) */
    space_id_t space_id,  /* in: space id of delta file */
    ulint space_flags,    /* in: space flags of delta file */
    ulint zip_size,       /* in: zip_size of tablespace */
    char *real_name,      /* out: full path of destination file */
    size_t real_name_len, /* out: buffer size for real_name */
    bool *success)        /* out: indicates error. true = success */
{
  char dest_dir[FN_REFLEN * 2 + 1];
  char dest_space_name[FN_REFLEN * 2 + 1];
  bool ok;
  pfs_os_file_t file = XB_FILE_UNDEFINED;
  xb_filter_entry_t *table;
  fil_space_t *fil_space;
  space_id_t f_space_id;
  os_file_create_t create_option = OS_FILE_OPEN;

  *success = false;

  if (dbname) {
    snprintf(dest_dir, sizeof(dest_dir), "%s/%s", xtrabackup_target_dir,
             dbname);
    Fil_path::normalize(dest_dir);

    snprintf(dest_space_name, sizeof(dest_space_name), "%s/%s", dbname, name);
  } else {
    snprintf(dest_dir, sizeof(dest_dir), "%s", xtrabackup_target_dir);
    Fil_path::normalize(dest_dir);

    snprintf(dest_space_name, sizeof(dest_space_name), "%s", name);
  }

  if (snprintf(real_name, real_name_len - 1, "%s/%s", xtrabackup_target_dir,
               dest_space_name) > (int)(real_name_len - 1)) {
    xb::error() << "Cannot format real_name.";
    return file;
  }
  Fil_path::normalize(real_name);
  /* Truncate ".ibd" */
  dest_space_name[strlen(dest_space_name) - 4] = '\0';

  /* Create the database directory if it doesn't exist yet */
  if (!os_file_create_directory(dest_dir, false)) {
    xb::error() << "cannot create dir " << dest_dir;
    return file;
  }

  /* remember space name used by incremental prepare. This hash is later used to
  detect the dropped tablespaces and remove them. Check rm_if_not_found() */
  table = static_cast<xb_filter_entry_t *>(ut::malloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY,
      sizeof(xb_filter_entry_t) + strlen(dest_space_name) + 1));

  table->name = ((char *)table) + sizeof(xb_filter_entry_t);
  strcpy(table->name, dest_space_name);
  HASH_INSERT(xb_filter_entry_t, name_hash, inc_dir_tables_hash,
              ut::hash_string(table->name), table);

  if (space_id != SPACE_UNKNOWN && !fsp_is_ibd_tablespace(space_id)) {
    /* since undo tablespaces cannot be renamed, we must either open existing
    with the same name or create new one */
    if (fsp_is_undo_tablespace(space_id)) {
      bool exists;
      os_file_type_t type;
      os_file_status(real_name, &exists, &type);

      if (exists) {
        f_space_id = get_space_id_from_page_0(real_name);

        if (f_space_id == SPACE_UNKNOWN) {
          xb::error() << "could not find space id from file " << real_name;
          goto exit;
        }

        if (space_id == f_space_id) {
          goto found;
        }

        /* space_id of undo tablespace from incremental is different from the
        full backup. Rename the existing undo tablespace to a temporary name and
        create undo tablespace file with new space_id */
        char tmpname[FN_REFLEN];
        snprintf(tmpname, FN_REFLEN, "./xtrabackup_tmp_#" SPACE_ID_PF ".ibu",
                 f_space_id);

        char *oldpath, *space_name;
        bool res =
            fil_space_read_name_and_filepath(f_space_id, &space_name, &oldpath);
        ut_a(res);
        xb::info() << "Renaming " << dest_space_name << " to " << tmpname
                   << ".ibu";

        ut_a(os_file_status(oldpath, &exists, &type));

        if (!fil_rename_tablespace(f_space_id, oldpath, tmpname, tmpname)) {
          xb::error() << "Cannot rename " << dest_space_name << "to "
                      << tmpname;
          ut::free(oldpath);
          ut::free(space_name);
          goto exit;
        }
        ut::free(oldpath);
        ut::free(space_name);
      }

      /* either file doesn't exist or it has been renamed above */
      fil_space = fil_space_create(dest_space_name, space_id, space_flags,
                                   FIL_TYPE_TABLESPACE);
      if (fil_space == nullptr) {
        xb::error() << "Cannot create tablespace " << dest_space_name;
        goto exit;
      }
      *success = xb_space_create_file(real_name, space_id, space_flags,
                                      fil_space, &file);
      goto exit;
    }
    goto found;
  }

  f_space_id = fil_space_get_id_by_name(dest_space_name);

  if (f_space_id != SPACE_UNKNOWN) {
    if (f_space_id == space_id || space_id == SPACE_UNKNOWN) {
      /* we found matching space */
      goto found;
    } else {
      char tmpname[FN_REFLEN];
      bool exists;
      os_file_type_t type;

      if (dbname != NULL) {
        snprintf(tmpname, FN_REFLEN, "%s/xtrabackup_tmp_#" SPACE_ID_PF, dbname,
                 f_space_id);
      } else {
        snprintf(tmpname, FN_REFLEN, "./xtrabackup_tmp_#" SPACE_ID_PF,
                 f_space_id);
      }

      char *oldpath, *space_name;

      bool res =
          fil_space_read_name_and_filepath(f_space_id, &space_name, &oldpath);
      ut_a(res);

      xb::info() << "Renaming " << dest_space_name << " to " << tmpname
                 << ".ibd";

      ut_a(os_file_status(oldpath, &exists, &type));

      if (exists &&
          !fil_rename_tablespace(f_space_id, oldpath, tmpname, NULL)) {
        xb::error() << "Cannot rename " << dest_space_name << " to " << tmpname;
        ut::free(oldpath);
        ut::free(space_name);
        goto exit;
      }
      ut::free(oldpath);
      ut::free(space_name);
    }
  }

  if (space_id == SPACE_UNKNOWN) {
    xb::error() << "Cannot handle DDL operation on tablespace "
                << dest_space_name;
    exit(EXIT_FAILURE);
  }

  fil_space = fil_space_get(space_id);

  if (fil_space != NULL) {
    char tmpname[FN_REFLEN * 2 + 2];
    bool exists;
    os_file_type_t type;

    strncpy(tmpname, dest_space_name, sizeof(tmpname) - 1);

    char *oldpath, *space_name;

    bool res =
        fil_space_read_name_and_filepath(fil_space->id, &space_name, &oldpath);

    ut_a(res);

    xb::info() << "Renaming " << fil_space->name << " to " << dest_space_name;

    ut_a(os_file_status(oldpath, &exists, &type));

    if (exists &&
        !fil_rename_tablespace(fil_space->id, oldpath, tmpname, NULL)) {
      xb::error() << "Cannot rename " << fil_space->name << " to "
                  << dest_space_name;
      ut::free(oldpath);
      ut::free(space_name);
      goto exit;
    }
    ut::free(oldpath);
    ut::free(space_name);

    goto found;
  }

  /* No matching space found. create the new one.  */

  fil_space = fil_space_create(dest_space_name, space_id, space_flags,
                               FIL_TYPE_TABLESPACE);
  if (fil_space == nullptr) {
    xb::error() << " Cannot create tablespace " << dest_space_name;
    goto exit;
  }

  /* Calculate correct tablespace flags for compressed tablespaces.  */
  if (zip_size != 0 && zip_size != ULINT_UNDEFINED) {
    space_flags |= (get_bit_shift(zip_size >> PAGE_ZIP_MIN_SIZE_SHIFT << 1)
                    << DICT_TF_ZSSIZE_SHIFT) |
                   DICT_TF_COMPACT |
                   (DICT_TF_FORMAT_ZIP << DICT_TF_FORMAT_SHIFT);
    ut_a(page_size_t(space_flags).physical() == zip_size);
  }
  *success =
      xb_space_create_file(real_name, space_id, space_flags, fil_space, &file);
  goto exit;

found:
  /* open the file and return it's handle */

  file = os_file_create_simple_no_error_handling(
      0, real_name, create_option, OS_FILE_READ_WRITE, srv_read_only_mode, &ok);

  if (ok) {
    *success = true;
  } else {
    xb::error() << "Cannot open file " << real_name;
  }

exit:

  return file;
}

/************************************************************************
Applies a given .delta file to the corresponding data file.
@return true on success */
static bool xtrabackup_apply_delta(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void * /*data*/) {
  pfs_os_file_t src_file = XB_FILE_UNDEFINED;
  pfs_os_file_t dst_file = XB_FILE_UNDEFINED;
  char src_path[FN_REFLEN];
  char dst_path[FN_REFLEN * 2 + 1];
  char meta_path[FN_REFLEN];
  char space_name[FN_REFLEN];
  bool success;

  bool last_buffer = false;
  ulint page_in_buffer;
  ulint incremental_buffers = 0;

  xb_delta_info_t info;
  ulint page_size;
  ulint page_size_shift;
  byte *incremental_buffer_base = NULL;
  byte *incremental_buffer;

  size_t offset;
  os_file_stat_t stat_info;

  if (entry.is_empty_dir) {
    return true;
  }

  IORequest read_request(IORequest::READ);
  IORequest write_request(IORequest::WRITE | IORequest::PUNCH_HOLE);

  ut_a(xtrabackup_incremental);

  if (!entry.db_name.empty()) {
    snprintf(src_path, sizeof(src_path), "%s/%s/%s", entry.datadir.c_str(),
             entry.db_name.c_str(), entry.file_name.c_str());
    snprintf(dst_path, sizeof(dst_path), "%s/%s/%s", xtrabackup_real_target_dir,
             entry.db_name.c_str(), entry.file_name.c_str());
  } else {
    snprintf(src_path, sizeof(src_path), "%s/%s", entry.datadir.c_str(),
             entry.file_name.c_str());
    snprintf(dst_path, sizeof(dst_path), "%s/%s", xtrabackup_real_target_dir,
             entry.file_name.c_str());
  }
  dst_path[strlen(dst_path) - 6] = '\0';

  strncpy(space_name, entry.file_name.c_str(), FN_REFLEN - 1);
  space_name[strlen(space_name) - 6] = 0;

  if (!get_meta_path(src_path, meta_path)) {
    goto error;
  }

  Fil_path::normalize(dst_path);
  Fil_path::normalize(src_path);
  Fil_path::normalize(meta_path);

  if (!xb_read_delta_metadata(meta_path, &info)) {
    goto error;
  }

  page_size = info.page_size;
  page_size_shift = get_bit_shift(page_size);
  xb::info() << "page size for " << src_path << " is " << page_size << " bytes";
  if (page_size_shift < 10 || page_size_shift > UNIV_PAGE_SIZE_SHIFT_MAX) {
    xb::error() << "invalid value of page_size (" << page_size
                << " bytes) read from " << meta_path;
    goto error;
  }

  src_file = os_file_create_simple_no_error_handling(
      0, src_path, OS_FILE_OPEN, OS_FILE_READ_WRITE, srv_read_only_mode,
      &success);
  if (!success) {
    os_file_get_last_error(true);
    xb::error() << "cannot open " << src_path;
    goto error;
  }

  posix_fadvise(src_file.m_file, 0, 0, POSIX_FADV_SEQUENTIAL);

  os_file_set_nocache(src_file.m_file, src_path, "OPEN");

  dst_file = xb_delta_open_matching_space(
      entry.db_name.empty() ? nullptr : entry.db_name.c_str(), space_name,
      info.space_id, info.space_flags, info.zip_size, dst_path,
      sizeof(dst_path), &success);
  if (!success) {
    xb::error() << "cannot open " << dst_path;
    goto error;
  }

  posix_fadvise(dst_file.m_file, 0, 0, POSIX_FADV_DONTNEED);

  os_file_set_nocache(dst_file.m_file, dst_path, "OPEN");

  os_file_get_status(dst_path, &stat_info, false, false);

  /* allocate buffer for incremental backup */
  incremental_buffer_base = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                         (page_size / 4 + 1) * page_size + UNIV_PAGE_SIZE_MAX));
  incremental_buffer = static_cast<byte *>(
      ut_align(incremental_buffer_base, UNIV_PAGE_SIZE_MAX));

  xb::info() << "Applying " << src_path << " to " << dst_path;

  while (!last_buffer) {
    ulint cluster_header;

    /* read to buffer */
    /* first block of block cluster */
    offset = ((incremental_buffers * (page_size / 4)) << page_size_shift);
    success = os_file_read(read_request, src_path, src_file, incremental_buffer,
                           offset, page_size);
    if (!success) {
      goto error;
    }

    cluster_header = mach_read_from_4(incremental_buffer);
    switch (cluster_header) {
      case 0x78747261UL: /*"xtra"*/
        break;
      case 0x58545241UL: /*"XTRA"*/
        last_buffer = true;
        break;
      default:
        xb::info() << src_path << " is not valid .delta file.";
        goto error;
    }

    for (page_in_buffer = 1; page_in_buffer < page_size / 4; page_in_buffer++) {
      if (mach_read_from_4(incremental_buffer + page_in_buffer * 4) ==
          0xFFFFFFFFUL)
        break;
    }

    ut_a(last_buffer || page_in_buffer == page_size / 4);

    /* read whole of the cluster */
    success = os_file_read(read_request, src_path, src_file, incremental_buffer,
                           offset, page_in_buffer * page_size);
    if (!success) {
      goto error;
    }

    posix_fadvise(src_file.m_file, offset, page_in_buffer * page_size,
                  POSIX_FADV_DONTNEED);

    for (page_in_buffer = 1; page_in_buffer < page_size / 4; page_in_buffer++) {
      const page_t *page = incremental_buffer + page_in_buffer * page_size;
      const ulint offset_on_page =
          mach_read_from_4(incremental_buffer + page_in_buffer * 4);

      if (offset_on_page == 0xFFFFFFFFUL) break;

      const auto offset_in_file = offset_on_page << page_size_shift;

      success = os_file_write(write_request, dst_path, dst_file, page,
                              offset_in_file, page_size);
      if (!success) {
        goto error;
      }

      if (IORequest::is_punch_hole_supported() &&
          (Compression::is_compressed_page(page) ||
           fil_page_get_type(page) == FIL_PAGE_COMPRESSED_AND_ENCRYPTED)) {
        size_t compressed_len =
            mach_read_from_2(page + FIL_PAGE_COMPRESS_SIZE_V1) + FIL_PAGE_DATA;
        compressed_len = ut_calc_align(compressed_len, stat_info.block_size);
        if (compressed_len < page_size) {
          if (os_file_punch_hole(dst_file.m_file,
                                 offset_in_file + compressed_len,
                                 page_size - compressed_len) != DB_SUCCESS) {
            xb::error() << "os_file_punch_hole returned error";
            goto error;
          }
        }
      }
    }

    incremental_buffers++;
  }

  if (incremental_buffer_base) ut::free(incremental_buffer_base);
  if (src_file != XB_FILE_UNDEFINED) os_file_close(src_file);
  if (dst_file != XB_FILE_UNDEFINED) os_file_close(dst_file);
  return true;

error:
  if (incremental_buffer_base) ut::free(incremental_buffer_base);
  if (src_file != XB_FILE_UNDEFINED) os_file_close(src_file);
  if (dst_file != XB_FILE_UNDEFINED) os_file_close(dst_file);
  xb::error() << "xtrabackup_apply_delta(): failed to apply " << src_path
              << " to " << dst_path;
  return false;
}

static void delete_force(const std::string &path) {
  if (access(path.c_str(), R_OK) == 0) {
    if (my_delete(path.c_str(), MYF(MY_WME))) {
      exit(EXIT_FAILURE);
    }
  }
}

static void rename_file(const std::string &from, const std::string &to) {
  if (my_rename(from.c_str(), to.c_str(), MY_WME)) {
    xb::error() << "Can't rename " << from << " to " << to << " errno "
                << errno;
    exit(EXIT_FAILURE);
  }
}

static void rename_force(const std::string &from, const std::string &to) {
  MY_STAT stat_info;
  if (access(to.c_str(), R_OK) == 0) {
    if (my_delete(to.c_str(), MYF(MY_WME))) {
      exit(EXIT_FAILURE);
    }
  }
  /* check if dest folder exists */
  std::string dest_dir = to.substr(0, to.find_last_of("/"));
  /* create target dir if not exist */
  if (!my_stat(dest_dir.c_str(), &stat_info, MYF(0)) &&
      (my_mkdir(dest_dir.c_str(), 0750, MYF(0)) < 0)) {
    xb::error() << "cannot mkdir: " << my_errno() << " " << dest_dir;
    exit(EXIT_FAILURE);
  }
  rename_file(from, to);
}

/* Handle DDL for new files */
static bool prepare_handle_new_files(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void *arg __attribute__((unused))) {
  if (entry.is_empty_dir) return true;
  std::string src_path = entry.path;
  std::string dest_path = src_path;
  xb::info() << "prepare_handle_new_files: " << src_path;
  size_t index = dest_path.find(".new");
#ifdef UNIV_DEBUG
  assert(index != std::string::npos);
#endif  // UNIV_DEBUG
  dest_path.erase(index, 4);
  rename_force(src_path, dest_path);

  return true;
}

/** Read file content into STL string */
static std::string read_file_as_string(const std::string file) {
  char content[FN_REFLEN];
  FILE *f = fopen(file.c_str(), "r");
  if (!f) {
    return "";
  }
  size_t len = fread(content, 1, FN_REFLEN, f);
  fclose(f);
  return std::string(content, len);
}
/* Handle DDL for renamed files */
static bool prepare_handle_ren_files(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void *arg __attribute__((unused))) {
  if (entry.is_empty_dir) return true;

  std::string ren_file = entry.path;
  std::string ren_path = entry.rel_path;
  std::string from_base = entry.datadir;
  std::string to_base = entry.datadir;
#ifdef UNIV_DEBUG
  size_t index = ren_file.find(".ren");
  assert(index != std::string::npos);
#endif  // UNIV_DEBUG
  std::string from = ren_path.substr(0, ren_path.length() - 4);
  from_base += from;
  std::string to = read_file_as_string(ren_file);
  to_base += to;
  if (to.empty()) {
    xb::error() << "Can not read " << ren_file;
    return false;
  }
  xb::info() << "prepare_handle_ren_files: From: " << from << " To: " << to;
  rename_force(from, to);
  if (xtrabackup_incremental) {
    rename_force(from_base + ".delta", to_base + ".delta");
    rename_force(from_base + ".meta", to_base + ".meta");
  }
  os_file_delete(0, ren_file.c_str());
  return true;
}
/* Handle DDL for deleted files */
static bool prepare_handle_del_files(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void *arg __attribute__((unused))) {
  if (entry.is_empty_dir) return true;

  std::string del_file = entry.path;
  std::string dest_path = entry.rel_path;
  xb::info() << "prepare_handle_del_files: " << del_file;
#ifdef UNIV_DEBUG
  size_t index = dest_path.find(".del");
  assert(index != std::string::npos);
#endif  // UNIV_DEBUG
  os_file_delete(0, del_file.c_str());
  del_file.erase(del_file.length() - 4);
  delete_force(dest_path.substr(0, dest_path.length() - 4).c_str());
  if (xtrabackup_incremental) {
    delete_force(del_file + ".delta");
    delete_force(del_file + ".meta");
  }

  return true;
}
/************************************************************************
Callback to handle datadir entry. Deletes entry if it has no matching
fil_space in fil_system directory.
@return false if delete attempt was unsuccessful */
static bool rm_if_not_found(
    const datadir_entry_t &entry, /*!<in: datadir entry */
    void *arg __attribute__((unused))) {
  char name[FN_REFLEN];
  xb_filter_entry_t *table;

  if (entry.is_empty_dir) {
    return true;
  }

  if (!entry.db_name.empty()) {
    snprintf(name, FN_REFLEN, "%s/%s", entry.db_name.c_str(),
             entry.file_name.c_str());
  } else {
    snprintf(name, FN_REFLEN, "%s", entry.file_name.c_str());
  }

  /* Truncate ".ibd" */
  name[strlen(name) - 4] = '\0';

  HASH_SEARCH(name_hash, inc_dir_tables_hash, ut::hash_string(name),
              xb_filter_entry_t *, table, (void)0, !strcmp(table->name, name));

  if (!table) {
    if (!entry.db_name.empty()) {
      snprintf(name, FN_REFLEN, "%s/%s/%s", entry.datadir.c_str(),
               entry.db_name.c_str(), entry.file_name.c_str());
    } else {
      snprintf(name, FN_REFLEN, "%s/%s", entry.datadir.c_str(),
               entry.file_name.c_str());
    }
    return os_file_delete(0, name);
  }

  return (true);
}

/** Make sure that we have a read access to the given datadir entry
@param[in]	statinfo	entry stat info
@param[out]	name		entry name */
static void check_datadir_enctry_access(const char *name,
                                        const struct stat *statinfo) {
  const char *entry_type = S_ISDIR(statinfo->st_mode) ? "directory" : "file";
  if ((statinfo->st_mode & S_IRUSR) != S_IRUSR) {
    xb::error() << entry_type << " " << SQUOTE(name)
                << " is not readable by "
                   "XtraBackup";
    exit(EXIT_FAILURE);
  }
}

/** Process single second level datadir entry for
xb_process_datadir
@param[in]	datadir	datadir path
@param[in]	path	path to the file
@param[in]	dbname	database name (first level entry name)
@param[in]	name	name of the file
@param[in]	suffix	suffix to match against
@param[in]	func	callback
@param[in]	data	additional argument for callback */
void process_datadir_l2cbk(const char *datadir, const char *dbname,
                           const char *path, const char *name,
                           const char *suffix, handle_datadir_entry_func_t func,
                           void *data) {
  struct stat statinfo;
  size_t suffix_len = strlen(suffix);

  if (stat(path, &statinfo) != 0) {
    return;
  }

  if (S_ISREG(statinfo.st_mode) &&
      (strlen(name) > suffix_len &&
       strcmp(name + strlen(name) - suffix_len, suffix) == 0)) {
    check_datadir_enctry_access(name, &statinfo);
    func(datadir_entry_t(datadir, path, dbname, name, false), data);
  }
}

/** Process single top level datadir entry for xb_process_datadir
@param[in]	datadir	datadir path
@param[in]	path	path to the file
@param[in]	name	name of the file
@param[in]	suffix	suffix to match against
@param[in]	func	callback
@param[in]	data	additional argument for callback */
void process_datadir_l1cbk(const char *datadir, const char *path,
                           const char *name, const char *suffix,
                           handle_datadir_entry_func_t func, void *data) {
  struct stat statinfo;
  size_t suffix_len = strlen(suffix);

  if (stat(path, &statinfo) != 0) {
    return;
  }

  if (S_ISDIR(statinfo.st_mode) && !check_if_skip_database_by_path(name)) {
    bool is_empty_dir = true;
    check_datadir_enctry_access(name, &statinfo);
    os_file_scan_directory(
        path,
        [&](const char *l2path, const char *l2name) mutable -> void {
          if (strcmp(l2name, ".") == 0 || strcmp(l2name, "..") == 0) {
            return;
          }
          is_empty_dir = false;
          char fullpath[FN_REFLEN];
          snprintf(fullpath, sizeof(fullpath), "%s/%s", l2path, l2name);
          process_datadir_l2cbk(datadir, name, fullpath, l2name, suffix, func,
                                data);
        },
        false);
    if (is_empty_dir) {
      func(datadir_entry_t(datadir, path, name, "", true), data);
    }
  }

  if (S_ISREG(statinfo.st_mode) &&
      (strlen(name) > suffix_len &&
       strcmp(name + strlen(name) - suffix_len, suffix) == 0)) {
    check_datadir_enctry_access(name, &statinfo);
    func(datadir_entry_t(datadir, path, "", name, false), data);
  }
}

/************************************************************************
Function enumerates files in datadir (provided by path) which are matched
by provided suffix. For each entry callback is called.
@return false if callback for some entry returned false */
bool xb_process_datadir(const char *path,   /*!<in: datadir path */
                        const char *suffix, /*!<in: suffix to match
                                            against */
                        handle_datadir_entry_func_t func, /*!<in: callback */
                        void *data) /*!<in: additional argument for
                                    callback */
{
  bool ret = os_file_scan_directory(
      path,
      [&](const char *l1path, const char *l1name) -> void {
        if (strcmp(l1name, ".") == 0 || strcmp(l1name, "..") == 0) {
          return;
        }
        char fullpath[FN_REFLEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", l1path, l1name);
        process_datadir_l1cbk(path, fullpath, l1name, suffix, func, data);
      },
      false);
  return ret;
}

/************************************************************************
Applies all .delta files from incremental_dir to the full backup.
@return true on success. */
static bool xtrabackup_apply_deltas() {
  return xb_process_datadir(xtrabackup_incremental_dir, ".delta",
                            xtrabackup_apply_delta, NULL);
}

/* replace log file in redo directory to xtrabackup_log
@in redo_path  path of redo direcotry
@in log_files_ctx log_files_ctxt
@in clear_flag  remove log_creator and log_format
@return false if failed and true if successful
*/
static bool xtrabackup_replace_log_file(const char *redo_path,
                                        const Log_files_context &log_files_ctx,
                                        bool clear_flag) {
  byte log_buf[UNIV_PAGE_SIZE_MAX];
  IORequest read_request(IORequest::READ);
  IORequest write_request(IORequest::WRITE);
  char src_path[FN_REFLEN];
  char dst_path[FN_REFLEN];
  bool success;
  pfs_os_file_t src_file = XB_FILE_UNDEFINED;
  /* rename '#ib_redoN' to 'xtrabackup_logfile' */
  ut::vector<Log_file_id> listed_files;

  const dberr_t err = log_list_existing_files(log_files_ctx, listed_files);
  if (err != DB_SUCCESS) {
    xb::error() << "Failed to find log files in redo directory ";
    goto error;
  }

  if (listed_files.size() != 1) {
    xb::error() << "Found more than one file in redo directory ";
    goto error;
  }

  if (!xtrabackup_incremental_dir) {
    sprintf(dst_path, "%s/%s/%s%ld", xtrabackup_target_dir, LOG_DIRECTORY_NAME,
            LOG_FILE_BASE_NAME, listed_files.back());
    sprintf(src_path, "%s/%s", xtrabackup_target_dir, XB_LOG_FILENAME);
  } else {
    sprintf(dst_path, "%s/%s/%s%ld", xtrabackup_incremental_dir,
            LOG_DIRECTORY_NAME, LOG_FILE_BASE_NAME, listed_files.back());
    sprintf(src_path, "%s/%s", xtrabackup_incremental_dir, XB_LOG_FILENAME);
  }

  Fil_path::normalize(dst_path);
  Fil_path::normalize(src_path);

  if (!os_file_rename(0, dst_path, src_path)) {
    return false;
  }
  xtrabackup_logfile_is_renamed = false;

  if (!clear_flag) {
    return true;
  }

  src_file = os_file_create_simple_no_error_handling(
      0, src_path, OS_FILE_OPEN, OS_FILE_READ_WRITE, srv_read_only_mode,
      &success);
  if (!success) {
    goto error;
  }

  success = os_file_read(read_request, src_path, src_file, log_buf, 0,
                         LOG_FILE_HDR_SIZE);
  if (!success) {
    goto error;
  }

  /* clear LOG_HEADER_CREATOR field */
  memset(log_buf + LOG_HEADER_CREATOR, ' ', 4);
  success = os_file_write(write_request, src_path, src_file, log_buf, 0,
                          LOG_FILE_HDR_SIZE);
  if (!success) {
    goto error;
  }

  src_file = XB_FILE_UNDEFINED;

  return true;

error:
  if (src_file != XB_FILE_UNDEFINED) os_file_close(src_file);
  xb::error() << "xtrabackup_replace_log_file() failed.";
  return false; /*ERROR*/
}

/* Replace the redo log file in redo directory to xtrabackup_log and clear the
redo directory
@in clear_flag clear the log_format and log_creator in redo log
@return true if any error occured or false  */
static bool xtrabackup_close_temp_log(bool clear_flag) {
  char redo_folder[FN_REFLEN];
  const char *backup_dir = !xtrabackup_incremental_dir
                               ? xtrabackup_target_dir
                               : xtrabackup_incremental_dir;

  Log_files_context log_files_ctx =
      Log_files_context{backup_dir, Log_files_ruleset::CURRENT};
  sprintf(redo_folder, "%s/%s", backup_dir, LOG_DIRECTORY_NAME);

  bool valid = xb_log_files_validate_creators(backup_dir);

  if (!valid) {
    xb::error()
        << "Unable to validate the redo log files at the end of prepare";
    return (true);
  }

  if (xb_generated_redo && !clear_flag) {
    xb::info() << "xtrabackup --prepare generated new redo log files. Skipping "
               << "rename from ib_redo0 to " << XB_LOG_FILENAME;
    return (false);
  }

  if (xtrabackup_logfile_is_renamed) {
    if (!xtrabackup_replace_log_file(redo_folder, log_files_ctx, clear_flag)) {
      xb::error() << "failed to replace redo log file to " << XB_LOG_FILENAME;
      return (true);
    }
    innobase_log_files_in_group = innobase_log_files_in_group_save;
    srv_log_group_home_dir = srv_log_group_home_dir_save;
    innobase_log_file_size = innobase_log_file_size_save;
  }

  /* remove #innodb_redo dir if exist */
  if (!os_file_scan_directory(
          redo_folder,
          [](const char *path, const char *file_name) {
            if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
              return;
            }
            const auto to_remove =
                std::string{path} + OS_PATH_SEPARATOR + file_name;
            unlink(to_remove.c_str());
          },
          false)) {
    xb::error() << "Error removing directory : " << redo_folder;
    return (true);
  }

  return (false);
}

/*********************************************************************/ /**
 Write the meta data (index user fields) config file.
 @return true in case of success otherwise false. */
static bool xb_export_cfg_write_index_fields(
    /*===========================*/
    const dict_index_t *index, /*!< in: write the meta data for
                               this index */
    FILE *file)                /*!< in: file to write to */
{
  /* This row will store prefix_len, fixed_len,
  and in IB_EXPORT_CFG_VERSION_V4, is_ascending */
  byte row[sizeof(uint32_t) * 3];
  size_t row_len = sizeof(row);

  for (ulint i = 0; i < index->n_fields; ++i) {
    byte *ptr = row;
    const dict_field_t *field = &index->fields[i];

    mach_write_to_4(ptr, field->prefix_len);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, field->fixed_len);

    if (cfg_version >= IB_EXPORT_CFG_VERSION_V4) {
      ptr += sizeof(uint32_t);
      /* In IB_EXPORT_CFG_VERSION_V4 we also write the is_ascending boolean. */
      mach_write_to_4(ptr, field->is_ascending);
    } else {
      row_len = sizeof(uint32_t) * 2;
    }

    if (fwrite(row, 1, row_len, file) != row_len) {
      xb::error() << "writing index fields.";

      return (false);
    }

    /* Include the NUL byte in the length. */
    uint32_t len = strlen(field->name) + 1;
    ut_a(len > 1);

    mach_write_to_4(row, len);

    if (fwrite(row, 1, sizeof(len), file) != sizeof(len) ||
        fwrite(field->name, 1, len, file) != len) {
      xb::error() << "writing index column.";

      return (false);
    }
  }

  return (true);
}

/*********************************************************************/ /**
 Write the meta data config file index information.
 @return true in case of success otherwise false. */
[[nodiscard]] static bool xb_export_cfg_write_one_index(
    /*======================*/
    const dict_index_t *index, /*!< in: write metadata for this
                               index */
    FILE *file)                /*!< in: file to write to */
{
  bool ret;
  byte *ptr;
  byte row[sizeof(space_index_t) + sizeof(uint32_t) * 8];

  ptr = row;

  ut_ad(sizeof(space_index_t) == 8);
  mach_write_to_8(ptr, index->id);
  ptr += sizeof(space_index_t);

  mach_write_to_4(ptr, index->space);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->page);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->type);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->trx_id_offset);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->n_user_defined_cols);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->n_uniq);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->n_nullable);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->n_fields);

  if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
    xb::error() << "writing index meta-data.";
    return (false);
  }

  /* Write the length of the index name.
  NUL byte is included in the length. */
  uint32_t len = static_cast<uint32_t>(strlen(index->name) + 1);
  ut_a(len > 1);

  mach_write_to_4(row, len);

  if (fwrite(row, 1, sizeof(len), file) != sizeof(len) ||
      fwrite(index->name, 1, len, file) != len) {
    xb::error() << "writing index name.";
    return (false);
  }

  ret = xb_export_cfg_write_index_fields(index, file);
  return (ret);
}

/*********************************************************************/ /**
 Write the meta data config file index information.
 @return true in case of success otherwise false. */
[[nodiscard]] static bool xb_export_cfg_write_indexes(
    /*======================*/
    const dict_table_t *table, /*!< in: write the meta data for
                               this table */
    FILE *file)                /*!< in: file to write to */
{
  byte row[sizeof(uint32_t)];

  /* Write the number of indexes in the table. */
  uint32_t num_indexes = 0;
  ulint flags = fil_space_get_flags(table->space);
  bool has_sdi = FSP_FLAGS_HAS_SDI(flags);

  if (has_sdi) {
    num_indexes += 1;
  }

  num_indexes += static_cast<uint32_t>(UT_LIST_GET_LEN(table->indexes));
  ut_ad(num_indexes != 0);

  mach_write_to_4(row, num_indexes);

  if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
    xb::error() << "writing index count.";
    return (false);
  }

  bool ret = true;

  /* Write SDI Index */
  if (has_sdi) {
    dict_index_t *index = dict_sdi_get_index(table->space);

    ut_ad(index != nullptr);
    ret = xb_export_cfg_write_one_index(index, file);
  }

  /* Write the index meta data. */
  for (const dict_index_t *index : table->indexes) {
    ret = xb_export_cfg_write_one_index(index, file);
  }

  return (ret);
}

/*********************************************************************/ /**
 Write the meta data (table columns) config file. Serialise the contents of
 dict_col_t structure, along with the column name. All fields are serialized
 as uint32_t.
 @return true in case of success otherwise false. */
[[nodiscard]] static bool xb_export_cfg_write_table(
    /*====================*/
    const dict_table_t *table, /*!< in: write the meta data for
                               this table */
    FILE *file)                /*!< in: file to write to */
{
  dict_col_t *col;
  byte row[sizeof(uint32_t) * 7];

  col = table->cols;

  for (ulint i = 0; i < table->get_total_cols(); ++i, ++col) {
    byte *ptr = row;

    mach_write_to_4(ptr, col->prtype);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->mtype);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->len);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->mbminmaxlen);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->ind);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->ord_part);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->max_prefix);

    if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
      xb::error() << "writing table column data.";

      return (false);
    }

    /* Write out the column name as [len, byte array]. The len
    includes the NUL byte. */
    uint32_t len;
    const char *col_name;

    col_name = table->get_col_name(dict_col_get_no(col));

    /* Include the NUL byte in the length. */
    len = strlen(col_name) + 1;
    ut_a(len > 1);

    mach_write_to_4(row, len);

    if (fwrite(row, 1, sizeof(len), file) != sizeof(len) ||
        fwrite(col_name, 1, len, file) != len) {
      xb::error() << "writing column name.";

      return (false);
    }
    /* Write column's INSTANT metadata */
    if (cfg_version >= IB_EXPORT_CFG_VERSION_V7) {
      byte row[2 + sizeof(uint32_t)];
      byte *ptr = row;

      /* version added */
      byte value =
          col->is_instant_added() ? col->get_version_added() : UINT8_UNDEFINED;
      mach_write_to_1(ptr, value);
      ptr++;

      /* version dropped */
      value = col->is_instant_dropped() ? col->get_version_dropped()
                                        : UINT8_UNDEFINED;
      mach_write_to_1(ptr, value);
      ptr++;

      /* physical position */
      mach_write_to_4(ptr, col->get_phy_pos());

      if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
        xb::error() << "while writing table column instant metadata.";
        return (false);
      }
      /* Write DD::Column specific info for dropped columns */
      if (col->is_instant_dropped()) {
        mutex_exit(&dict_sys->mutex);
        std::unique_ptr<dd::Table> dd_table =
            xb::prepare::get_dd_Table(table->space, table->id);
        mutex_enter(&dict_sys->mutex);

        if (dd_table != nullptr) {
          /* Total metadata to be written
          1 byte for is NULLABLE
          1 byte for is_unsigned
          4 bytes for char_length
          4 bytes for column type
          4 bytes for numeric scale
          8 bytes for collation id */
          constexpr size_t METADATA_SIZE = 22;

          byte _row[METADATA_SIZE];

          ut_ad(col->is_instant_dropped());
          const dd::Column *column = dd_find_column(dd_table.get(), col_name);
          ut_ad(column != nullptr);

          byte *_ptr = _row;

          /* 1 byte for is NULLABLE */
          mach_write_to_1(_ptr, column->is_nullable());
          _ptr++;

          /* 1 byte for is_unsigned */
          mach_write_to_1(_ptr, column->is_unsigned());
          _ptr++;

          /* 4 bytes for char_length() */
          mach_write_to_4(_ptr, column->char_length());
          _ptr += 4;

          /* 4 bytes for column type */
          mach_write_to_4(_ptr, (uint32_t)column->type());
          _ptr += 4;

          /* 4 bytes for numeric scale */
          mach_write_to_4(_ptr, column->numeric_scale());
          _ptr += 4;

          /* 8 bytes for collation id */
          mach_write_to_8(_ptr, column->collation_id());
          _ptr += 8;

          if (fwrite(_row, 1, sizeof(_row), file) != sizeof(_row)) {
            return (false);
          }
          /* Write elements for enum column type.
          [4]     bytes : numner of elements
          For each element
            [4]     bytes : element name length (len+1)
            [len+1] bytes : element name */
          if (column->type() == dd::enum_column_types::ENUM ||
              column->type() == dd::enum_column_types::SET) {
            byte __row[sizeof(uint32_t)];

            /* Write element count */
            mach_write_to_4(__row, column->elements().size());
            if (fwrite(__row, 1, sizeof(__row), file) != sizeof(__row)) {
              xb::error()
                  << "while writing enum column element count for column "
                  << col_name;

              return (false);
            }

            /* Write out the enum/set column element name as [len, byte array].
             */
            for (const auto *source_elem : column->elements()) {
              const char *elem_name = source_elem->name().c_str();
              uint32_t len = strlen(elem_name) + 1;
              ut_a(len > 1);

              mach_write_to_4(__row, len);

              if (fwrite(__row, 1, sizeof(len), file) != sizeof(len) ||
                  fwrite(elem_name, 1, len, file) != len) {
                xb::error()
                    << "while writing enum column element name for column "
                    << col_name;

                return (false);
              }
            }
          }
        } else {
          xb::error() << "DD table ID " << table->space << " not found.";
        }
      }
    }
    if (cfg_version >= IB_EXPORT_CFG_VERSION_V3) {
      if (row_quiesce_write_default_value(col, file) != DB_SUCCESS) {
        return (false);
      }
    }
  }

  return (true);
}

/*********************************************************************/ /**
 Write the meta data config file header.
 @return true in case of success otherwise false. */
[[nodiscard]] static bool xb_export_cfg_write_header(
    /*=====================*/
    const dict_table_t *table, /*!< in: write the meta data for
                               this table */
    FILE *file)                /*!< in: file to write to */
{
  byte value[sizeof(uint32_t)];

  /* Write the meta-data version number. */
  mach_write_to_4(value, cfg_version);

  if (fwrite(&value, 1, sizeof(value), file) != sizeof(value)) {
    xb::error() << "writing meta-data version number.";

    return (false);
  }

  /* Write the server hostname. */
  uint32_t len;
  const char *hostname = "Hostname unknown";

  /* The server hostname includes the NUL byte. */
  len = strlen(hostname) + 1;
  mach_write_to_4(value, len);

  if (fwrite(&value, 1, sizeof(value), file) != sizeof(value) ||
      fwrite(hostname, 1, len, file) != len) {
    xb::error() << "writing hostname.";

    return (false);
  }

  /* The table name includes the NUL byte. */
  ut_a(table->name.m_name != NULL);
  len = strlen(table->name.m_name) + 1;

  /* Write the table name. */
  mach_write_to_4(value, len);

  if (fwrite(&value, 1, sizeof(value), file) != sizeof(value) ||
      fwrite(table->name.m_name, 1, len, file) != len) {
    xb::error() << "writing table name.";

    return (false);
  }

  byte row[sizeof(uint32_t) * 3];

  /* Write the next autoinc value. */
  mach_write_to_8(row, table->autoinc);

  if (fwrite(row, 1, sizeof(uint64_t), file) != sizeof(uint64_t)) {
    xb::error() << "writing table autoinc value.";

    return (false);
  }

  byte *ptr = row;

  /* Write the system page size. */
  mach_write_to_4(ptr, UNIV_PAGE_SIZE);
  ptr += sizeof(uint32_t);

  /* Write the table->flags. */
  mach_write_to_4(ptr, table->flags);
  ptr += sizeof(uint32_t);

  /* Write the number of columns in the table. In case of INSTANT, include
  dropped columns as well. */
  mach_write_to_4(ptr, table->get_total_cols());

  if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
    xb::error() << "writing table meta-data.";

    return (false);
  }

  if (cfg_version >= IB_EXPORT_CFG_VERSION_V5) {
    /* write number of nullable column before first instant column */
    mach_write_to_4(value, table->first_index()->get_instant_nullable());

    if (fwrite(&value, 1, sizeof(value), file) != sizeof(value)) {
      xb::error() << "Error writing table meta-data.";

      return (false);
    }
  }

  /* Write table instant metadata */
  if (cfg_version >= IB_EXPORT_CFG_VERSION_V7) {
    byte row[sizeof(uint32_t) * 5];
    byte *ptr = row;

    /* Write initial column count */
    mach_write_to_4(ptr, table->initial_col_count);
    ptr += sizeof(uint32_t);

    /* Write current column count */
    mach_write_to_4(ptr, table->current_col_count);
    ptr += sizeof(uint32_t);

    /* Write total column count */
    mach_write_to_4(ptr, table->total_col_count);
    ptr += sizeof(uint32_t);

    /* Write number of instantly dropped columns */
    mach_write_to_4(ptr, table->get_n_instant_drop_cols());
    ptr += sizeof(uint32_t);

    /* Write current row version */
    mach_write_to_4(ptr, table->current_row_version);

    if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
      xb::error() << "while writing table meta-data.";

      return (false);
    }
  }

  /* Write the space flags */
  ulint space_flags = fil_space_get_flags(table->space);
  ut_ad(space_flags != ULINT_UNDEFINED);
  mach_write_to_4(value, space_flags);

  if (fwrite(&value, 1, sizeof(value), file) != sizeof(value)) {
    xb::error() << "Error writing space_flags.";
    return (false);
  }

  if (cfg_version >= IB_EXPORT_CFG_VERSION_V6) {
    /* Write compression type info. */
    uint8_t compression_type =
        static_cast<uint8_t>(fil_get_compression(table->space));
    mach_write_to_1(value, compression_type);

    if (fwrite(&value, 1, sizeof(uint8_t), file) != sizeof(uint8_t)) {
      xb::info() << "Error writing compression type info.";

      return (false);
    }
  }

  return (true);
}

/*********************************************************************/ /**
 Write MySQL 5.6-style meta data config file.
 @return true in case of success otherwise false. */
static bool xb_export_cfg_write(
    const fil_node_t *node, const dict_table_t *table) /*!< in: write the meta
                                                       data for this table */
{
  char file_path[FN_REFLEN];
  FILE *file;
  bool success;

  strcpy(file_path, node->name);
  strcpy(file_path + strlen(file_path) - 4, ".cfg");

  file = fopen(file_path, "w+b");

  if (file == NULL) {
    xb::error() << "cannot open " << node->name;

    success = false;
  } else {
    success = xb_export_cfg_write_header(table, file);

    if (success) {
      success = xb_export_cfg_write_table(table, file);
    }

    if (success) {
      success = xb_export_cfg_write_indexes(table, file);
    }

    if (fclose(file) != 0) {
      xb::error() << "cannot close " << node->name;
      success = false;
    }
  }

  return (success);
}

/** Write the transfer key to CFP file.
@param[in]	table		write the data for this table
@param[in] 	encryption_metadata metadta of encryption
@param[in]	file		file to write to
@return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t xb_export_write_transfer_key(
    const Encryption_metadata &encryption_metadata, FILE *file) {
  byte key_size[sizeof(uint32_t)];
  byte row[Encryption::KEY_LEN * 3];
  byte *ptr = row;
  byte *transfer_key = ptr;
  lint elen;
  ut_ad(encryption_metadata.can_encrypt() &&
        encryption_metadata.m_key != NULL && encryption_metadata.m_iv != NULL);

  /* Write the encryption key size. */
  mach_write_to_4(key_size, Encryption::KEY_LEN);

  if (fwrite(&key_size, 1, sizeof(key_size), file) != sizeof(key_size)) {
    xb::error() << "IO Write error: (" << errno << "," << strerror(errno) << ")"
                << " while writing key size.";

    return (DB_IO_ERROR);
  }

  /* Generate and write the transfer key. */
  Encryption::random_value(transfer_key);
  if (fwrite(transfer_key, 1, Encryption::KEY_LEN, file) !=
      Encryption::KEY_LEN) {
    xb::error() << "IO Write error: (" << errno << "," << strerror(errno) << ")"
                << " while writing transfer key.";

    return (DB_IO_ERROR);
  }

  ptr += Encryption::KEY_LEN;

  /* Encrypt tablespace key. */
  elen = my_aes_encrypt(encryption_metadata.m_key, Encryption::KEY_LEN, ptr,
                        reinterpret_cast<unsigned char *>(transfer_key),
                        Encryption::KEY_LEN, my_aes_256_ecb, NULL, false);

  if (elen == MY_AES_BAD_DATA) {
    xb::error() << "IO Write error: (" << errno << "," << strerror(errno) << ")"
                << " while encrypt tablespace key.";
    return (DB_ERROR);
  }

  /* Write encrypted tablespace key */
  if (fwrite(ptr, 1, Encryption::KEY_LEN, file) != Encryption::KEY_LEN) {
    xb::error() << "IO Write error: (" << errno << "," << strerror(errno) << ")"
                << " while writing encrypted tablespace key.";

    return (DB_IO_ERROR);
  }
  ptr += Encryption::KEY_LEN;

  /* Encrypt tablespace iv. */
  elen = my_aes_encrypt(encryption_metadata.m_iv, Encryption::KEY_LEN, ptr,
                        reinterpret_cast<unsigned char *>(transfer_key),
                        Encryption::KEY_LEN, my_aes_256_ecb, NULL, false);

  if (elen == MY_AES_BAD_DATA) {
    xb::error() << "IO Write error: (" << errno << "," << strerror(errno) << ")"
                << " while encrypt tablespace iv.";
    return (DB_ERROR);
  }

  /* Write encrypted tablespace iv */
  if (fwrite(ptr, 1, Encryption::KEY_LEN, file) != Encryption::KEY_LEN) {
    xb::error() << "IO Write error: (" << errno << "," << strerror(errno) << ")"
                << " while writing encrypted tablespace iv.";

    return (DB_IO_ERROR);
  }

  return (DB_SUCCESS);
}

/** Write the encryption data after quiesce.
@param[in]	table		write the data for this table
@return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t xb_export_cfp_write(dict_table_t *table) {
  dberr_t err;
  char name[OS_FILE_MAX_PATH];

  /* If table is not encrypted, return. */
  if (!dd_is_table_in_encrypted_tablespace(table)) {
    return (DB_SUCCESS);
  }

  /* Get the encryption key and iv from space */
  /* For encrypted table, before we discard the tablespace,
  we need save the encryption information into table, otherwise,
  this information will be lost in fil_discard_tablespace along
  with fil_space_free(). */

  fil_space_t *space = fil_space_get(table->space);
  ut_ad(space != nullptr && FSP_FLAGS_GET_ENCRYPTION(space->flags));

  srv_get_encryption_data_filename(table, name, sizeof(name));

  ib::info() << "Writing table encryption data to '" << name << "'";

  FILE *file = fopen(name, "w+b");

  if (file == NULL) {
    xb::error() << "Can't create file " << SQUOTE(name) << " (errno: " << errno
                << " - " << strerror(errno) << ")";

    err = DB_IO_ERROR;
  } else {
    err = xb_export_write_transfer_key(space->m_encryption_metadata, file);

    if (fflush(file) != 0) {
      char buf[BUFSIZ];

      snprintf(buf, sizeof(buf), "%s flush() failed", name);

      xb::error() << "IO Write error: (" << errno << ", " << strerror(errno)
                  << ") " << buf;

      err = DB_IO_ERROR;
    }

    if (fclose(file) != 0) {
      char buf[BUFSIZ];

      snprintf(buf, sizeof(buf), "%s flose() failed", name);

      xb::error() << "IO Write error: (" << errno << ", " << strerror(errno)
                  << ") " << buf;
      err = DB_IO_ERROR;
    }
  }

  return (err);
}

static void innodb_free_param() {
  srv_sys_space.shutdown();
  srv_tmp_space.shutdown();
  free(internal_innobase_data_file_path);
  internal_innobase_data_file_path = NULL;
  free_tmpdir(&mysql_tmpdir_list);
  ibt::srv_temp_dir = srv_temp_dir;
}

static void read_metadata() {
  char metadata_path[FN_REFLEN];
  char xtrabackup_info_path[FN_REFLEN];

  /* cd to target-dir */
  if (my_setwd(xtrabackup_real_target_dir, MYF(MY_WME))) {
    xb::error() << "cannot my_setwd " << xtrabackup_real_target_dir;
    exit(EXIT_FAILURE);
  }
  xb::info() << "cd to " << xtrabackup_real_target_dir;

  xtrabackup_target_dir = mysql_data_home_buff;
  xtrabackup_target_dir[0] = FN_CURLIB;  // all paths are relative from here
  xtrabackup_target_dir[1] = 0;

  sprintf(metadata_path, "%s/%s", xtrabackup_target_dir,
          XTRABACKUP_METADATA_FILENAME);

  if (!xtrabackup_read_metadata(metadata_path)) {
    xb::error() << "failed to read metadata from " << SQUOTE(metadata_path);
    exit(EXIT_FAILURE);
  }
  /* read xtrabackup_info file to read server version and version of
   * xtrabackup used during backup */
  sprintf(xtrabackup_info_path, "%s/%s", xtrabackup_target_dir,
          XTRABACKUP_INFO);

  if (!xtrabackup_read_info(xtrabackup_info_path)) {
    xb::error() << "Failed to parse xtrabackup_info from "
                << SQUOTE(xtrabackup_info_path);
    exit(EXIT_FAILURE);
  }
}

static void xtrabackup_prepare_func(int argc, char **argv) {
  ulint err;
  datafiles_iter_t *it;
  fil_node_t *node;
  fil_space_t *space;
  IORequest write_request(IORequest::WRITE);

  read_metadata();

  /* prepare version check */
  if (!check_server_version(xb_server_version, mysql_server_version_str,
                            mysql_server_flavor, mysql_server_version_str))
    exit(EXIT_FAILURE);

  if (!strcmp(metadata_type_str, "full-backuped")) {
    xb::info() << "This target seems to be not prepared yet.";
    metadata_type = METADATA_FULL_BACKUP;
  } else if (!strcmp(metadata_type_str, "log-applied")) {
    xb::info() << "This target seems to be already prepared with "
                  "--apply-log-only.";
    metadata_type = METADATA_LOG_APPLIED;
    goto skip_check;
  } else if (!strcmp(metadata_type_str, "full-prepared")) {
    xb::info() << "This target seems to be already prepared.";
    metadata_type = METADATA_FULL_PREPARED;
  } else {
    xb::info() << "This target seems not to have correct metadata...";
    exit(EXIT_FAILURE);
  }

  if (xtrabackup_incremental) {
    xb::error() << "applying incremental backup needs target prepared "
                   "with --apply-log-only.";
    exit(EXIT_FAILURE);
  }
skip_check:
  if (xtrabackup_incremental && metadata_to_lsn != incremental_lsn) {
    xb::error() << "This incremental backup seems not to be proper for "
                   "the target.";
    xb::error() << "Check 'to_lsn' of the target and 'from_lsn' of the "
                   "incremental.";
    exit(EXIT_FAILURE);
  }

  if (xtrabackup_incremental) {
    backup_redo_log_flushed_lsn = incremental_flushed_lsn;
  }

  /* Handle DDL files produced by DDL tracking during backup */
  xb_process_datadir(
      xtrabackup_incremental_dir ? xtrabackup_incremental_dir : ".", ".del",
      prepare_handle_del_files, NULL);
  xb_process_datadir(
      xtrabackup_incremental_dir ? xtrabackup_incremental_dir : ".", ".ren",
      prepare_handle_ren_files, NULL);
  if (xtrabackup_incremental_dir) {
    /** This is the new file, this might be less than the original .ibd because
     * we are copying the file while there are still dirty pages in the BP.
     * Those changes will later be conciliated via redo log*/
    xb_process_datadir(xtrabackup_incremental_dir, ".new.meta",
                       prepare_handle_new_files, NULL);
    xb_process_datadir(xtrabackup_incremental_dir, ".new.delta",
                       prepare_handle_new_files, NULL);
    xb_process_datadir(xtrabackup_incremental_dir, ".new",
                       prepare_handle_new_files, NULL);
  } else {
    xb_process_datadir(".", ".new", prepare_handle_new_files, NULL);
  }

  init_mysql_environment();
  my_thread_init();
  THD *thd = create_internal_thd();

  /* Create logfiles for recovery from 'xtrabackup_logfile', before start InnoDB
   */
  srv_max_n_threads = 1000;
  /* temporally dummy value to avoid crash */
  srv_page_size_shift = 14;
  srv_page_size = (1 << srv_page_size_shift);
  srv_redo_log_capacity = 1024 * 1024 * 1024;  // default 1G
  srv_redo_log_capacity_used = 0;
  os_event_global_init();
  sync_check_init(srv_max_n_threads);
#ifdef UNIV_DEBUG
  sync_check_enable();
#endif
  os_thread_open();
  trx_pool_init();
  ut_crc32_init();
  clone_init();

  xb_filters_init();

  if (xtrabackup_init_temp_log()) goto error_cleanup;

  if (innodb_init_param()) {
    goto error_cleanup;
  }

  if (opt_transition_key && !xb_tablespace_keys_exist()) {
    xb::error() << "--transition-key specified, but "
                   "xtrabackup_keys is not found.";
    goto error_cleanup;
  }

  if (!xtrabackup::utils::read_server_uuid()) goto error_cleanup;

  if (opt_transition_key) {
    if (!xb_tablespace_keys_load(xtrabackup_incremental, opt_transition_key,
                                 strlen(opt_transition_key))) {
      xb::error() << "failed to load tablespace keys";
      goto error_cleanup;
    }
  } else {
    /* Initialize keyrings */
    if (!xtrabackup::components::keyring_init_offline()) {
      xb::error() << "failed to init keyring component";
      goto error_cleanup;
    }
    if (!xtrabackup::components::keyring_component_initialized &&
        !xb_keyring_init_for_prepare(argc, argv)) {
      xb::error() << "failed to init keyring plugin";
      goto error_cleanup;
    }
    xtrabackup::components::inititialize_service_handles();
    if (xb_tablespace_keys_exist()) {
      use_dumped_tablespace_keys = true;
      if (!xb_tablespace_keys_load(xtrabackup_incremental, NULL, 0)) {
        xb::error() << "failed to load tablespace keys";
        goto error_cleanup;
      }
    }
  }

  if (!validate_options(
          (std::string(xtrabackup_target_dir) + "backup-my.cnf").c_str(),
          orig_argc, orig_argv)) {
    exit(EXIT_FAILURE);
  }

  xb_normalize_init_values();

  Tablespace_map::instance().deserialize("./");

  if (xtrabackup_incremental) {
    Tablespace_map::instance().deserialize(xtrabackup_incremental_dir);
    err = xb_data_files_init();
    if (err != DB_SUCCESS) {
      xb::error() << "xb_data_files_init() failed "
                  << "with error code " << err;
      goto error_cleanup;
    }
    inc_dir_tables_hash = ut::new_<hash_table_t>(1000);

    if (!xtrabackup_apply_deltas()) {
      xb_data_files_close();
      xb_filter_hash_free(inc_dir_tables_hash);
      goto error_cleanup;
    }

    xb_data_files_close();

    /* Cleanup datadir from tablespaces deleted between full and
    incremental backups */

    xb_process_datadir("./", ".ibd", rm_if_not_found, NULL);
    xb_process_datadir("./", ".ibu", rm_if_not_found, NULL);

    xb_filter_hash_free(inc_dir_tables_hash);
  }
  clone_free();
  fil_close();

  trx_pool_close();

  os_thread_close();

  sync_check_close();
  os_event_global_destroy();

  innodb_free_param();

  /* Reset the configuration as it might have been changed by
  xb_data_files_init(). */
  if (innodb_init_param()) {
    goto error_cleanup;
  }

  srv_apply_log_only = (bool)xtrabackup_apply_log_only;


  xb::info() << "Starting InnoDB instance for recovery.";
  xb::info() << "Using " << xtrabackup_use_memory
             << " bytes for buffer pool (set by "
             << ((estimate_memory) ? "--use-free-memory-pct" : "--use-memory")
             << " parameter)";

  if (innodb_init(true, true)) {
    goto error_cleanup;
  }

  it = datafiles_iter_new(nullptr);
  if (it == NULL) {
    xb::info() << "datafiles_iter_new() failed.";
    exit(EXIT_FAILURE);
  }

  while ((node = datafiles_iter_next(it)) != NULL) {
    byte *header;
    ulint size;
    mtr_t mtr;
    buf_block_t *block;

    space = node->space;

    /* Align space sizes along with fsp header. We want to process
    each space once, so skip all nodes except the first one in a
    multi-node space. */
    if (node != &space->files.front()) {
      continue;
    }

    mtr_start(&mtr);

    mtr_s_lock(fil_space_get_latch(space->id), &mtr, UT_LOCATION_HERE);

    block = buf_page_get(page_id_t(space->id, 0), page_size_t(space->flags),
                         RW_S_LATCH, UT_LOCATION_HERE, &mtr);
    header = FSP_HEADER_OFFSET + buf_block_get_frame(block);

    size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr);

    mtr_commit(&mtr);

    bool res = fil_space_extend(space, size);

    ut_a(res);
  }

  datafiles_iter_free(it);

  if (xtrabackup_export) {
    xb::info() << "export option is specified.";

    /* flush insert buffer at shutdwon */
    innobase_fast_shutdown = 0;

    it = datafiles_iter_new(nullptr);
    if (it == NULL) {
      xb::error() << "datafiles_iter_new() "
                     "failed.";
      exit(EXIT_FAILURE);
    }

    while ((node = datafiles_iter_next(it)) != NULL) {
      space = node->space;

      /* treat file_per_table only */
      if (!fsp_is_file_per_table(space->id, space->flags)) {
        continue;
      }

      auto result = xb::prepare::dict_load_from_spaces_sdi(space->id);
      dberr_t err = std::get<0>(result);
      auto table_vec = std::get<1>(result);
      if (err != DB_SUCCESS) {
        xb::error() << "cannot find dictionary record of table " << space->name;
        ut_ad(table_vec.empty());
        continue;
      }

      // It is possible that partition IBD has multiple tables
      for (auto table : table_vec) {
        mutex_enter(&(dict_sys->mutex));
        /* Write transfer key for tablespace file */
        fil_space_t *sp = fil_space_get(table->space);
        if (!xb_export_cfp_write(table)) {
          goto next_node;
        }

        /* Write MySQL 8.0 .cfg file */
        if (!xb_export_cfg_write(&sp->files[0], table)) {
          goto next_node;
        }

      next_node:
        if (table != nullptr) {
          dd_table_close(table, thd, nullptr, true);
        }
        mutex_exit(&(dict_sys->mutex));
      }
    }

    datafiles_iter_free(it);
  }

  /* Check whether the log is applied enough or not. */
  if ((xtrabackup_incremental && log_get_lsn(*log_sys) < incremental_to_lsn) ||
      (!xtrabackup_incremental && log_get_lsn(*log_sys) < metadata_to_lsn)) {
    xb::error() << "The transaction log file is corrupted.";
    xb::error() << "The log was not applied to the intended LSN!";
    xb::error() << "Log applied to lsn " << log_get_lsn(*log_sys);
    if (xtrabackup_incremental) {
      xb::error() << "The intended lsn is " << incremental_to_lsn;
    } else {
      xb::error() << "The intended lsn is " << metadata_to_lsn;
    }
    exit(EXIT_FAILURE);
  }

  xb_write_galera_info(xtrabackup_incremental);

  if (innodb_end()) goto error_cleanup;

  innodb_free_param();

  /* re-init necessary components */
  os_event_global_init();
  sync_check_init(srv_max_n_threads);
#ifdef UNIV_DEBUG
  sync_check_enable();
#endif
  /* Reset the system variables in the recovery module. */
  os_thread_open();
  trx_pool_init();
  que_init();

  if (xtrabackup_close_temp_log(true)) exit(EXIT_FAILURE);

  /* output to metadata file */
  {
    char filename[FN_REFLEN];

    strcpy(metadata_type_str,
           srv_apply_log_only ? "log-applied" : "full-prepared");

    if (xtrabackup_incremental && metadata_to_lsn < incremental_to_lsn) {
      metadata_to_lsn = incremental_to_lsn;
      metadata_last_lsn = incremental_last_lsn;
    }

    sprintf(filename, "%s/%s", xtrabackup_target_dir,
            XTRABACKUP_METADATA_FILENAME);
    if (!xtrabackup_write_metadata(filename)) {
      xb::error() << "failed to write metadata to " << SQUOTE(filename);
      exit(EXIT_FAILURE);
    }

    if (xtrabackup_extra_lsndir) {
      sprintf(filename, "%s/%s", xtrabackup_extra_lsndir,
              XTRABACKUP_METADATA_FILENAME);
      if (!xtrabackup_write_metadata(filename)) {
        xb::error() << "failed to write metadata to " << SQUOTE(filename);
        exit(EXIT_FAILURE);
      }
    }
  }

  if (!apply_log_finish()) {
    exit(EXIT_FAILURE);
  }

  trx_pool_close();

  fil_close();

  os_thread_close();

  sync_check_close();
  os_event_global_destroy();

  xb_keyring_shutdown();

  destroy_internal_thd(thd);
  my_thread_end();
  Tablespace_map::instance().serialize();

  cleanup_mysql_environment();

  xb_filters_free();

  return;

error_cleanup:

  xb_keyring_shutdown();
  destroy_internal_thd(thd);
  my_thread_end();

  xtrabackup_close_temp_log(false);

  cleanup_mysql_environment();

  xb_filters_free();

  exit(EXIT_FAILURE);
}

/**************************************************************************
Signals-related setup. */
static void setup_signals()
/*===========*/
{
  struct sigaction sa;

  /* Print a stacktrace on some signals */
  sa.sa_flags = SA_RESETHAND | SA_NODEFER;
  sigemptyset(&sa.sa_mask);
  sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);
#ifdef HAVE_STACKTRACE
  my_init_stacktrace();
#endif
  sa.sa_handler = handle_fatal_signal;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);

#ifdef __linux__
  /* Ensure xtrabackup process is killed when the parent one
  (innobackupex) is terminated with an unhandled signal */

  if (prctl(PR_SET_PDEATHSIG, SIGKILL)) {
    xb::error() << "prctl() failed with errno = " << errno;
    exit(EXIT_FAILURE);
  }
#endif
}

/**************************************************************************
Append group name to xb_load_default_groups list. */
static void append_defaults_group(const char *group,
                                  const char *default_groups[],
                                  size_t default_groups_size) {
  uint i;
  bool appended = false;
  for (i = 0; i < default_groups_size - 1; i++) {
    if (default_groups[i] == NULL) {
      default_groups[i] = group;
      appended = true;
      break;
    }
  }
  ut_a(appended);
}

bool xb_init() {
  const char *mixed_options[4] = {NULL, NULL, NULL, NULL};
  int n_mixed_options;

  if (opt_no_lock || opt_no_backup_locks) opt_lock_ddl = LOCK_DDL_OFF;

  /* sanity checks */
  if (opt_lock_ddl != LOCK_DDL_OFF && opt_lock_ddl_per_table) {
    xb::error()
        << "--lock-ddl and --lock-ddl-per-table are mutually exclusive. "
           "Please specify --lock-ddl=OFF to use --lock-ddl-per-table.";
    return (false);
  }

  if (opt_slave_info && opt_no_lock && !opt_safe_slave_backup) {
    xb::error() << "--slave-info is used with --no-lock but "
                   "without --safe-slave-backup. The binlog position "
                   "cannot be consistent with the backup data.";
    return (false);
  }

  if (opt_rsync && xtrabackup_stream_fmt) {
    xb::error() << "--rsync doesn't work with --stream";
    return (false);
  }

  if (opt_transition_key && opt_generate_transition_key) {
    xb::error() << "options --transition-key and "
                << "--generate-transition-key are mutually exclusive.";
    return (false);
  }

  n_mixed_options = 0;

  if (opt_decompress) {
    mixed_options[n_mixed_options++] = "--decompress";
  } else if (opt_decrypt) {
    mixed_options[n_mixed_options++] = "--decrypt";
  }

  if (xtrabackup_copy_back) {
    mixed_options[n_mixed_options++] = "--copy-back";
  }

  if (xtrabackup_move_back) {
    mixed_options[n_mixed_options++] = "--move-back";
  }

  if (xtrabackup_prepare) {
    mixed_options[n_mixed_options++] = "--apply-log";
  }

  if (n_mixed_options > 1) {
    xb::error() << mixed_options[0] << " and " << mixed_options[1]
                << " are mutually exclusive";
    return (false);
  }

  if (xtrabackup_backup) {
#ifdef HAVE_VERSION_CHECK
    if (!opt_noversioncheck) {
      version_check();
    }
#endif

    if ((mysql_connection = xb_mysql_connect()) == NULL) {
      return (false);
    }

    if (!get_mysql_vars(mysql_connection)) {
      return (false);
    }

    if (opt_page_tracking &&
        !pagetracking::is_component_installed(mysql_connection)) {
      xb::error() << "pagetracking: Please install mysqlbackup "
                  << "component.(INSTALL COMPONENT "
                  << "\"file://component_mysqlbackup\") to "
                  << "use page tracking";
      return (false);
    }

    if (opt_lock_ddl_per_table) {
      xb::warn() << "Option --lock-ddl-per-table is deprecated. Please use "
                    "--lock-ddl instead.";
    }

    if (opt_check_privileges) {
      check_all_privileges();
    }

    history_start_time = time(NULL);

    /* stop slave before taking backup up locks if lock-ddl=ON*/
    if (!opt_no_lock && opt_lock_ddl == LOCK_DDL_ON && opt_safe_slave_backup) {
      if (!wait_for_safe_slave(mysql_connection)) {
        return (false);
      }
    }

    if (opt_lock_ddl == LOCK_DDL_ON &&
        !lock_tables_for_backup(mysql_connection, opt_lock_ddl_timeout, 0)) {
      return (false);
    }

    parse_show_engine_innodb_status(mysql_connection);
  }

  return (true);
}

static const char *normalize_privilege_target_name(const char *name) {
  if (strcmp(name, "*") == 0) {
    return "\\*";
  } else {
    /* should have no regex special characters. */
    ut_ad(strpbrk(name, ".()[]*+?") == 0);
  }
  return name;
}

/******************************************************************/ /**
 Check if specific privilege is granted.
 Uses regexp magic to check if requested privilege is granted for given
 database.table or database.* or *.*
 or if user has 'ALL PRIVILEGES' granted.
 @return true if requested privilege is granted, false otherwise. */
static bool has_privilege(const std::list<std::string> &granted,
                          const char *required, const char *db_name,
                          const char *table_name) {
  char buffer[1000];
  xb_regex_t priv_re;
  xb_regmatch_t tables_regmatch[1];
  bool result = false;

  db_name = normalize_privilege_target_name(db_name);
  table_name = normalize_privilege_target_name(table_name);

  int written =
      snprintf(buffer, sizeof(buffer),
               "GRANT .*(%s)|(ALL PRIVILEGES).* ON (\\*|`%s`)\\.(\\*|`%s`)",
               required, db_name, table_name);
  if (written < 0 || written == sizeof(buffer) ||
      !compile_regex(buffer, "has_privilege", &priv_re)) {
    exit(EXIT_FAILURE);
  }

  typedef std::list<std::string>::const_iterator string_iter;
  for (string_iter i = granted.begin(), e = granted.end(); i != e; ++i) {
    int res = xb_regexec(&priv_re, i->c_str(), 1, tables_regmatch, 0);

    if (res != REG_NOMATCH) {
      result = true;
      break;
    }
  }

  xb_regfree(&priv_re);
  return result;
}

enum {
  PRIVILEGE_OK = 0,
  PRIVILEGE_WARNING = 1,
  PRIVILEGE_ERROR = 2,
};

/******************************************************************/ /**
 Check if specific privilege is granted.
 Prints error message if required privilege is missing.
 @return PRIVILEGE_OK if requested privilege is granted, error otherwise. */
static int check_privilege(
    const std::list<std::string> &granted_priv, /* in: list of
                                                    granted privileges*/
    const char *required,        /* in: required privilege name */
    const char *target_database, /* in: required privilege target
                                         database name */
    const char *target_table,    /* in: required privilege target
                                         table name */
    int error = PRIVILEGE_ERROR) /* in: return value if privilege
                                         is not granted */
{
  if (!has_privilege(granted_priv, required, target_database, target_table)) {
    std::stringstream str;
    str << " missing required privilege " << required << " on "
        << target_database << "." << target_table;
    if (error == PRIVILEGE_ERROR) {
      xb::error() << str.str();
    } else {
      xb::warn() << str.str();
    }

    return error;
  }
  return PRIVILEGE_OK;
}

/******************************************************************/ /**
 Check DB user privileges according to the intended actions.

 Fetches DB user privileges, determines intended actions based on
 command-line arguments and prints missing privileges.
 May terminate application with EXIT_FAILURE exit code.*/
static void check_all_privileges() {
  if (!mysql_connection) {
    /* Not connected, no queries is going to be executed. */
    return;
  }

  /* Fetch effective privileges. */
  std::list<std::string> granted_privileges;
  MYSQL_ROW row = 0;
  MYSQL_RES *result = xb_mysql_query(mysql_connection, "SHOW GRANTS", true);
  while ((row = mysql_fetch_row(result))) {
    granted_privileges.push_back(*row);
  }
  mysql_free_result(result);

  int check_result = PRIVILEGE_OK;
  bool reload_checked = false;

  /* SELECT FROM P_S.LOG_STATUS */
  check_result |= check_privilege(granted_privileges, "BACKUP_ADMIN", "*", "*");
  check_result |= check_privilege(granted_privileges, "SELECT",
                                  "PERFORMANCE_SCHEMA", "LOG_STATUS");

  /* SHOW DATABASES */
  check_result |=
      check_privilege(granted_privileges, "SHOW DATABASES", "*", "*");

  /* SELECT 'INNODB_CHANGED_PAGES', COUNT(*) FROM INFORMATION_SCHEMA.PLUGINS */
  check_result |= check_privilege(granted_privileges, "SELECT",
                                  "INFORMATION_SCHEMA", "PLUGINS");

  /* SHOW ENGINE INNODB STATUS */
  /* SHOW FULL PROCESSLIST */
  check_result |= check_privilege(granted_privileges, "PROCESS", "*", "*");

  if (xb_mysql_numrows(mysql_connection,
                       "SHOW DATABASES LIKE 'PERCONA_SCHEMA';", false) == 0) {
    /* CREATE DATABASE IF NOT EXISTS PERCONA_SCHEMA */
    check_result |= check_privilege(granted_privileges, "CREATE", "*", "*");
  } else if (xb_mysql_numrows(mysql_connection,
                              "SHOW TABLES IN PERCONA_SCHEMA "
                              "LIKE 'xtrabackup_history';",
                              false) == 0) {
    /* CREATE TABLE IF NOT EXISTS PERCONA_SCHEMA.xtrabackup_history */
    check_result |=
        check_privilege(granted_privileges, "CREATE", "PERCONA_SCHEMA", "*");
  }

  /* FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS */
  if (have_flush_engine_logs
      /* FLUSH NO_WRITE_TO_BINLOG TABLES */
      ||
      (opt_lock_wait_timeout && !opt_kill_long_queries_timeout && !opt_no_lock)
      /* FLUSH TABLES WITH READ LOCK */
      || !opt_no_lock
      /* LOCK BINLOG FOR BACKUP */
      /* UNLOCK BINLOG */
      || (have_backup_locks && !opt_no_lock)) {
    check_result |= check_privilege(granted_privileges, "RELOAD", "*", "*");
    reload_checked = true;
  }

  /* FLUSH TABLES WITH READ LOCK */
  if (!opt_no_lock
      /* LOCK TABLES FOR BACKUP */
      /* UNLOCK TABLES */
      && ((have_backup_locks && !opt_no_lock) || opt_slave_info)) {
    check_result |=
        check_privilege(granted_privileges, "LOCK TABLES", "*", "*");
  }

  /* SELECT innodb_to_lsn FROM PERCONA_SCHEMA.xtrabackup_history ... */
  if (opt_incremental_history_name || opt_incremental_history_uuid) {
    check_result |= check_privilege(granted_privileges, "SELECT",
                                    "PERCONA_SCHEMA", "xtrabackup_history");
  }

  if (!reload_checked
      /* FLUSH BINARY LOGS */
      && opt_galera_info) {
    check_result |= check_privilege(granted_privileges, "RELOAD", "*", "*",
                                    PRIVILEGE_WARNING);
  }

  /* KILL ... */
  if (opt_kill_long_queries_timeout
      /* START SLAVE SQL_THREAD */
      /* STOP SLAVE SQL_THREAD */
      || opt_safe_slave_backup) {
    check_result |= check_privilege(granted_privileges, "SUPER", "*", "*",
                                    PRIVILEGE_WARNING);
  }

  /* SHOW MASTER STATUS */
  /* SHOW SLAVE STATUS */
  if (opt_galera_info || opt_slave_info ||
      (opt_no_lock && opt_safe_slave_backup)
      /* LOCK BINLOG FOR BACKUP */
      || (have_backup_locks && !opt_no_lock)) {
    check_result |= check_privilege(granted_privileges, "REPLICATION CLIENT",
                                    "*", "*", PRIVILEGE_WARNING);
  }

  if (check_result & PRIVILEGE_ERROR) {
    exit(EXIT_FAILURE);
  }
}

/** Validate xtrabackup options. Only validates command line arguments and
options specified in [xtrabackup] section of my.cnf.
@param[in]  file        config file basename for load_defaults
@param[in]  argc        program's argc
@param[in]  argv        program's argv
@return true if no errors found */
static bool validate_options(const char *file, int argc, char **argv) {
  int my_argc = argc;
  auto my_argv_buf = std::unique_ptr<char *[]>(new char *[my_argc]);
  char **my_argv = my_argv_buf.get();

  for (int i = 0; i < my_argc; ++i) {
    my_argv[i] = argv[i];
  }

  const char *groups[] = {"xtrabackup", 0};
  MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

  if (load_defaults(file, groups, &my_argc, &my_argv, &argv_alloc)) {
    return (false);
  }

  /* options consumed by plugins */
  std::vector<my_option> plugin_options;
  add_plugin_options(&plugin_options, &argv_alloc);

  my_option last_option = {0,      0, 0, 0, 0, 0, GET_NO_ARG,
                           NO_ARG, 0, 0, 0, 0, 0, 0};
  plugin_options.push_back(last_option);

  /* these options are not members of global my_option, but are recognised by
  xtrabackup */
  my_option my_extra_options[] = {
      {"no-defaults", 0, nullptr, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0,
       0},
      {"login-path", 0, nullptr, nullptr, nullptr, 0, GET_STR, REQUIRED_ARG, 0,
       0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  };

  const auto tmp_getopt_skip_unknown = my_getopt_skip_unknown;
  my_getopt_skip_unknown = true;

  if (my_argc > 0) {
    if (my_handle_options(&my_argc, &my_argv, xb_client_options, nullptr,
                          nullptr, false, true)) {
      return (false);
    }
  }

  const auto restore_argv0 = [argv](int *c, char **v) {
    for (int i = *c - 1; i >= 0; --i) {
      v[i + 1] = v[i];
    }
    v[0] = argv[0];
    ++(*c);
  };

  if (my_argc > 0) {
    restore_argv0(&my_argc, my_argv);

    if (my_handle_options(&my_argc, &my_argv, xb_server_options, nullptr,
                          nullptr, false, true)) {
      return (false);
    }
  }

  if (plugin_options.size() == 0) {
    my_getopt_skip_unknown = false;
  }

  if (my_argc > 0) {
    restore_argv0(&my_argc, my_argv);
    if (my_handle_options(&my_argc, &my_argv, my_extra_options, nullptr,
                          nullptr, false, true)) {
      return (false);
    }
  }

  my_getopt_skip_unknown = !opt_strict;

  if (my_argc > 0 && plugin_options.size() > 0) {
    restore_argv0(&my_argc, my_argv);
    if (my_handle_options(&my_argc, &my_argv, &plugin_options[0], nullptr,
                          nullptr, false, true)) {
      return (false);
    }
  }

  if (!opt_strict && my_argc > 0) {
    for (int i = 0; i < my_argc; ++i) {
      if (strncmp(my_argv[i], "--", 2) == 0) {
        xb::warn() << "unknown option " << my_argv[i];
      }
    }
  }

  my_getopt_skip_unknown = tmp_getopt_skip_unknown;

  return (true);
}

static void handle_options(int argc, char **argv, int *argc_client,
                           char ***argv_client, int *argc_server,
                           char ***argv_server) {
  int i;
  int ho_error;
  char conf_file[FN_REFLEN];

  const char *target_dir = NULL;
  bool prepare = false;

  *argc_client = argc;
  *argc_server = argc;
  *argv_client = argv;
  *argv_server = argv;

  /* scan options for group and config file to load defaults from */
  for (i = 1; i < argc; i++) {
    const char *optend = strcend(argv[i], '=');

    if (strncmp(argv[i], "--defaults-group", optend - argv[i]) == 0) {
      defaults_group = optend + 1;
      append_defaults_group(defaults_group, xb_server_default_groups,
                            array_elements(xb_server_default_groups));
    }

    if (strncmp(argv[i], "--login-path", optend - argv[i]) == 0) {
      append_defaults_group(optend + 1, xb_client_default_groups,
                            array_elements(xb_client_default_groups));
    }

    if (!strncmp(argv[i], "--prepare", optend - argv[i])) {
      prepare = true;
    }

    if (!strncmp(argv[i], "--apply-log", optend - argv[i])) {
      prepare = true;
    }

    if (!strncmp(argv[i], "--target-dir", optend - argv[i]) && *optend) {
      target_dir = optend + 1;
    }

    if (!*optend && argv[i][0] != '-') {
      target_dir = argv[i];
    }
  }

  snprintf(conf_file, sizeof(conf_file), "my");

  if (prepare && target_dir) {
    snprintf(conf_file, sizeof(conf_file), "%s/backup-my.cnf", target_dir);
  }

  if (load_defaults(conf_file, xb_server_default_groups, argc_server,
                    argv_server, &argv_alloc)) {
    exit(EXIT_FAILURE);
  }

  print_param_str << "# This MySQL options file was generated by XtraBackup.\n"
                     "["
                  << defaults_group << "]\n";

  /* We want xtrabackup to ignore unknown options, because it only
  recognizes a small subset of server variables */
  my_getopt_skip_unknown = true;

  /* Reset u_max_value for all options, as we don't want the
  --maximum-... modifier to set the actual option values */
  for (my_option *optp = xb_server_options; optp->name; optp++) {
    optp->u_max_value = (G_PTR *)&global_max_value;
  }

  /* Throw a descriptive error if --defaults-file or --defaults-extra-file
  is not the first command line argument */
  for (int i = 2; i < argc; i++) {
    const char *optend = strcend((argv)[i], '=');

    if (optend - argv[i] == 15 &&
        !strncmp(argv[i], "--defaults-file", optend - argv[i])) {
      xb::error() << "--defaults-file must be specified first "
                     "on the command line";
      exit(EXIT_FAILURE);
    }
    if (optend - argv[i] == 21 &&
        !strncmp(argv[i], "--defaults-extra-file", optend - argv[i])) {
      xb::error() << "--defaults-extra-file must be specified first "
                     "on the command line";
      exit(EXIT_FAILURE);
    }
  }

  if (*argc_server > 0 &&
      (ho_error = handle_options(argc_server, argv_server, xb_server_options,
                                 xb_get_one_option)))
    exit(ho_error);

  if (!param_str.str().empty()) {
    xb::info() << "recognized server arguments: " << param_str.str().c_str();
    param_str.str("");
    param_str.clear();
  }

  if (load_defaults(conf_file, xb_client_default_groups, argc_client,
                    argv_client, &argv_alloc)) {
    exit(EXIT_FAILURE);
  }

  if (*argc_client > 0 &&
      (ho_error = handle_options(argc_client, argv_client, xb_client_options,
                                 xb_get_one_option)))
    exit(ho_error);

  if (!param_str.str().empty()) {
    xb::info() << "recognized client arguments: " << param_str.str().c_str();
    param_str.clear();
  }

  /* Reject command line arguments that don't look like options, i.e. are
  not of the form '-X' (single-character options) or '--option' (long
  options) */
  for (int i = 0; i < *argc_client; i++) {
    const char *const opt = (*argv_client)[i];

    if (strncmp(opt, "--", 2) && !(strlen(opt) == 2 && opt[0] == '-')) {
      bool server_option = true;

      for (int j = 0; j < *argc_server; j++) {
        if (opt == (*argv_server)[j]) {
          server_option = false;
          break;
        }
      }

      if (!server_option) {
        xb::error() << "unknown argument: " << SQUOTE(opt);
        exit(EXIT_FAILURE);
      }
    }
  }

  if (tty_password) {
    opt_password = get_tty_password(NullS);
  }

  if (tty_transition_key) {
    opt_transition_key = get_tty_password("Enter transition key: ");
  }
}

void setup_error_messages() {
  my_default_lc_messages = &my_locale_en_US;
  my_default_lc_messages->errmsgs->read_texts();
}

void xb_set_plugin_dir() {
  if (opt_xtra_plugin_dir != NULL) {
    strncpy(opt_plugin_dir, opt_xtra_plugin_dir, FN_REFLEN - 1);
  } else {
    strcpy(opt_plugin_dir, PLUGINDIR);
  }
}

/* ================= main =================== */

int main(int argc, char **argv) {
  char **client_defaults, **server_defaults;
  int client_argc, server_argc;
  char cwd[FN_REFLEN];

  orig_argc = argc;
  orig_argv = argv;

  /* Logs xtrabackup generated timestamps in local timezone instead of UTC */
  opt_log_timestamps = 1;
  /* This variable determines the size of dict_table_t* cache in InnoDB. This is
  default and it is sufficient because rollback is single threaded and we only
  open tables one by one */
  table_def_size = 4000;

  setup_signals();

  MY_INIT(argv[0]);

  current_thd = NULL;

  xb_regex_init();

  capture_tool_command(argc, argv);

  if (mysql_server_init(-1, NULL, NULL)) {
    exit(EXIT_FAILURE);
  }

  /* Setup logging before handle_options(), so that error
  generated by handle_options() are displayed to user */
  system_charset_info = &my_charset_utf8mb3_general_ci;
  files_charset_info = &my_charset_utf8mb3_general_ci;
  national_charset_info = &my_charset_utf8mb3_general_ci;
  table_alias_charset = &my_charset_bin;
  character_set_filesystem = &my_charset_bin;

  sys_var_init();
  setup_error_messages();

  init_error_log();
  setup_error_log_components();

  handle_options(argc, argv, &client_argc, &client_defaults, &server_argc,
                 &server_defaults);

  print_version();

  atexit(destroy_error_log);

  if (xtrabackup_encrypt) {
    xb_libgcrypt_init();
  }

  if ((!xtrabackup_print_param) && (!xtrabackup_prepare) &&
      (strcmp(mysql_data_home, "./") == 0)) {
    if (!xtrabackup_print_param) usage();
    xb::error() << "Please set parameter 'datadir'";
    exit(EXIT_FAILURE);
  }

  if (xtrabackup_fifo_streams_set && xtrabackup_fifo_dir == NULL &&
      strcmp(xtrabackup_target_dir, "./xtrabackup_backupfiles/") == 0) {
    xb::error() << "Option --fifo-streams requires --fifo-dir to be set.";
    exit(EXIT_FAILURE);
  }

  /* Expand target-dir, incremental-basedir, etc. */

  my_getwd(cwd, sizeof(cwd), MYF(0));

  my_load_path(xtrabackup_real_target_dir, xtrabackup_target_dir, cwd);
  unpack_dirname(xtrabackup_real_target_dir, xtrabackup_real_target_dir);
  xtrabackup_target_dir = xtrabackup_real_target_dir;
  xb_set_plugin_dir();
  if (xtrabackup_incremental_basedir) {
    my_load_path(xtrabackup_real_incremental_basedir,
                 xtrabackup_incremental_basedir, cwd);
    unpack_dirname(xtrabackup_real_incremental_basedir,
                   xtrabackup_real_incremental_basedir);
    xtrabackup_incremental_basedir = xtrabackup_real_incremental_basedir;
  }

  if (xtrabackup_incremental_dir) {
    my_load_path(xtrabackup_real_incremental_dir, xtrabackup_incremental_dir,
                 cwd);
    unpack_dirname(xtrabackup_real_incremental_dir,
                   xtrabackup_real_incremental_dir);
    xtrabackup_incremental_dir = xtrabackup_real_incremental_dir;
  }

  if (xtrabackup_extra_lsndir) {
    my_load_path(xtrabackup_real_extra_lsndir, xtrabackup_extra_lsndir, cwd);
    unpack_dirname(xtrabackup_real_extra_lsndir, xtrabackup_real_extra_lsndir);
    xtrabackup_extra_lsndir = xtrabackup_real_extra_lsndir;
  }

  /* get default temporary directory */
  if (!opt_mysql_tmpdir || !opt_mysql_tmpdir[0]) {
    opt_mysql_tmpdir = getenv("TMPDIR");
#if defined(__WIN__)
    if (!opt_mysql_tmpdir) {
      opt_mysql_tmpdir = getenv("TEMP");
    }
    if (!opt_mysql_tmpdir) {
      opt_mysql_tmpdir = getenv("TMP");
    }
#endif
    if (!opt_mysql_tmpdir || !opt_mysql_tmpdir[0]) {
      opt_mysql_tmpdir = const_cast<char *>(DEFAULT_TMPDIR);
    }
  }

  /* temporary setting of enough size */
  srv_page_size_shift = UNIV_PAGE_SIZE_SHIFT_MAX;
  srv_page_size = UNIV_PAGE_SIZE_MAX;
  if (xtrabackup_backup && xtrabackup_incremental) {
    /* direct specification is only for --backup */
    /* and the lsn is prior to the other option */

    char *endchar;
    int error = 0;
    incremental_lsn = strtoll(xtrabackup_incremental, &endchar, 10);
    if (*endchar != '\0') error = 1;

    if (error) {
      xb::error() << "value " << SQUOTE(xtrabackup_incremental)
                  << " may be wrong format for incremental option.";
      exit(EXIT_FAILURE);
    }
  } else if (xtrabackup_backup && xtrabackup_incremental_basedir) {
    char filename[FN_REFLEN];

    sprintf(filename, "%s/%s", xtrabackup_incremental_basedir,
            XTRABACKUP_METADATA_FILENAME);

    if (!xtrabackup_read_metadata(filename)) {
      xb::error() << "failed to read metadata from " << filename;
      exit(EXIT_FAILURE);
    }

    incremental_lsn = metadata_to_lsn;
    xtrabackup_incremental = xtrabackup_incremental_basedir;  // dummy
  } else if (xtrabackup_prepare && xtrabackup_incremental_dir) {
    char filename[FN_REFLEN];

    sprintf(filename, "%s/%s", xtrabackup_incremental_dir,
            XTRABACKUP_METADATA_FILENAME);

    if (!xtrabackup_read_metadata(filename)) {
      xb::error() << "failed to read metadata from " << filename;
      exit(EXIT_FAILURE);
    }

    incremental_lsn = metadata_from_lsn;
    incremental_to_lsn = metadata_to_lsn;
    incremental_last_lsn = metadata_last_lsn;
    incremental_flushed_lsn = backup_redo_log_flushed_lsn;
    xtrabackup_incremental = xtrabackup_incremental_dir;  // dummy
    incremental_redo_memory = redo_memory;
    incremental_redo_frames = redo_frames;

  } else if (opt_incremental_history_name) {
    xtrabackup_incremental = opt_incremental_history_name;
  } else if (opt_incremental_history_uuid) {
    xtrabackup_incremental = opt_incremental_history_uuid;
  } else {
    xtrabackup_incremental = NULL;
  }

  if (!xb_init()) {
    exit(EXIT_FAILURE);
  }

  /* --print-param */
  if (xtrabackup_print_param) {
    printf("%s", print_param_str.str().c_str());

    exit(EXIT_SUCCESS);
  }

  if (xtrabackup_incremental) {
    xb::info() << "incremental backup from " << incremental_lsn
               << " is enabled.";
  }

  if (xtrabackup_export && innobase_file_per_table == false) {
    xb::info() << "auto-enabling --innodb-file-per-table due to "
                  "the --export option";
    innobase_file_per_table = true;
  }

  if (xtrabackup_throttle && !xtrabackup_backup) {
    xtrabackup_throttle = 0;
    xb::warn() << "--throttle has effect only with --backup";
  }

  /* cannot execute both for now */
  {
    int num = 0;

    if (xtrabackup_backup) num++;
    if (xtrabackup_prepare) num++;
    if (xtrabackup_copy_back) num++;
    if (xtrabackup_move_back) num++;
    if (xtrabackup_decrypt_decompress) num++;
    if (num != 1) { /* !XOR (for now) */
      usage();
      exit(EXIT_FAILURE);
    }
  }
  if (opt_component_keyring_file_config != nullptr) {
    xb::warn() << "--component-keyring-file-config is deprecated, please use "
               << " --component-keyring-config instead.";
    if (opt_component_keyring_config != nullptr) {
      xb::error() << "--component-keyring-file-config and "
                  << "--component-keyring-config are mutually exclusive."
                  << "Please use --component-keyring-config.";
      exit(EXIT_FAILURE);
    }
    opt_component_keyring_config =
        static_cast<char *>(opt_component_keyring_file_config);
  }

#ifndef __WIN__
  signal(SIGCONT, sigcont_handler);
#endif

  /* --backup */
  if (xtrabackup_backup) {
    xtrabackup_backup_func();
  }

  /* --prepare */
  if (xtrabackup_prepare) {
    xtrabackup_prepare_func(server_argc, server_defaults);
  }

  if (xtrabackup_copy_back || xtrabackup_move_back) {
    if (!check_if_param_set("datadir")) {
      xb::error() << "datadir must be specified.";
      exit(EXIT_FAILURE);
    }

    /* abort backup  if target is not prepared */
    read_metadata();
    if (strcmp(metadata_type_str, "full-prepared") != 0) {
      xb::error() << "The target is not fully prepared. Please prepare it "
                     "without option --apply-log-only";
      exit(EXIT_FAILURE);
    }

    init_mysql_environment();
    if (!copy_back(server_argc, server_defaults)) {
      exit(EXIT_FAILURE);
    }
    cleanup_mysql_environment();
  }

  if (xtrabackup_decrypt_decompress && !decrypt_decompress()) {
    exit(EXIT_FAILURE);
  }

  backup_cleanup();

  xb_regex_end();

  xb::info() << "completed OK!";

  exit(EXIT_SUCCESS);
}

/* This section ensures file_utils.h remains consistent with InnoDB Layer */
static_assert(XB_FIL_PAGE_TYPE == FIL_PAGE_TYPE,
              "XB_FIL_PAGE_TYPE == FIL_PAGE_TYPE");
static_assert(XB_FIL_PAGE_DATA == FIL_PAGE_DATA,
              "XB_FIL_PAGE_DATA == FIL_PAGE_DATA");
static_assert(XB_FSP_HEADER_OFFSET == FSP_HEADER_OFFSET,
              "XB_FSP_HEADER_OFFSET == FSP_HEADER_OFFSET");
static_assert(XB_FSP_SPACE_FLAGS == FSP_SPACE_FLAGS,
              "XB_FSP_SPACE_FLAGS == FSP_SPACE_FLAGS");
static_assert(XB_FSP_FLAGS_POS_PAGE_SSIZE == FSP_FLAGS_POS_PAGE_SSIZE,
              "XB_FSP_FLAGS_POS_PAGE_SSIZE == FSP_FLAGS_POS_PAGE_SSIZE");
static_assert(XB_FSP_FLAGS_MASK_PAGE_SSIZE == FSP_FLAGS_MASK_PAGE_SSIZE,
              "XB_FSP_FLAGS_MASK_PAGE_SSIZE == FSP_FLAGS_MASK_PAGE_SSIZE");
static_assert(XB_FIL_PAGE_COMPRESSED == FIL_PAGE_COMPRESSED,
              "XB_FIL_PAGE_COMPRESSED == FIL_PAGE_COMPRESSED");
static_assert(XB_FIL_PAGE_COMPRESSED_AND_ENCRYPTED ==
                  FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
              "XB_FIL_PAGE_COMPRESSED_AND_ENCRYPTED == "
              "FIL_PAGE_COMPRESSED_AND_ENCRYPTED");
static_assert(XB_FIL_PAGE_COMPRESS_SIZE_V1 == FIL_PAGE_COMPRESS_SIZE_V1,
              "XB_FIL_PAGE_COMPRESS_SIZE_V1 == FIL_PAGE_COMPRESS_SIZE_V1");
static_assert(XB_UNIV_PAGE_SIZE_ORIG == UNIV_PAGE_SIZE_ORIG,
              "XB_UNIV_PAGE_SIZE_ORIG == UNIV_PAGE_SIZE_ORIG");
static_assert(XB_UNIV_ZIP_SIZE_MIN == UNIV_ZIP_SIZE_MIN,
              "XB_UNIV_ZIP_SIZE_MIN == UNIV_ZIP_SIZE_MIN");
static_assert(XB_UNIV_PAGE_SIZE_MAX == UNIV_PAGE_SIZE_MAX,
              "XB_UNIV_PAGE_SIZE_MAX == UNIV_PAGE_SIZE_MAX");
static_assert(Log_format::CURRENT == Log_format::VERSION_8_0_30,
              "Log_format::CURRENT == Log_format::VERSION_8_0_30");
