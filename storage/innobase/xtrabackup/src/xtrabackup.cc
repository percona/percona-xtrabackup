/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009-2017 Percona LLC and/or its affiliates
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

//#define XTRABACKUP_TARGET_IS_PLUGIN

#include <mysql_version.h>
#include <my_base.h>
#include <my_getopt.h>
#include <mysql_com.h>
#include <my_default.h>
#include <mysqld.h>
#include <sql_bitmap.h>

#include <signal.h>
#include <fcntl.h>
#include <string.h>

#ifdef __linux__
# include <sys/prctl.h>
#endif

#include <sys/resource.h>

#include <btr0sea.h>
#include <dict0priv.h>
#include <dict0stats.h>
#include <lock0lock.h>
#include <log0recv.h>
#include <row0mysql.h>
#include <row0quiesce.h>
#include <srv0start.h>
#include <buf0dblwr.h>
#include <my_aes.h>
#include <sql_locale.h>

#include <list>
#include <sstream>
#include <set>
#include <mysql.h>

#define G_PTR uchar*

#include "common.h"
#include "xtrabackup_version.h"
#include "datasink.h"

#include "xb_regex.h"
#include "fil_cur.h"
#include "write_filt.h"
#include "xtrabackup.h"
#include "ds_buffer.h"
#include "ds_tmpfile.h"
#include "xbstream.h"
#include "changed_page_bitmap.h"
#include "read_filt.h"
#include "wsrep.h"
#include "innobackupex.h"
#include "backup_mysql.h"
#include "backup_copy.h"
#include "backup_mysql.h"
#include "keyring_plugins.h"
#include "xb0xb.h"
#include "ds_encrypt.h"
#include "xbcrypt_common.h"
#include "crc_glue.h"
#include "xtrabackup_config.h"

/* TODO: replace with appropriate macros used in InnoDB 5.6 */
#define PAGE_ZIP_MIN_SIZE_SHIFT	10
#define DICT_TF_ZSSIZE_SHIFT	1
#define DICT_TF_FORMAT_ZIP	1
#define DICT_TF_FORMAT_SHIFT		5

int sys_var_init();

my_bool innodb_inited= 0;


/* This tablespace name is reserved by InnoDB for the system tablespace
which uses space_id 0 and stores extra types of system pages like UNDO
and doublewrite. */
const char reserved_system_space_name[] = "innodb_system";

/* This tablespace name is reserved by InnoDB for the predefined temporary
tablespace. */
const char reserved_temporary_space_name[] = "innodb_temporary";

/* === xtrabackup specific options === */
char xtrabackup_real_target_dir[FN_REFLEN] = "./xtrabackup_backupfiles/";
char *xtrabackup_target_dir= xtrabackup_real_target_dir;
my_bool xtrabackup_version = FALSE;
my_bool xtrabackup_backup = FALSE;
my_bool xtrabackup_stats = FALSE;
my_bool xtrabackup_prepare = FALSE;
my_bool xtrabackup_copy_back = FALSE;
my_bool xtrabackup_move_back = FALSE;
my_bool xtrabackup_decrypt_decompress = FALSE;
my_bool xtrabackup_print_param = FALSE;

my_bool xtrabackup_export = FALSE;
my_bool xtrabackup_apply_log_only = FALSE;

longlong xtrabackup_use_memory = 100*1024*1024L;
my_bool xtrabackup_create_ib_logfile = FALSE;

long xtrabackup_throttle = 0; /* 0:unlimited */
lint io_ticket;
os_event_t wait_throttle = NULL;
os_event_t log_copying_stop = NULL;

char *xtrabackup_incremental = NULL;
lsn_t incremental_lsn;
lsn_t incremental_to_lsn;
lsn_t incremental_last_lsn;
lsn_t incremental_flushed_lsn;
xb_page_bitmap *changed_page_bitmap = NULL;

char *xtrabackup_incremental_basedir = NULL; /* for --backup */
char *xtrabackup_extra_lsndir = NULL; /* for --backup with --extra-lsndir */
char *xtrabackup_incremental_dir = NULL; /* for --prepare */

char xtrabackup_real_incremental_basedir[FN_REFLEN];
char xtrabackup_real_extra_lsndir[FN_REFLEN];
char xtrabackup_real_incremental_dir[FN_REFLEN];

lsn_t xtrabackup_archived_to_lsn = 0; /* for --archived-to-lsn */

char *xtrabackup_tables = NULL;
char *xtrabackup_tables_file = NULL;
char *xtrabackup_tables_exclude = NULL;

typedef std::list<xb_regex_t> regex_list_t;
static regex_list_t regex_include_list;
static regex_list_t regex_exclude_list;

static hash_table_t* tables_include_hash = NULL;
static hash_table_t* tables_exclude_hash = NULL;

char *xtrabackup_databases = NULL;
char *xtrabackup_databases_file = NULL;
char *xtrabackup_databases_exclude = NULL;
static hash_table_t* databases_include_hash = NULL;
static hash_table_t* databases_exclude_hash = NULL;

static hash_table_t* inc_dir_tables_hash;

struct xb_filter_entry_struct{
	char*		name;
	ibool		has_tables;
	hash_node_t	name_hash;
};
typedef struct xb_filter_entry_struct	xb_filter_entry_t;

static ulint		thread_nr[SRV_MAX_N_IO_THREADS + 6];
static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 6];

lsn_t checkpoint_lsn_start;
lsn_t checkpoint_no_start;
lsn_t log_copy_scanned_lsn;
ibool log_copying = TRUE;
ibool log_copying_running = FALSE;
ibool io_watching_thread_running = FALSE;

ibool xtrabackup_logfile_is_renamed = FALSE;

int xtrabackup_parallel;

char *xtrabackup_stream_str = NULL;
xb_stream_fmt_t xtrabackup_stream_fmt = XB_STREAM_FMT_NONE;
ibool xtrabackup_stream = FALSE;

const char *xtrabackup_compress_alg = NULL;
ibool xtrabackup_compress = FALSE;
uint xtrabackup_compress_threads;
ulonglong xtrabackup_compress_chunk_size = 0;

const char *xtrabackup_encrypt_algo_names[] =
{ "NONE", "AES128", "AES192", "AES256", NullS};
TYPELIB xtrabackup_encrypt_algo_typelib=
{array_elements(xtrabackup_encrypt_algo_names)-1,"",
	xtrabackup_encrypt_algo_names, NULL};

ibool xtrabackup_encrypt = FALSE;
ulong xtrabackup_encrypt_algo;
char *xtrabackup_encrypt_key = NULL;
char *xtrabackup_encrypt_key_file = NULL;
uint xtrabackup_encrypt_threads;
ulonglong xtrabackup_encrypt_chunk_size = 0;

ulint xtrabackup_rebuild_threads = 1;

/* sleep interval beetween log copy iterations in log copying thread
in milliseconds (default is 1 second) */
ulint xtrabackup_log_copy_interval = 1000;

/* Ignored option (--log) for MySQL option compatibility */
char*	log_ignored_opt				= NULL;

/* === metadata of backup === */
#define XTRABACKUP_METADATA_FILENAME "xtrabackup_checkpoints"
char metadata_type[30] = ""; /*[full-backuped|log-applied|
			     full-prepared|incremental]*/
lsn_t metadata_from_lsn = 0;
lsn_t metadata_to_lsn = 0;
lsn_t metadata_last_lsn = 0;

#define XB_LOG_FILENAME "xtrabackup_logfile"

ds_file_t	*dst_log_file = NULL;

static char mysql_data_home_buff[2];

const char *defaults_group = "mysqld";

/* === static parameters in ha_innodb.cc */

#define HA_INNOBASE_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_INNOBASE_RANGE_COUNT	  100

ulong 	innobase_large_page_size = 0;

/* The default values for the following, type long or longlong, start-up
parameters are declared in mysqld.cc: */

long innobase_buffer_pool_awe_mem_mb = 0;
long innobase_file_io_threads = 4;
long innobase_read_io_threads = 4;
long innobase_write_io_threads = 4;
long innobase_force_recovery = 0;
long innobase_log_buffer_size = 16*1024*1024L;
long innobase_log_files_in_group = 2;
long innobase_open_files = 300L;

longlong innobase_page_size = (1LL << 14); /* 16KB */
static ulong innobase_log_block_size = 512;
my_bool innobase_fast_checksum = FALSE;
char*	innobase_doublewrite_file = NULL;
char*	innobase_buffer_pool_filename = NULL;

longlong innobase_buffer_pool_size = 8*1024*1024L;
longlong innobase_log_file_size = 48*1024*1024L;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

char*	innobase_ignored_opt			= NULL;
char*	innobase_data_home_dir			= NULL;
char*   innobase_data_file_path                 = NULL;
char*   innobase_temp_data_file_path            = NULL;
char*	innobase_log_arch_dir			= NULL;/* unused */
/* The following has a misleading name: starting from 4.0.5, this also
affects Windows: */
char*	innobase_unix_file_flush_method		= NULL;

/* Below we have boolean-valued start-up parameters, and their default
values */

ulong	innobase_fast_shutdown			= 1;
my_bool innobase_log_archive			= FALSE;/* unused */
my_bool innobase_use_doublewrite    = TRUE;
my_bool innobase_use_checksums      = TRUE;
my_bool innobase_use_large_pages    = FALSE;
my_bool	innobase_file_per_table			= FALSE;
my_bool innobase_locks_unsafe_for_binlog        = FALSE;
my_bool innobase_rollback_on_timeout		= FALSE;
my_bool innobase_create_status_file		= FALSE;
my_bool innobase_adaptive_hash_index		= TRUE;

static char *internal_innobase_data_file_path	= NULL;

char*	opt_transition_key			= NULL;
char*	opt_xtra_plugin_dir			= NULL;

my_bool	opt_generate_new_master_key = FALSE;
my_bool	opt_generate_transition_key = FALSE;

bool use_dumped_tablespace_keys = false;

/* The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL	32
ulong	innobase_active_counter	= 0;

ibool srv_compact_backup = FALSE;
ibool srv_rebuild_indexes = FALSE;

static char *xtrabackup_debug_sync = NULL;

my_bool xtrabackup_compact = FALSE;
my_bool xtrabackup_rebuild_indexes = FALSE;

my_bool xtrabackup_incremental_force_scan = FALSE;

/* The flushed lsn which is read from data files */
lsn_t	min_flushed_lsn= 0;
lsn_t	max_flushed_lsn= 0;

/* The size of archived log file */
size_t xtrabackup_arch_file_size = 0ULL;
/* The minimal LSN of found archived log files */
lsn_t xtrabackup_arch_first_file_lsn = 0ULL;
/* The maximum LSN of found archived log files */
lsn_t xtrabackup_arch_last_file_lsn = 0ULL;

ulong xb_open_files_limit= 0;
my_bool xb_close_files= FALSE;

/* Datasinks */
ds_ctxt_t       *ds_data     = NULL;
ds_ctxt_t       *ds_meta     = NULL;
ds_ctxt_t       *ds_redo     = NULL;

static bool	innobackupex_mode = false;

static long	innobase_log_files_in_group_save;
static char	*srv_log_group_home_dir_save;
static longlong	innobase_log_file_size_save;

/* set true if corresponding variable set as option config file or
command argument */
bool innodb_log_checksum_algorithm_specified = false;

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

my_bool opt_galera_info = FALSE;
my_bool opt_slave_info = FALSE;
my_bool opt_no_lock = FALSE;
my_bool opt_safe_slave_backup = FALSE;
my_bool opt_rsync = FALSE;
my_bool opt_force_non_empty_dirs = FALSE;
#ifdef HAVE_VERSION_CHECK
my_bool opt_noversioncheck = FALSE;
#endif
my_bool opt_no_backup_locks = FALSE;
my_bool opt_decompress = FALSE;
my_bool opt_remove_original = FALSE;
my_bool opt_tables_compatibility_check = TRUE;
static my_bool opt_check_privileges = FALSE;

static const char *binlog_info_values[] = {"off", "lockless", "on", "auto",
					   NullS};
static TYPELIB binlog_info_typelib = {array_elements(binlog_info_values)-1, "",
				      binlog_info_values, NULL};
ulong opt_binlog_info;

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

const char *query_type_names[] = { "ALL", "UPDATE", "SELECT", NullS};

TYPELIB query_type_typelib= {array_elements(query_type_names) - 1, "",
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
my_bool opt_dump_innodb_buffer_pool = FALSE;

my_bool opt_lock_ddl = FALSE;
my_bool opt_lock_ddl_per_table = FALSE;
uint opt_lock_ddl_timeout = 0;
uint opt_backup_lock_timeout = 0;
uint opt_backup_lock_retry_count = 0;

const char *opt_history = NULL;
my_bool opt_decrypt = FALSE;
uint opt_read_buffer_size = 0;

const char *ssl_mode_names_lib[] =
  {"DISABLED", "PREFERRED", "REQUIRED", "VERIFY_CA", "VERIFY_IDENTITY",
   NullS };
TYPELIB ssl_mode_typelib = {array_elements(ssl_mode_names_lib) - 1, "",
                            ssl_mode_names_lib, NULL};

#if defined(HAVE_OPENSSL)
uint opt_ssl_mode     = SSL_MODE_PREFERRED;
my_bool opt_use_ssl_arg= TRUE;
my_bool opt_ssl_verify_server_cert_arg = FALSE;
my_bool ssl_mode_set_explicitly= FALSE;
#if !defined(HAVE_YASSL)
char *opt_server_public_key = NULL;
#endif
#endif

static void
check_all_privileges();

/* Whether xtrabackup_binlog_info should be created on recovery */
static bool recover_binlog_info;

/* Redo log format version */
ulint redo_log_version = REDO_LOG_V1;

/* New server-id to encrypt tablespace keys for */
ulint opt_encrypt_server_id = 0;
bool opt_encrypt_for_server_id_specified = false;

#define CLIENT_WARN_DEPRECATED(opt, new_opt) \
  msg("WARNING: " opt \
      " is deprecated and will be removed in a future version. " \
      "Use " new_opt " instead.\n")

/* Simple datasink creation tracking...add datasinks in the reverse order you
want them destroyed. */
#define XTRABACKUP_MAX_DATASINKS	10
static	ds_ctxt_t	*datasinks[XTRABACKUP_MAX_DATASINKS];
static	uint		actual_datasinks = 0;
static inline
void
xtrabackup_add_datasink(ds_ctxt_t *ds)
{
	xb_ad(actual_datasinks < XTRABACKUP_MAX_DATASINKS);
	datasinks[actual_datasinks] = ds; actual_datasinks++;
}

/* ======== Datafiles iterator ======== */
datafiles_iter_t *
datafiles_iter_new(fil_system_t *f_system)
{
	datafiles_iter_t *it;

	it = static_cast<datafiles_iter_t *>
		(ut_malloc_nokey(sizeof(datafiles_iter_t)));
	mutex_create(LATCH_ID_XTRA_DATAFILES_ITER_MUTEX, &it->mutex);

	it->system = f_system;
	it->space = NULL;
	it->node = NULL;
	it->started = FALSE;

	return it;
}

fil_node_t *
datafiles_iter_next(datafiles_iter_t *it)
{
	fil_node_t *new_node;

	mutex_enter(&it->mutex);

	if (it->node == NULL) {
		if (it->started)
			goto end;
		it->started = TRUE;
	} else {
		it->node = UT_LIST_GET_NEXT(chain, it->node);
		if (it->node != NULL)
			goto end;
	}

	it->space = (it->space == NULL) ?
		UT_LIST_GET_FIRST(it->system->space_list) :
		UT_LIST_GET_NEXT(space_list, it->space);

	while (it->space != NULL &&
	       (it->space->purpose != FIL_TYPE_TABLESPACE ||
		UT_LIST_GET_LEN(it->space->chain) == 0))
		it->space = UT_LIST_GET_NEXT(space_list, it->space);
	if (it->space == NULL)
		goto end;

	it->node = UT_LIST_GET_FIRST(it->space->chain);
end:
	new_node = it->node;
	mutex_exit(&it->mutex);

	return new_node;
}

void
datafiles_iter_free(datafiles_iter_t *it)
{
	mutex_free(&it->mutex);
	ut_free(it);
}

/* ======== Date copying thread context ======== */

typedef struct {
	datafiles_iter_t 	*it;
	uint			num;
	uint			*count;
	ib_mutex_t		*count_mutex;
	bool			*error;
	os_thread_id_t		id;
} data_thread_ctxt_t;

/* ======== for option and variables ======== */

enum options_xtrabackup
{
  OPT_XTRA_TARGET_DIR = 1000,     /* make sure it is larger
                                     than OPT_MAX_CLIENT_OPTION */
  OPT_XTRA_BACKUP,
  OPT_XTRA_STATS,
  OPT_XTRA_PREPARE,
  OPT_XTRA_EXPORT,
  OPT_XTRA_APPLY_LOG_ONLY,
  OPT_XTRA_PRINT_PARAM,
  OPT_XTRA_USE_MEMORY,
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
  OPT_XTRA_STREAM,
  OPT_XTRA_COMPRESS,
  OPT_XTRA_COMPRESS_THREADS,
  OPT_XTRA_COMPRESS_CHUNK_SIZE,
  OPT_XTRA_ENCRYPT,
  OPT_XTRA_ENCRYPT_KEY,
  OPT_XTRA_ENCRYPT_KEY_FILE,
  OPT_XTRA_ENCRYPT_THREADS,
  OPT_XTRA_ENCRYPT_CHUNK_SIZE,
  OPT_XTRA_SERVER_ID,
  OPT_XTRA_ENCRYPT_FOR_SERVER_ID,
  OPT_LOG,
  OPT_INNODB,
  OPT_INNODB_CHECKSUMS,
  OPT_INNODB_DATA_FILE_PATH,
  OPT_INNODB_DATA_HOME_DIR,
  OPT_INNODB_ADAPTIVE_HASH_INDEX,
  OPT_INNODB_DOUBLEWRITE,
  OPT_INNODB_FAST_SHUTDOWN,
  OPT_INNODB_FILE_PER_TABLE,
  OPT_INNODB_FLUSH_LOG_AT_TRX_COMMIT,
  OPT_INNODB_FLUSH_METHOD,
  OPT_INNODB_LOCKS_UNSAFE_FOR_BINLOG,
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
  OPT_INNODB_FAST_CHECKSUM,
  OPT_INNODB_EXTRA_UNDOSLOTS,
  OPT_INNODB_DOUBLEWRITE_FILE,
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
  OPT_XTRA_DEBUG_SYNC,
  OPT_XTRA_COMPACT,
  OPT_XTRA_REBUILD_INDEXES,
  OPT_XTRA_REBUILD_THREADS,
  OPT_INNODB_CHECKSUM_ALGORITHM,
  OPT_INNODB_UNDO_DIRECTORY,
  OPT_INNODB_UNDO_TABLESPACES,
  OPT_INNODB_LOG_CHECKSUM_ALGORITHM,
  OPT_XTRA_INCREMENTAL_FORCE_SCAN,
  OPT_DEFAULTS_GROUP,
  OPT_OPEN_FILES_LIMIT,
  OPT_CLOSE_FILES,
  OPT_CORE_FILE,

  OPT_COPY_BACK,
  OPT_MOVE_BACK,
  OPT_GALERA_INFO,
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
  OPT_NO_BACKUP_LOCKS,
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
  OPT_BINLOG_INFO,
  OPT_REDO_LOG_VERSION,
  OPT_XB_SECURE_AUTH,
  OPT_TRANSITION_KEY,
  OPT_GENERATE_TRANSITION_KEY,
  OPT_XTRA_PLUGIN_DIR,
  OPT_GENERATE_NEW_MASTER_KEY,

  OPT_SSL_SSL,
  OPT_SSL_MODE,
  OPT_SSL_VERIFY_SERVER_CERT,
  OPT_SERVER_PUBLIC_KEY,

  OPT_XTRA_TABLES_EXCLUDE,
  OPT_XTRA_DATABASES_EXCLUDE,

  OPT_XTRA_TABLES_COMPATIBILITY_CHECK,
  OPT_XTRA_CHECK_PRIVILEGES,
  OPT_XTRA_READ_BUFFER_SIZE,
};

struct my_option xb_client_options[] =
{
  {"version", 'v', "print xtrabackup version information",
   (G_PTR *) &xtrabackup_version, (G_PTR *) &xtrabackup_version, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"target-dir", OPT_XTRA_TARGET_DIR, "destination directory", (G_PTR*) &xtrabackup_target_dir,
   (G_PTR*) &xtrabackup_target_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"backup", OPT_XTRA_BACKUP, "take backup to target-dir",
   (G_PTR*) &xtrabackup_backup, (G_PTR*) &xtrabackup_backup,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"stats", OPT_XTRA_STATS, "calc statistic of datadir (offline mysqld is recommended)",
   (G_PTR*) &xtrabackup_stats, (G_PTR*) &xtrabackup_stats,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"prepare", OPT_XTRA_PREPARE, "prepare a backup for starting mysql server on the backup.",
   (G_PTR*) &xtrabackup_prepare, (G_PTR*) &xtrabackup_prepare,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"export", OPT_XTRA_EXPORT, "create files to import to another database when prepare.",
   (G_PTR*) &xtrabackup_export, (G_PTR*) &xtrabackup_export,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"apply-log-only", OPT_XTRA_APPLY_LOG_ONLY,
   "stop recovery process not to progress LSN after applying log when prepare.",
   (G_PTR*) &xtrabackup_apply_log_only, (G_PTR*) &xtrabackup_apply_log_only,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"print-param", OPT_XTRA_PRINT_PARAM, "print parameter of mysqld needed for copyback.",
   (G_PTR*) &xtrabackup_print_param, (G_PTR*) &xtrabackup_print_param,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"use-memory", OPT_XTRA_USE_MEMORY, "The value is used instead of buffer_pool_size",
   (G_PTR*) &xtrabackup_use_memory, (G_PTR*) &xtrabackup_use_memory,
   0, GET_LL, REQUIRED_ARG, 100*1024*1024L, 1024*1024L, LLONG_MAX, 0,
   1024*1024L, 0},
  {"throttle", OPT_XTRA_THROTTLE, "limit count of IO operations (pairs of read&write) per second to IOS values (for '--backup')",
   (G_PTR*) &xtrabackup_throttle, (G_PTR*) &xtrabackup_throttle,
   0, GET_LONG, REQUIRED_ARG, 0, 0, LONG_MAX, 0, 1, 0},
  {"log", OPT_LOG, "Ignored option for MySQL option compatibility",
   (G_PTR*) &log_ignored_opt, (G_PTR*) &log_ignored_opt, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-copy-interval", OPT_XTRA_LOG_COPY_INTERVAL, "time interval between checks done by log copying thread in milliseconds (default is 1 second).",
   (G_PTR*) &xtrabackup_log_copy_interval, (G_PTR*) &xtrabackup_log_copy_interval,
   0, GET_LONG, REQUIRED_ARG, 1000, 0, LONG_MAX, 0, 1, 0},
  {"extra-lsndir", OPT_XTRA_EXTRA_LSNDIR, "(for --backup): save an extra copy of the xtrabackup_checkpoints file in this directory.",
   (G_PTR*) &xtrabackup_extra_lsndir, (G_PTR*) &xtrabackup_extra_lsndir,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"incremental-lsn", OPT_XTRA_INCREMENTAL, "(for --backup): copy only .ibd pages newer than specified LSN 'high:low'. ##ATTENTION##: If a wrong LSN value is specified, it is impossible to diagnose this, causing the backup to be unusable. Be careful!",
   (G_PTR*) &xtrabackup_incremental, (G_PTR*) &xtrabackup_incremental,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"incremental-basedir", OPT_XTRA_INCREMENTAL_BASEDIR, "(for --backup): copy only .ibd pages newer than backup at specified directory.",
   (G_PTR*) &xtrabackup_incremental_basedir, (G_PTR*) &xtrabackup_incremental_basedir,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"incremental-dir", OPT_XTRA_INCREMENTAL_DIR, "(for --prepare): apply .delta files and logfile in the specified directory.",
   (G_PTR*) &xtrabackup_incremental_dir, (G_PTR*) &xtrabackup_incremental_dir,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
 {"to-archived-lsn", OPT_XTRA_ARCHIVED_TO_LSN,
   "Don't apply archived logs with bigger log sequence number.",
   (G_PTR*) &xtrabackup_archived_to_lsn, (G_PTR*) &xtrabackup_archived_to_lsn, 0,
   GET_LL, REQUIRED_ARG, 0, 0, LLONG_MAX, 0, 0, 0},
  {"tables", OPT_XTRA_TABLES, "filtering by regexp for table names.",
   (G_PTR*) &xtrabackup_tables, (G_PTR*) &xtrabackup_tables,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tables_file", OPT_XTRA_TABLES_FILE, "filtering by list of the exact database.table name in the file.",
   (G_PTR*) &xtrabackup_tables_file, (G_PTR*) &xtrabackup_tables_file,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"databases", OPT_XTRA_DATABASES, "filtering by list of databases.",
   (G_PTR*) &xtrabackup_databases, (G_PTR*) &xtrabackup_databases,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"databases_file", OPT_XTRA_TABLES_FILE,
   "filtering by list of databases in the file.",
   (G_PTR*) &xtrabackup_databases_file, (G_PTR*) &xtrabackup_databases_file,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tables-exclude", OPT_XTRA_TABLES_EXCLUDE, "filtering by regexp for table names. "
  "Operates the same way as --tables, but matched names are excluded from backup. "
  "Note that this option has a higher priority than --tables.",
    (G_PTR*) &xtrabackup_tables_exclude, (G_PTR*) &xtrabackup_tables_exclude,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"databases-exclude", OPT_XTRA_DATABASES_EXCLUDE, "Excluding databases based on name, "
  "Operates the same way as --databases, but matched names are excluded from backup. "
  "Note that this option has a higher priority than --databases.",
    (G_PTR*) &xtrabackup_databases_exclude, (G_PTR*) &xtrabackup_databases_exclude,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"create-ib-logfile", OPT_XTRA_CREATE_IB_LOGFILE, "** not work for now** creates ib_logfile* also after '--prepare'. ### If you want create ib_logfile*, only re-execute this command in same options. ###",
   (G_PTR*) &xtrabackup_create_ib_logfile, (G_PTR*) &xtrabackup_create_ib_logfile,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"stream", OPT_XTRA_STREAM, "Stream all backup files to the standard output "
   "in the specified format. Currently the only supported format is 'tar'.",
   (G_PTR*) &xtrabackup_stream_str, (G_PTR*) &xtrabackup_stream_str, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"compress", OPT_XTRA_COMPRESS, "Compress individual backup files using the "
   "specified compression algorithm. Currently the only supported algorithm "
   "is 'quicklz'. It is also the default algorithm, i.e. the one used when "
   "--compress is used without an argument.",
   (G_PTR*) &xtrabackup_compress_alg, (G_PTR*) &xtrabackup_compress_alg, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

  {"compress-threads", OPT_XTRA_COMPRESS_THREADS,
   "Number of threads for parallel data compression. The default value is 1.",
   (G_PTR*) &xtrabackup_compress_threads, (G_PTR*) &xtrabackup_compress_threads,
   0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

  {"compress-chunk-size", OPT_XTRA_COMPRESS_CHUNK_SIZE,
   "Size of working buffer(s) for compression threads in bytes. The default value is 64K.",
   (G_PTR*) &xtrabackup_compress_chunk_size, (G_PTR*) &xtrabackup_compress_chunk_size,
   0, GET_ULL, REQUIRED_ARG, (1 << 16), 1024, ULLONG_MAX, 0, 0, 0},

  {"encrypt", OPT_XTRA_ENCRYPT, "Encrypt individual backup files using the "
   "specified encryption algorithm.",
   &xtrabackup_encrypt_algo, &xtrabackup_encrypt_algo,
   &xtrabackup_encrypt_algo_typelib, GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"encrypt-key", OPT_XTRA_ENCRYPT_KEY, "Encryption key to use.",
   (G_PTR*) &xtrabackup_encrypt_key, (G_PTR*) &xtrabackup_encrypt_key, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"encrypt-key-file", OPT_XTRA_ENCRYPT_KEY_FILE, "File which contains encryption key to use.",
   (G_PTR*) &xtrabackup_encrypt_key_file, (G_PTR*) &xtrabackup_encrypt_key_file, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"encrypt-threads", OPT_XTRA_ENCRYPT_THREADS,
   "Number of threads for parallel data encryption. The default value is 1.",
   (G_PTR*) &xtrabackup_encrypt_threads, (G_PTR*) &xtrabackup_encrypt_threads,
   0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

  {"encrypt-chunk-size", OPT_XTRA_ENCRYPT_CHUNK_SIZE,
   "Size of working buffer(S) for encryption threads in bytes. The default value is 64K.",
   (G_PTR*) &xtrabackup_encrypt_chunk_size, (G_PTR*) &xtrabackup_encrypt_chunk_size,
   0, GET_ULL, REQUIRED_ARG, (1 << 16), 1024, ULLONG_MAX, 0, 0, 0},

  {"compact", OPT_XTRA_COMPACT,
   "Create a compact backup by skipping secondary index pages.",
   (G_PTR*) &xtrabackup_compact, (G_PTR*) &xtrabackup_compact,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"rebuild_indexes", OPT_XTRA_REBUILD_INDEXES,
   "Rebuild secondary indexes in InnoDB tables after applying the log. "
   "Only has effect with --prepare.",
   (G_PTR*) &xtrabackup_rebuild_indexes, (G_PTR*) &xtrabackup_rebuild_indexes,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"rebuild_threads", OPT_XTRA_REBUILD_THREADS,
   "Use this number of threads to rebuild indexes in a compact backup. "
   "Only has effect with --prepare and --rebuild-indexes.",
   (G_PTR*) &xtrabackup_rebuild_threads, (G_PTR*) &xtrabackup_rebuild_threads,
   0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

  {"incremental-force-scan", OPT_XTRA_INCREMENTAL_FORCE_SCAN,
   "Perform a full-scan incremental backup even in the presence of changed "
   "page bitmap data",
   (G_PTR*)&xtrabackup_incremental_force_scan,
   (G_PTR*)&xtrabackup_incremental_force_scan, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},


  {"close_files", OPT_CLOSE_FILES, "do not keep files opened. Use at your own "
   "risk.", (G_PTR*) &xb_close_files, (G_PTR*) &xb_close_files, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},

  {"core-file", OPT_CORE_FILE, "Write core on fatal signals", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},


  {"copy-back", OPT_COPY_BACK, "Copy all the files in a previously made "
   "backup from the backup directory to their original locations.",
   (uchar *) &xtrabackup_copy_back, (uchar *) &xtrabackup_copy_back, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"move-back", OPT_MOVE_BACK, "Move all the files in a previously made "
   "backup from the backup directory to the actual datadir location. "
   "Use with caution, as it removes backup files.",
   (uchar *) &xtrabackup_move_back, (uchar *) &xtrabackup_move_back, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"galera-info", OPT_GALERA_INFO, "This options creates the "
   "xtrabackup_galera_info file which contains the local node state at "
   "the time of the backup. Option should be used when performing the "
   "backup of Percona-XtraDB-Cluster. Has no effect when backup locks "
   "are used to create the backup.",
   (uchar *) &opt_galera_info, (uchar *) &opt_galera_info, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"slave-info", OPT_SLAVE_INFO, "This option is useful when backing "
   "up a replication slave server. It prints the binary log position "
   "and name of the master server. It also writes this information to "
   "the \"xtrabackup_slave_info\" file as a \"CHANGE MASTER\" command. "
   "A new slave for this master can be set up by starting a slave server "
   "on this backup and issuing a \"CHANGE MASTER\" command with the "
   "binary log position saved in the \"xtrabackup_slave_info\" file.",
   (uchar *) &opt_slave_info, (uchar *) &opt_slave_info, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"no-lock", OPT_NO_LOCK, "Use this option to disable table lock "
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
   (uchar *) &opt_no_lock, (uchar *) &opt_no_lock, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"lock-ddl", OPT_LOCK_DDL, "Issue LOCK TABLES FOR BACKUP if it is "
   "supported by server at the beginning of the backup to block all DDL "
   "operations.", (uchar*) &opt_lock_ddl, (uchar*) &opt_lock_ddl, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"lock-ddl-timeout", OPT_LOCK_DDL_TIMEOUT,
   "If LOCK TABLES FOR BACKUP does not return within given timeout, abort "
   "the backup.",
   (uchar*) &opt_lock_ddl_timeout,
   (uchar*) &opt_lock_ddl_timeout, 0, GET_UINT,
   REQUIRED_ARG, 31536000, 1, 31536000, 0, 1, 0},

  {"lock-ddl-per-table", OPT_LOCK_DDL_PER_TABLE, "Lock DDL for each table "
   "before xtrabackup starts to copy it and until the backup is completed.",
   (uchar*) &opt_lock_ddl_per_table, (uchar*) &opt_lock_ddl_per_table, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"backup-lock-timeout", OPT_BACKUP_LOCK_TIMEOUT,
   "Timeout in seconds for attempts to acquire metadata locks.",
   (uchar *)&opt_backup_lock_timeout, (uchar *)&opt_backup_lock_timeout, 0,
   GET_UINT, REQUIRED_ARG, 31536000, 1, 31536000, 0, 1, 0},

  {"backup-lock-retry-count", OPT_BACKUP_LOCK_RETRY,
   "Number of attempts to acquire metadata locks.",
   (uchar *)&opt_backup_lock_retry_count,
   (uchar *)&opt_backup_lock_retry_count, 0,
   GET_UINT, REQUIRED_ARG, 0, 0, UINT_MAX, 0, 1, 0},

  {"dump-innodb-buffer-pool", OPT_DUMP_INNODB_BUFFER,
   "Instruct MySQL server to dump innodb buffer pool by issuing a "
   "SET GLOBAL innodb_buffer_pool_dump_now=ON ",
   (uchar*) &opt_dump_innodb_buffer_pool,
   (uchar*) &opt_dump_innodb_buffer_pool, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"dump-innodb-buffer-pool-timeout", OPT_DUMP_INNODB_BUFFER_TIMEOUT,
   "This option specifies the number of seconds xtrabackup waits "
   "for innodb buffer pool dump to complete",
   (uchar*) &opt_dump_innodb_buffer_pool_timeout,
   (uchar*) &opt_dump_innodb_buffer_pool_timeout, 0, GET_UINT,
   REQUIRED_ARG, 10, 0, 0, 0, 0, 0},

  {"dump-innodb-buffer-pool-pct", OPT_DUMP_INNODB_BUFFER_PCT,
   "This option specifies the percentage of buffer pool "
   "to be dumped ",
   (uchar*) &opt_dump_innodb_buffer_pool_pct,
   (uchar*) &opt_dump_innodb_buffer_pool_pct, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 100, 0, 1, 0},

  {"safe-slave-backup", OPT_SAFE_SLAVE_BACKUP, "Stop slave SQL thread "
   "and wait to start backup until Slave_open_temp_tables in "
   "\"SHOW STATUS\" is zero. If there are no open temporary tables, "
   "the backup will take place, otherwise the SQL thread will be "
   "started and stopped until there are no open temporary tables. "
   "The backup will fail if Slave_open_temp_tables does not become "
   "zero after --safe-slave-backup-timeout seconds. The slave SQL "
   "thread will be restarted when the backup finishes.",
   (uchar *) &opt_safe_slave_backup,
   (uchar *) &opt_safe_slave_backup,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"rsync", OPT_RSYNC, "Uses the rsync utility to optimize local file "
   "transfers. When this option is specified, innobackupex uses rsync "
   "to copy all non-InnoDB files instead of spawning a separate cp for "
   "each file, which can be much faster for servers with a large number "
   "of databases or tables.  This option cannot be used together with "
   "--stream.",
   (uchar *) &opt_rsync, (uchar *) &opt_rsync,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"force-non-empty-directories", OPT_FORCE_NON_EMPTY_DIRS, "This "
   "option, when specified, makes --copy-back or --move-back transfer "
   "files to non-empty directories. Note that no existing files will be "
   "overwritten. If --copy-back or --nove-back has to copy a file from "
   "the backup directory which already exists in the destination "
   "directory, it will still fail with an error.",
   (uchar *) &opt_force_non_empty_dirs,
   (uchar *) &opt_force_non_empty_dirs,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

#ifdef HAVE_VERSION_CHECK
  {"no-version-check", OPT_NO_VERSION_CHECK, "This option disables the "
   "version check which is enabled by the --version-check option.",
   (uchar *) &opt_noversioncheck,
   (uchar *) &opt_noversioncheck,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif

  {"tables-compatibility-check", OPT_XTRA_TABLES_COMPATIBILITY_CHECK,
   "This option enables engine compatibility warning.",
   (uchar *) & opt_tables_compatibility_check,
   (uchar *) & opt_tables_compatibility_check,
   0, GET_BOOL, NO_ARG, TRUE, 0, 0, 0, 0, 0},


  {"no-backup-locks", OPT_NO_BACKUP_LOCKS, "This option controls if "
   "backup locks should be used instead of FLUSH TABLES WITH READ LOCK "
   "on the backup stage. The option has no effect when backup locks are "
   "not supported by the server. This option is enabled by default, "
   "disable with --no-backup-locks.",
   (uchar *) &opt_no_backup_locks,
   (uchar *) &opt_no_backup_locks,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"decompress", OPT_DECOMPRESS, "Decompresses all files with the .qp "
   "extension in a backup previously made with the --compress option.",
   (uchar *) &opt_decompress,
   (uchar *) &opt_decompress,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"user", 'u', "This option specifies the MySQL username used "
   "when connecting to the server, if that's not the current user. "
   "The option accepts a string argument. See mysql --help for details.",
   (uchar*) &opt_user, (uchar*) &opt_user, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"host", 'H', "This option specifies the host to use when "
   "connecting to the database server with TCP/IP.  The option accepts "
   "a string argument. See mysql --help for details.",
   (uchar*) &opt_host, (uchar*) &opt_host, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"port", 'P', "This option specifies the port to use when "
   "connecting to the database server with TCP/IP.  The option accepts "
   "a string argument. See mysql --help for details.",
   &opt_port, &opt_port, 0, GET_UINT, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},

  {"password", 'p', "This option specifies the password to use "
   "when connecting to the database. It accepts a string argument.  "
   "See mysql --help for details.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

  {"socket", 'S', "This option specifies the socket to use when "
   "connecting to the local database server with a UNIX domain socket.  "
   "The option accepts a string argument. See mysql --help for details.",
   (uchar*) &opt_socket, (uchar*) &opt_socket, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

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
   (uchar*) &opt_incremental_history_name,
   (uchar*) &opt_incremental_history_name, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"incremental-history-uuid", OPT_INCREMENTAL_HISTORY_UUID,
   "This option specifies the UUID of the specific history record "
   "stored in the PERCONA_SCHEMA.xtrabackup_history to base an "
   "incremental backup on. --incremental-history-name, "
   "--incremental-basedir and --incremental-lsn. If no valid lsn can be "
   "found (no success record with that uuid) xtrabackup will return "
   "with an error. It is used with the --incremental option.",
   (uchar*) &opt_incremental_history_uuid,
   (uchar*) &opt_incremental_history_uuid, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"decrypt", OPT_DECRYPT, "Decrypts all files with the .xbcrypt "
   "extension in a backup previously made with --encrypt option.",
   &opt_decrypt_algo, &opt_decrypt_algo,
   &xtrabackup_encrypt_algo_typelib, GET_ENUM, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},

  {"remove-original", OPT_REMOVE_ORIGINAL, "Remove .qp and .xbcrypt files "
   "after decryption and decompression.",
   (uchar *) &opt_remove_original,
   (uchar *) &opt_remove_original,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"ftwrl-wait-query-type", OPT_LOCK_WAIT_QUERY_TYPE,
   "This option specifies which types of queries are allowed to complete "
   "before innobackupex will issue the global lock. Default is all.",
   (uchar*) &opt_lock_wait_query_type,
   (uchar*) &opt_lock_wait_query_type, &query_type_typelib,
   GET_ENUM, REQUIRED_ARG, QUERY_TYPE_ALL, 0, 0, 0, 0, 0},

  {"kill-long-query-type", OPT_KILL_LONG_QUERY_TYPE,
   "This option specifies which types of queries should be killed to "
   "unblock the global lock. Default is \"all\".",
   (uchar*) &opt_kill_long_query_type,
   (uchar*) &opt_kill_long_query_type, &query_type_typelib,
   GET_ENUM, REQUIRED_ARG, QUERY_TYPE_SELECT, 0, 0, 0, 0, 0},

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
   (uchar*) &opt_kill_long_queries_timeout,
   (uchar*) &opt_kill_long_queries_timeout, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"ftwrl-wait-timeout", OPT_LOCK_WAIT_TIMEOUT,
   "This option specifies time in seconds that innobackupex should wait "
   "for queries that would block FTWRL before running it. If there are "
   "still such queries when the timeout expires, innobackupex terminates "
   "with an error. Default is 0, in which case innobackupex does not "
   "wait for queries to complete and starts FTWRL immediately.",
   (uchar*) &opt_lock_wait_timeout,
   (uchar*) &opt_lock_wait_timeout, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"ftwrl-wait-threshold", OPT_LOCK_WAIT_THRESHOLD,
   "This option specifies the query run time threshold which is used by "
   "innobackupex to detect long-running queries with a non-zero value "
   "of --ftwrl-wait-timeout. FTWRL is not started until such "
   "long-running queries exist. This option has no effect if "
   "--ftwrl-wait-timeout is 0. Default value is 60 seconds.",
   (uchar*) &opt_lock_wait_threshold,
   (uchar*) &opt_lock_wait_threshold, 0, GET_UINT,
   REQUIRED_ARG, 60, 0, 0, 0, 0, 0},

  {"debug-sleep-before-unlock", OPT_DEBUG_SLEEP_BEFORE_UNLOCK,
   "This is a debug-only option used by the XtraBackup test suite.",
   (uchar*) &opt_debug_sleep_before_unlock,
   (uchar*) &opt_debug_sleep_before_unlock, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"safe-slave-backup-timeout", OPT_SAFE_SLAVE_BACKUP_TIMEOUT,
   "How many seconds --safe-slave-backup should wait for "
   "Slave_open_temp_tables to become zero. (default 300)",
   (uchar*) &opt_safe_slave_backup_timeout,
   (uchar*) &opt_safe_slave_backup_timeout, 0, GET_UINT,
   REQUIRED_ARG, 300, 0, 0, 0, 0, 0},

  {"binlog-info", OPT_BINLOG_INFO,
   "This option controls how XtraBackup should retrieve server's binary log "
   "coordinates corresponding to the backup. Possible values are OFF, ON, "
   "LOCKLESS and AUTO. See the XtraBackup manual for more information",
   &opt_binlog_info, &opt_binlog_info,
   &binlog_info_typelib, GET_ENUM, OPT_ARG, BINLOG_INFO_AUTO, 0, 0, 0, 0, 0},

  {"reencrypt-for-server-id", OPT_XTRA_ENCRYPT_FOR_SERVER_ID,
   "Re-encrypt tablespace keys for given server-id.",
   &opt_encrypt_server_id, &opt_encrypt_server_id, 0,
   GET_UINT, REQUIRED_ARG, 0, 0, UINT_MAX32,
   0, 0, 0},

  {"check-privileges", OPT_XTRA_CHECK_PRIVILEGES, "Check database user "
    "privileges before performing any query.", &opt_check_privileges,
   &opt_check_privileges, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"read_buffer_size",
   OPT_XTRA_READ_BUFFER_SIZE,
   "Set datafile read buffer size, given value is scaled up to page size."
   " Default is 10Mb.",
   &opt_read_buffer_size,
   &opt_read_buffer_size,
   0, GET_UINT, OPT_ARG, 10*1024*1024,
   UNIV_PAGE_SIZE_MAX, UINT_MAX, 0, UNIV_PAGE_SIZE_MAX, 0},

#include "sslopt-longopts.h"

#if !defined(HAVE_YASSL)
  {"server-public-key-path", OPT_SERVER_PUBLIC_KEY,
   "File path to the server public RSA key in PEM format.",
   &opt_server_public_key, &opt_server_public_key, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

uint xb_client_options_count = array_elements(xb_client_options);

struct my_option xb_server_options[] =
{
  {"datadir", 'h', "Path to the database root.", (G_PTR*) &mysql_data_home,
   (G_PTR*) &mysql_data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't',
   "Path for temporary files. Several paths may be specified, separated by a "
#if defined(__WIN__) || defined(OS2) || defined(__NETWARE__)
   "semicolon (;)"
#else
   "colon (:)"
#endif
   ", in this case they are used in a round-robin fashion.",
   (G_PTR*) &opt_mysql_tmpdir,
   (G_PTR*) &opt_mysql_tmpdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"parallel", OPT_XTRA_PARALLEL,
   "Number of threads to use for parallel datafiles transfer. "
   "The default value is 1.",
   (G_PTR*) &xtrabackup_parallel, (G_PTR*) &xtrabackup_parallel, 0, GET_INT,
   REQUIRED_ARG, 1, 1, INT_MAX, 0, 0, 0},

   {"log", OPT_LOG, "Ignored option for MySQL option compatibility",
   (G_PTR*) &log_ignored_opt, (G_PTR*) &log_ignored_opt, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

   {"log_bin", OPT_LOG, "Base name for the log sequence",
   &opt_log_bin, &opt_log_bin, 0, GET_STR_ALLOC, OPT_ARG, 0, 0, 0, 0, 0, 0},

   {"innodb", OPT_INNODB, "Ignored option for MySQL option compatibility",
   (G_PTR*) &innobase_ignored_opt, (G_PTR*) &innobase_ignored_opt, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

  {"innodb_adaptive_hash_index", OPT_INNODB_ADAPTIVE_HASH_INDEX,
   "Enable InnoDB adaptive hash index (enabled by default).  "
   "Disable with --skip-innodb-adaptive-hash-index.",
   (G_PTR*) &innobase_adaptive_hash_index,
   (G_PTR*) &innobase_adaptive_hash_index,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"innodb_autoextend_increment", OPT_INNODB_AUTOEXTEND_INCREMENT,
   "Data file autoextend increment in megabytes",
   (G_PTR*) &sys_tablespace_auto_extend_increment,
   (G_PTR*) &sys_tablespace_auto_extend_increment,
   0, GET_ULONG, REQUIRED_ARG, 8L, 1L, 1000L, 0, 1L, 0},
  {"innodb_buffer_pool_size", OPT_INNODB_BUFFER_POOL_SIZE,
   "The size of the memory buffer InnoDB uses to cache data and indexes of its tables.",
   (G_PTR*) &innobase_buffer_pool_size, (G_PTR*) &innobase_buffer_pool_size, 0,
   GET_LL, REQUIRED_ARG, 8*1024*1024L, 1024*1024L, LLONG_MAX, 0,
   1024*1024L, 0},
  {"innodb_checksums", OPT_INNODB_CHECKSUMS, "Enable InnoDB checksums validation (enabled by default). \
Disable with --skip-innodb-checksums.", (G_PTR*) &innobase_use_checksums,
   (G_PTR*) &innobase_use_checksums, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"innodb_data_file_path", OPT_INNODB_DATA_FILE_PATH,
   "Path to individual files and their sizes.", &innobase_data_file_path,
   &innobase_data_file_path, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_data_home_dir", OPT_INNODB_DATA_HOME_DIR,
   "The common part for InnoDB table spaces.", &innobase_data_home_dir,
   &innobase_data_home_dir, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_doublewrite", OPT_INNODB_DOUBLEWRITE, "Enable InnoDB doublewrite buffer (enabled by default). \
Disable with --skip-innodb-doublewrite.", (G_PTR*) &innobase_use_doublewrite,
   (G_PTR*) &innobase_use_doublewrite, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"innodb_io_capacity", OPT_INNODB_IO_CAPACITY,
   "Number of IOPs the server can do. Tunes the background IO rate",
   (G_PTR*) &srv_io_capacity, (G_PTR*) &srv_io_capacity,
   0, GET_ULONG, OPT_ARG, 200, 100, ~0UL, 0, 0, 0},
  {"innodb_file_io_threads", OPT_INNODB_FILE_IO_THREADS,
   "Number of file I/O threads in InnoDB.", (G_PTR*) &innobase_file_io_threads,
   (G_PTR*) &innobase_file_io_threads, 0, GET_LONG, REQUIRED_ARG, 4, 4, 64, 0,
   1, 0},
  {"innodb_read_io_threads", OPT_INNODB_READ_IO_THREADS,
   "Number of background read I/O threads in InnoDB.", (G_PTR*) &innobase_read_io_threads,
   (G_PTR*) &innobase_read_io_threads, 0, GET_LONG, REQUIRED_ARG, 4, 1, 64, 0,
   1, 0},
  {"innodb_write_io_threads", OPT_INNODB_WRITE_IO_THREADS,
   "Number of background write I/O threads in InnoDB.", (G_PTR*) &innobase_write_io_threads,
   (G_PTR*) &innobase_write_io_threads, 0, GET_LONG, REQUIRED_ARG, 4, 1, 64, 0,
   1, 0},
  {"innodb_file_per_table", OPT_INNODB_FILE_PER_TABLE,
   "Stores each InnoDB table to an .ibd file in the database dir.",
   (G_PTR*) &innobase_file_per_table,
   (G_PTR*) &innobase_file_per_table, 0, GET_BOOL, NO_ARG,
   FALSE, 0, 0, 0, 0, 0},
  {"innodb_flush_log_at_trx_commit", OPT_INNODB_FLUSH_LOG_AT_TRX_COMMIT,
   "Set to 0 (write and flush once per second), 1 (write and flush at each commit) or 2 (write at commit, flush once per second).",
   (G_PTR*) &srv_flush_log_at_trx_commit,
   (G_PTR*) &srv_flush_log_at_trx_commit,
   0, GET_ULONG, OPT_ARG,  1, 0, 2, 0, 0, 0},
  {"innodb_flush_method", OPT_INNODB_FLUSH_METHOD,
   "With which method to flush data.", (G_PTR*) &innobase_unix_file_flush_method,
   (G_PTR*) &innobase_unix_file_flush_method, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},

/* ####### Should we use this option? ####### */
  {"innodb_force_recovery", OPT_INNODB_FORCE_RECOVERY,
   "Helps to save your data in case the disk image of the database becomes corrupt.",
   (G_PTR*) &innobase_force_recovery, (G_PTR*) &innobase_force_recovery, 0,
   GET_LONG, REQUIRED_ARG, 0, 0, 6, 0, 1, 0},

  {"innodb_log_arch_dir", OPT_INNODB_LOG_ARCH_DIR,
   "Where full logs should be archived.", (G_PTR*) &innobase_log_arch_dir,
   (G_PTR*) &innobase_log_arch_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_log_buffer_size", OPT_INNODB_LOG_BUFFER_SIZE,
   "The size of the buffer which InnoDB uses to write log to the log files on disk.",
   (G_PTR*) &innobase_log_buffer_size, (G_PTR*) &innobase_log_buffer_size, 0,
   GET_LONG, REQUIRED_ARG, 16*1024*1024L, 256*1024L, LONG_MAX, 0, 1024, 0},
  {"innodb_log_file_size", OPT_INNODB_LOG_FILE_SIZE,
   "Size of each log file in a log group.",
   (G_PTR*) &innobase_log_file_size, (G_PTR*) &innobase_log_file_size, 0,
   GET_LL, REQUIRED_ARG, 48*1024*1024L, 1*1024*1024L, LLONG_MAX, 0,
   1024*1024L, 0},
  {"innodb_log_files_in_group", OPT_INNODB_LOG_FILES_IN_GROUP,
   "Number of log files in the log group. InnoDB writes to the files in a "
   "circular fashion. Value 3 is recommended here.",
   &innobase_log_files_in_group, &innobase_log_files_in_group,
   0, GET_LONG, REQUIRED_ARG, 2, 2, 100, 0, 1, 0},
  {"innodb_log_group_home_dir", OPT_INNODB_LOG_GROUP_HOME_DIR,
   "Path to InnoDB log files.", &srv_log_group_home_dir,
   &srv_log_group_home_dir, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_max_dirty_pages_pct", OPT_INNODB_MAX_DIRTY_PAGES_PCT,
   "Percentage of dirty pages allowed in bufferpool.",
   (G_PTR*) &srv_max_buf_pool_modified_pct,
   (G_PTR*) &srv_max_buf_pool_modified_pct, 0, GET_DOUBLE, REQUIRED_ARG,
   (longlong)getopt_double2ulonglong(75), (longlong)getopt_double2ulonglong(0),
   getopt_double2ulonglong(100), 0, 0, 0},
  {"innodb_open_files", OPT_INNODB_OPEN_FILES,
   "How many files at the maximum InnoDB keeps open at the same time.",
   (G_PTR*) &innobase_open_files, (G_PTR*) &innobase_open_files, 0,
   GET_LONG, REQUIRED_ARG, 300L, 10L, LONG_MAX, 0, 1L, 0},
  {"innodb_use_native_aio", OPT_INNODB_USE_NATIVE_AIO,
   "Use native AIO if supported on this platform.",
   (G_PTR*) &srv_use_native_aio,
   (G_PTR*) &srv_use_native_aio, 0, GET_BOOL, NO_ARG,
   FALSE, 0, 0, 0, 0, 0},
  {"innodb_page_size", OPT_INNODB_PAGE_SIZE,
   "The universal page size of the database.",
   (G_PTR*) &innobase_page_size, (G_PTR*) &innobase_page_size, 0,
   /* Use GET_LL to support numeric suffixes in 5.6 */
   GET_LL, REQUIRED_ARG,
   (1LL << 14), (1LL << 12), (1LL << UNIV_PAGE_SIZE_SHIFT_MAX), 0, 1L, 0},
  {"innodb_log_block_size", OPT_INNODB_LOG_BLOCK_SIZE,
  "The log block size of the transaction log file. "
   "Changing for created log file is not supported. Use on your own risk!",
   (G_PTR*) &innobase_log_block_size, (G_PTR*) &innobase_log_block_size, 0,
   GET_ULONG, REQUIRED_ARG, 512, 512, 1 << UNIV_PAGE_SIZE_SHIFT_MAX, 0, 1L, 0},
  {"innodb_fast_checksum", OPT_INNODB_FAST_CHECKSUM,
   "Change the algorithm of checksum for the whole of datapage to 4-bytes word based.",
   (G_PTR*) &innobase_fast_checksum,
   (G_PTR*) &innobase_fast_checksum, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_doublewrite_file", OPT_INNODB_DOUBLEWRITE_FILE,
   "Path to special datafile for doublewrite buffer. (default is "": not used)",
   (G_PTR*) &innobase_doublewrite_file, (G_PTR*) &innobase_doublewrite_file,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_buffer_pool_filename", OPT_INNODB_BUFFER_POOL_FILENAME,
   "Filename to/from which to dump/load the InnoDB buffer pool",
   (G_PTR*) &innobase_buffer_pool_filename,
   (G_PTR*) &innobase_buffer_pool_filename,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

#ifndef __WIN__
  {"debug-sync", OPT_XTRA_DEBUG_SYNC,
   "Debug sync point. This is only used by the xtrabackup test suite",
   (G_PTR*) &xtrabackup_debug_sync,
   (G_PTR*) &xtrabackup_debug_sync,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif

  {"innodb_checksum_algorithm", OPT_INNODB_CHECKSUM_ALGORITHM,
  "The algorithm InnoDB uses for page checksumming. [CRC32, STRICT_CRC32, "
   "INNODB, STRICT_INNODB, NONE, STRICT_NONE]", &srv_checksum_algorithm,
   &srv_checksum_algorithm, &innodb_checksum_algorithm_typelib, GET_ENUM,
   REQUIRED_ARG, SRV_CHECKSUM_ALGORITHM_INNODB, 0, 0, 0, 0, 0},
  {"innodb_log_checksum_algorithm", OPT_INNODB_LOG_CHECKSUM_ALGORITHM,
  "The algorithm InnoDB uses for log checksumming. [CRC32, STRICT_CRC32, "
   "INNODB, STRICT_INNODB, NONE, STRICT_NONE]", &srv_log_checksum_algorithm,
   &srv_log_checksum_algorithm, &innodb_checksum_algorithm_typelib, GET_ENUM,
   REQUIRED_ARG, SRV_CHECKSUM_ALGORITHM_INNODB, 0, 0, 0, 0, 0},
  {"innodb_undo_directory", OPT_INNODB_UNDO_DIRECTORY,
   "Directory where undo tablespace files live, this path can be absolute.",
   &srv_undo_dir, &srv_undo_dir, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},

  {"innodb_undo_tablespaces", OPT_INNODB_UNDO_TABLESPACES,
   "Number of undo tablespaces to use.",
   (G_PTR*)&srv_undo_tablespaces, (G_PTR*)&srv_undo_tablespaces,
   0, GET_ULONG, REQUIRED_ARG, 0, 0, 126, 0, 1, 0},

  {"defaults_group", OPT_DEFAULTS_GROUP, "defaults group in config file (default \"mysqld\").",
   (G_PTR*) &defaults_group, (G_PTR*) &defaults_group,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"open_files_limit", OPT_OPEN_FILES_LIMIT, "the maximum number of file "
   "descriptors to reserve with setrlimit().",
   (G_PTR*) &xb_open_files_limit, (G_PTR*) &xb_open_files_limit, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, UINT_MAX, 0, 1, 0},

  {"redo-log-version", OPT_REDO_LOG_VERSION,
   "Redo log version of the backup. For --prepare only.",
   &redo_log_version, &redo_log_version, 0, GET_UINT,
   REQUIRED_ARG, 1, 0, 0, 0, 0, 0},

  {"server-id", OPT_XTRA_SERVER_ID, "The server instance being backed up",
   &server_id, &server_id, 0, GET_UINT, REQUIRED_ARG, 0, 0, UINT_MAX32,
   0, 0, 0},

  {"transition-key", OPT_TRANSITION_KEY, "Transition key to encrypt "
   "tablespace keys with.", 0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

  {"xtrabackup-plugin-dir", OPT_XTRA_PLUGIN_DIR,
   "Directory for xtrabackup plugins.",
   &opt_xtra_plugin_dir, &opt_xtra_plugin_dir,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"generate-new-master-key", OPT_GENERATE_NEW_MASTER_KEY,
   "Generate new master key when doing copy-back.",
   &opt_generate_new_master_key, &opt_generate_new_master_key, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},

  {"generate-transition-key", OPT_GENERATE_TRANSITION_KEY,
   "Generate transition key and store it into keyring.",
   &opt_generate_transition_key, &opt_generate_transition_key, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

uint xb_server_options_count = array_elements(xb_server_options);

#ifndef __WIN__
static int debug_sync_resumed;

static void sigcont_handler(int sig);

static void sigcont_handler(int sig __attribute__((unused)))
{
	debug_sync_resumed= 1;
}
#endif

static inline
void
debug_sync_point(const char *name)
{
#ifndef __WIN__
	FILE	*fp;
	pid_t	pid;
	char	pid_path[FN_REFLEN];

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
		msg("xtrabackup: Error: cannot open %s\n", pid_path);
		exit(EXIT_FAILURE);
	}
	fprintf(fp, "%u\n", (uint) pid);
	fclose(fp);

	msg("xtrabackup: DEBUG: Suspending at debug sync point '%s'. "
	    "Resume with 'kill -SIGCONT %u'.\n", name, (uint) pid);

	debug_sync_resumed= 0;
	kill(pid, SIGSTOP);
	while (!debug_sync_resumed) {
		sleep(1);
	}

	/* On resume */
	msg("xtrabackup: DEBUG: removing the pid file.\n");
	my_delete(pid_path, MYF(MY_WME));
#endif
}

static const char *xb_client_default_groups[]=
	{ "xtrabackup", "client", 0, 0, 0 };

static const char *xb_server_default_groups[]=
	{ "xtrabackup", "mysqld", 0, 0, 0 };

static void print_version(void)
{
  msg("%s version %s based on MySQL server %s %s (%s) (revision id: %s)\n",
      my_progname, XTRABACKUP_VERSION, MYSQL_SERVER_VERSION, SYSTEM_TYPE,
      MACHINE_TYPE, XTRABACKUP_REVISION);
}

static void usage(void)
{
  puts("Open source backup tool for InnoDB and XtraDB\n\
\n\
Copyright (C) 2009-2017 Percona LLC and/or its affiliates.\n\
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

  printf("Usage: [%s [--defaults-file=#] --backup | %s [--defaults-file=#] --prepare] [OPTIONS]\n",my_progname,my_progname);
  print_defaults("my", xb_server_default_groups);
  my_print_help(xb_client_options);
  my_print_help(xb_server_options);
  my_print_variables(xb_server_options);
  my_print_variables(xb_client_options);
}

#define ADD_PRINT_PARAM_OPT(value)              \
  { \
    print_param_str << opt->name << "=" << value << "\n"; \
    param_set.insert(opt->name); \
  }

/************************************************************************
Check if parameter is set in defaults file or via command line argument
@return true if parameter is set. */
bool
check_if_param_set(const char *param)
{
	return param_set.find(param) != param_set.end();
}

my_bool
xb_get_one_option(int optid,
		  const struct my_option *opt,
		  char *argument)
{
  static const char* hide_value[]=
    { "password", "encrypt-key", "transition-key" };

  param_str << "--" << opt->name;
  if (argument) {
    bool param_handled = false;
    for (unsigned i=0; i<sizeof(hide_value)/sizeof(char*); ++i) {
      if (strcmp(opt->name, hide_value[i]) == 0) {
        param_handled = true;
        param_str << "=*";
        break;
      }
    }
    if(!param_handled) {
      param_str << "=" << argument;
    }
  }
  param_str << " ";
  switch(optid) {
  case 'h':
    strmake(mysql_real_data_home,argument, FN_REFLEN - 1);
    mysql_data_home= mysql_real_data_home;

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

    ADD_PRINT_PARAM_OPT(innobase_unix_file_flush_method);
    break;

  case OPT_INNODB_PAGE_SIZE:

    ADD_PRINT_PARAM_OPT(innobase_page_size);
    break;

  case OPT_INNODB_FAST_CHECKSUM:

    ADD_PRINT_PARAM_OPT(!!innobase_fast_checksum);
    break;

  case OPT_INNODB_LOG_BLOCK_SIZE:

    ADD_PRINT_PARAM_OPT(innobase_log_block_size);
    break;

  case OPT_INNODB_DOUBLEWRITE_FILE:

    ADD_PRINT_PARAM_OPT(innobase_doublewrite_file);
    break;

  case OPT_INNODB_UNDO_DIRECTORY:

    ADD_PRINT_PARAM_OPT(srv_undo_dir);
    break;

  case OPT_INNODB_UNDO_TABLESPACES:

    ADD_PRINT_PARAM_OPT(srv_undo_tablespaces);
    break;

  case OPT_INNODB_CHECKSUM_ALGORITHM:

    ut_a(srv_checksum_algorithm <= SRV_CHECKSUM_ALGORITHM_STRICT_NONE);

    ADD_PRINT_PARAM_OPT(innodb_checksum_algorithm_names[srv_checksum_algorithm]);
    innodb_checksum_algorithm_specified = true;
    break;

  case OPT_INNODB_LOG_CHECKSUM_ALGORITHM:

    ut_a(srv_log_checksum_algorithm <= SRV_CHECKSUM_ALGORITHM_STRICT_NONE);

    ADD_PRINT_PARAM_OPT(innodb_checksum_algorithm_names[srv_log_checksum_algorithm]);
    innodb_log_checksum_algorithm_specified = true;
    break;

  case OPT_INNODB_BUFFER_POOL_FILENAME:

    ADD_PRINT_PARAM_OPT(innobase_buffer_pool_filename);
    break;

  case OPT_XTRA_TARGET_DIR:
    strmake(xtrabackup_real_target_dir,argument, sizeof(xtrabackup_real_target_dir)-1);
    xtrabackup_target_dir= xtrabackup_real_target_dir;
    break;
  case OPT_XTRA_STREAM:
    if (!strcasecmp(argument, "tar"))
      xtrabackup_stream_fmt = XB_STREAM_FMT_TAR;
    else if (!strcasecmp(argument, "xbstream"))
      xtrabackup_stream_fmt = XB_STREAM_FMT_XBSTREAM;
    else
    {
      msg("Invalid --stream argument: %s\n", argument);
      return 1;
    }
    xtrabackup_stream = TRUE;
    break;
  case OPT_XTRA_COMPRESS:
    if (argument == NULL)
      xtrabackup_compress_alg = "quicklz";
    else if (strcasecmp(argument, "quicklz"))
    {
      msg("Invalid --compress argument: %s\n", argument);
      return 1;
    }
    xtrabackup_compress = TRUE;
    break;
  case OPT_XTRA_ENCRYPT:
    if (argument == NULL)
    {
      msg("Missing --encrypt argument, must specify a valid encryption "
          " algorithm.\n");
      return 1;
    }
    xtrabackup_encrypt = TRUE;
    break;
  case OPT_DECRYPT:
    if (argument == NULL) {
      msg("Missing --decrypt argument, must specify a "
          "valid encryption  algorithm.\n");
      return(1);
    }
    opt_decrypt = TRUE;
    xtrabackup_decrypt_decompress = true;
    break;
  case OPT_DECOMPRESS:
    opt_decompress = TRUE;
    xtrabackup_decrypt_decompress = true;
    break;
  case (int) OPT_CORE_FILE:
    test_flags |= TEST_CORE_ON_SIGNAL;
    break;
  case OPT_HISTORY:
    if (argument) {
      opt_history = argument;
    } else {
      opt_history = "";
    }
    break;
  case OPT_XTRA_ENCRYPT_FOR_SERVER_ID:
    opt_encrypt_for_server_id_specified = true;
    break;
  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";                       /* Don't require password */
    if (argument)
    {
      char *start = argument;
      my_free(opt_password);
      opt_password = my_strdup(PSI_NOT_INSTRUMENTED, argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';         /* Destroy argument */
      if (*start)
        start[1] = 0;                             /* Cut length of argument */
      tty_password = false;
    }
    else
      tty_password = true;
    break;
  case OPT_TRANSITION_KEY:
    if (argument == disabled_my_option)
      argument = (char*) "";                      /* Don't require password */
    if (argument)
    {
      char *start = argument;
      my_free(opt_transition_key);
      opt_transition_key = my_strdup(PSI_NOT_INSTRUMENTED,
                                     argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';         /* Destroy argument */
      if (*start)
        start[1] = 0;                             /* Cut length of argument */
      tty_transition_key = false;
    }
    else
      tty_transition_key = true;
    use_dumped_tablespace_keys = true;
    break;
  case OPT_GENERATE_TRANSITION_KEY:
    use_dumped_tablespace_keys = true;
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

/***********************************************************************
Initializes log_block_size */
static
ibool
xb_init_log_block_size(void)
{
	srv_log_block_size = 0;
	if (innobase_log_block_size != 512) {
		uint	n_shift = get_bit_shift(innobase_log_block_size);

		if (n_shift > 0) {
			srv_log_block_size = (1 << n_shift);
			msg("InnoDB: The log block size is set to %lu.\n",
			    srv_log_block_size);
		}
	} else {
		srv_log_block_size = 512;
	}
	if (!srv_log_block_size) {
		msg("InnoDB: Error: %lu is not valid value for "
		    "innodb_log_block_size.\n", innobase_log_block_size);
		return FALSE;
	}

	return TRUE;
}

/** Check that a page_size is correct for InnoDB.
If correct, set the associated page_size_shift which is the power of 2
for this page size.
@param[in]      page_size       Page Size to evaluate
@return an associated page_size_shift if valid, 0 if invalid. */
inline
ulong
innodb_page_size_validate(
        ulong   page_size)
{
        ulong           n;

        for (n = UNIV_PAGE_SIZE_SHIFT_MIN;
             n <= UNIV_PAGE_SIZE_SHIFT_MAX;
             n++) {
                if (page_size == static_cast<ulong>(1 << n)) {
                        return(n);
                }
        }

        return(0);
}

static my_bool
innodb_init_param(void)
{
	/* innobase_init */
	static char	current_dir[3];		/* Set if using current lib */
	char		*default_path;
        ulint		fsp_flags;

	/* === some variables from mysqld === */
	memset((G_PTR) &mysql_tmpdir_list, 0, sizeof(mysql_tmpdir_list));

	if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
		exit(EXIT_FAILURE);

	/* dummy for initialize all_charsets[] */
	get_charset_name(0);

	/* Check that the value of system variable innodb_page_size was
	set correctly.  Its value was put into srv_page_size. If valid,
	return the associated srv_page_size_shift. */
	srv_page_size_shift = innodb_page_size_validate(innobase_page_size);
	if (!srv_page_size_shift) {
		msg("xtrabackup: Invalid page size=%llu.\n", innobase_page_size);
		goto error;
	}
	srv_page_size = innobase_page_size;

	if (!xb_init_log_block_size()) {
		goto error;
	}

	srv_fast_checksum = (ibool) innobase_fast_checksum;

	/* Check that values don't overflow on 32-bit systems. */
	if (sizeof(ulint) == 4) {
		if (xtrabackup_use_memory > UINT_MAX32) {
			msg("xtrabackup: use-memory can't be over 4GB"
			    " on 32-bit systems\n");
		}

		if (innobase_buffer_pool_size > UINT_MAX32) {
			msg("xtrabackup: innobase_buffer_pool_size can't be "
			    "over 4GB on 32-bit systems\n");

			goto error;
		}

		if (innobase_log_file_size > UINT_MAX32) {
			msg("xtrabackup: innobase_log_file_size can't be "
			    "over 4GB on 32-bit systemsi\n");

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

	fil_path_to_mysql_datadir = default_path;
	folder_mysql_datadir = fil_path_to_mysql_datadir;

	/* Set InnoDB initialization parameters according to the values
	read from MySQL .cnf file */

	if (xtrabackup_backup || xtrabackup_stats) {
		msg("xtrabackup: using the following InnoDB configuration:\n");
	} else {
		msg("xtrabackup: using the following InnoDB configuration "
		    "for recovery:\n");
	}

	/*--------------- Data files -------------------------*/

	/* The default dir for data files is the datadir of MySQL */

	srv_data_home = ((xtrabackup_backup || xtrabackup_stats) && innobase_data_home_dir
			 ? innobase_data_home_dir : default_path);
	msg("xtrabackup:   innodb_data_home_dir = %s\n", srv_data_home);

	/*--------------- Shared tablespaces -------------------------*/

	/* Set default InnoDB data file size to 10 MB and let it be
  	auto-extending. Thus users can use InnoDB in >= 4.0 without having
	to specify any startup options. */

	if (!innobase_data_file_path) {
  		innobase_data_file_path = (char*) "ibdata1:10M:autoextend";
	}
	msg("xtrabackup:   innodb_data_file_path = %s\n",
	    innobase_data_file_path);

	/* This is the first time univ_page_size is used.
	It was initialized to 16k pages before srv_page_size was set */
	univ_page_size.copy_from(
		page_size_t(srv_page_size, srv_page_size, false));

	srv_sys_space.set_space_id(TRX_SYS_SPACE);

	/* Create the filespace flags. */
	fsp_flags = fsp_flags_init(
		univ_page_size, false, false, false, false);
	srv_sys_space.set_flags(fsp_flags);

	srv_sys_space.set_name(reserved_system_space_name);
	srv_sys_space.set_path(srv_data_home);

	/* Supports raw devices */
	if (!srv_sys_space.parse_params(innobase_data_file_path,
					true, xtrabackup_prepare)) {
		goto error;
	}

	/* Set default InnoDB temp data file size to 12 MB and let it be
	auto-extending. */

	if (!innobase_temp_data_file_path) {
		innobase_temp_data_file_path = (char*) "ibtmp1:12M:autoextend";
	}

	/* We set the temporary tablspace id later, after recovery.
	The temp tablespace doesn't support raw devices.
	Set the name and path. */
	srv_tmp_space.set_name(reserved_temporary_space_name);
	srv_tmp_space.set_path(srv_data_home);

	/* Create the filespace flags with the temp flag set. */
	fsp_flags = fsp_flags_init(
		univ_page_size, false, false, false, true);
	srv_tmp_space.set_flags(fsp_flags);

	if (!srv_tmp_space.parse_params(innobase_temp_data_file_path, false,
					xtrabackup_prepare)) {
		goto error;
	}

	/* Perform all sanity check before we take action of deleting files*/
	if (srv_sys_space.intersection(&srv_tmp_space)) {
		msg("%s and %s file names seem to be the same.",
			srv_tmp_space.name(), srv_sys_space.name());
		goto error;
	}

	/* -------------- Log files ---------------------------*/

	/* The default dir for log files is the datadir of MySQL */

	if (!((xtrabackup_backup || xtrabackup_stats) &&
	      srv_log_group_home_dir)) {
		srv_log_group_home_dir = default_path;
	}
	if (xtrabackup_prepare && xtrabackup_incremental_dir) {
		srv_log_group_home_dir = xtrabackup_incremental_dir;
	}
	msg("xtrabackup:   innodb_log_group_home_dir = %s\n",
	    srv_log_group_home_dir);

	os_normalize_path(srv_log_group_home_dir);

	if (strchr(srv_log_group_home_dir, ';')) {

		msg("syntax error in innodb_log_group_home_dir, ");

		goto error;
	}

	srv_adaptive_flushing = FALSE;
	srv_file_format = 1; /* Barracuda */
	srv_max_file_format_at_startup = UNIV_FORMAT_MIN; /* on */
	/* --------------------------------------------------*/

	srv_file_flush_method_str = innobase_unix_file_flush_method;

	srv_n_log_files = (ulint) innobase_log_files_in_group;
	srv_log_file_size = (ulint) innobase_log_file_size;
	msg("xtrabackup:   innodb_log_files_in_group = %ld\n",
	    srv_n_log_files);
	msg("xtrabackup:   innodb_log_file_size = %lld\n",
	    (long long int) srv_log_file_size);

	srv_log_buffer_size = (ulint) innobase_log_buffer_size;

        /* We set srv_pool_size here in units of 1 kB. InnoDB internally
        changes the value so that it becomes the number of database pages. */

	/* TDOD: add option */
	srv_buf_pool_chunk_unit = 134217728;
	srv_buf_pool_size = (ulint) xtrabackup_use_memory;
	srv_buf_pool_instances = 1;

	srv_n_file_io_threads = (ulint) innobase_file_io_threads;
	srv_n_read_io_threads = (ulint) innobase_read_io_threads;
	srv_n_write_io_threads = (ulint) innobase_write_io_threads;

	srv_force_recovery = (ulint) innobase_force_recovery;

	srv_use_doublewrite_buf = (ibool) innobase_use_doublewrite;

	if (!innobase_use_checksums) {

		srv_checksum_algorithm = SRV_CHECKSUM_ALGORITHM_NONE;
	}

	btr_search_enabled = (char) innobase_adaptive_hash_index;

	os_use_large_pages = (ibool) innobase_use_large_pages;
	os_large_page_size = (ulint) innobase_large_page_size;

	row_rollback_on_timeout = (ibool) innobase_rollback_on_timeout;

	srv_file_per_table = (my_bool) innobase_file_per_table;

        srv_locks_unsafe_for_binlog = (ibool) innobase_locks_unsafe_for_binlog;

	srv_max_n_open_files = (ulint) innobase_open_files;
	srv_innodb_status = (ibool) innobase_create_status_file;

	srv_print_verbose_log = 1;

	/* Store the default charset-collation number of this MySQL
	installation */

	/* We cannot treat characterset here for now!! */
	data_mysql_default_charset_coll = (ulint)default_charset_info->number;

	//innobase_commit_concurrency_init_default();

	/* Since we in this module access directly the fields of a trx
        struct, and due to different headers and flags it might happen that
	mutex_t has a different size in this module and in InnoDB
	modules, we check at run time that the size is the same in
	these compilation modules. */

	/* On 5.5+ srv_use_native_aio is TRUE by default. It is later reset
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

		srv_use_native_aio = FALSE;
		break;

	case OS_WIN2000:
	case OS_WINXP:
		/* On 2000 and XP, async IO is available. */
		srv_use_native_aio = TRUE;
		break;

	default:
		/* Vista and later have both async IO and condition variables */
		srv_use_native_aio = TRUE;
		srv_use_native_conditions = TRUE;
		break;
	}

#elif defined(LINUX_NATIVE_AIO)

	if (srv_use_native_aio) {
		ut_print_timestamp(stderr);
		msg(" InnoDB: Using Linux native AIO\n");
	}
#else
	/* Currently native AIO is supported only on windows and linux
	and that also when the support is compiled in. In all other
	cases, we ignore the setting of innodb_use_native_aio. */
	srv_use_native_aio = FALSE;

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

	innodb_log_checksum_func_update(srv_log_checksum_algorithm);

	return(FALSE);

error:
	msg("xtrabackup: innodb_init_param(): Error occured.\n");
	return(TRUE);
}

static my_bool
innodb_init(void)
{
	int	err;

	err = innobase_start_or_create_for_mysql();

	if (err != DB_SUCCESS) {
		free(internal_innobase_data_file_path);
		internal_innobase_data_file_path = NULL;
		goto error;
	}

	/* They may not be needed for now */
//	(void) hash_init(&innobase_open_tables,system_charset_info, 32, 0, 0,
//			 		(hash_get_key) innobase_get_key, 0, 0);
//        pthread_mutex_init(&innobase_share_mutex, MY_MUTEX_INIT_FAST);
//        pthread_mutex_init(&prepare_commit_mutex, MY_MUTEX_INIT_FAST);
//        pthread_mutex_init(&commit_threads_m, MY_MUTEX_INIT_FAST);
//        pthread_mutex_init(&commit_cond_m, MY_MUTEX_INIT_FAST);
//        pthread_cond_init(&commit_cond, NULL);

	innodb_inited= 1;

	return(FALSE);

error:
	msg("xtrabackup: innodb_init(): Error occured.\n");
	return(TRUE);
}

static my_bool
innodb_end(void)
{
	srv_fast_shutdown = (ulint) innobase_fast_shutdown;
	innodb_inited = 0;

	msg("xtrabackup: starting shutdown with innodb_fast_shutdown = %lu\n",
	    srv_fast_shutdown);

	if (innobase_shutdown_for_mysql() != DB_SUCCESS) {
		goto error;
	}
	free(internal_innobase_data_file_path);
	internal_innobase_data_file_path = NULL;

	/* They may not be needed for now */
//	hash_free(&innobase_open_tables);
//	pthread_mutex_destroy(&innobase_share_mutex);
//	pthread_mutex_destroy(&prepare_commit_mutex);
//	pthread_mutex_destroy(&commit_threads_m);
//	pthread_mutex_destroy(&commit_cond_m);
//	pthread_cond_destroy(&commit_cond);

	return(FALSE);

error:
	msg("xtrabackup: innodb_end(): Error occured.\n");
	return(TRUE);
}

/* ================= common ================= */

/***********************************************************************
Read backup meta info.
@return TRUE on success, FALSE on failure. */
static
my_bool
xtrabackup_read_metadata(char *filename)
{
	FILE	*fp;
	my_bool	 r = TRUE;
	int	 t;

	fp = fopen(filename,"r");
	if(!fp) {
		msg("xtrabackup: Error: cannot open %s\n", filename);
		return(FALSE);
	}

	if (fscanf(fp, "backup_type = %29s\n", metadata_type)
	    != 1) {
		r = FALSE;
		goto end;
	}
	if (fscanf(fp, "from_lsn = " LSN_PF "\n", &metadata_from_lsn)
			!= 1) {
		r = FALSE;
		goto end;
	}
	if (fscanf(fp, "to_lsn = " LSN_PF "\n", &metadata_to_lsn)
			!= 1) {
		r = FALSE;
		goto end;
	}
	if (fscanf(fp, "last_lsn = " LSN_PF "\n", &metadata_last_lsn)
			!= 1) {
		metadata_last_lsn = 0;
	}
	/* Optional fields */

	if (fscanf(fp, "compact = %d\n", &t) == 1) {
		xtrabackup_compact = (t == 1);
	} else {
		xtrabackup_compact = 0;
	}

	if (fscanf(fp, "recover_binlog_info = %d\n", &t) == 1) {
		recover_binlog_info = (t == 1);
	}

	if (fscanf(fp, "flushed_lsn = " LSN_PF "\n",
		   &backup_redo_log_flushed_lsn) != 1) {
		backup_redo_log_flushed_lsn = 0;
	}
end:
	fclose(fp);

	return(r);
}

/***********************************************************************
Print backup meta info to a specified buffer. */
static
void
xtrabackup_print_metadata(char *buf, size_t buf_len)
{
	snprintf(buf, buf_len,
		 "backup_type = %s\n"
		 "from_lsn = " LSN_PF "\n"
		 "to_lsn = " LSN_PF "\n"
		 "last_lsn = " LSN_PF "\n"
		 "compact = %d\n"
		 "recover_binlog_info = %d\n"
		 "flushed_lsn = " LSN_PF "\n",
		 metadata_type,
		 metadata_from_lsn,
		 metadata_to_lsn,
		 metadata_last_lsn,
		 MY_TEST(xtrabackup_compact == TRUE),
		 MY_TEST((xtrabackup_backup &&
			  (opt_binlog_info == BINLOG_INFO_LOCKLESS)) ||
			 (xtrabackup_prepare && recover_binlog_info)),
		 backup_redo_log_flushed_lsn);
}

/***********************************************************************
Stream backup meta info to a specified datasink.
@return TRUE on success, FALSE on failure. */
static
my_bool
xtrabackup_stream_metadata(ds_ctxt_t *ds_ctxt)
{
	char		buf[1024];
	size_t		len;
	ds_file_t	*stream;
	MY_STAT		mystat;
	my_bool		rc = TRUE;

	xtrabackup_print_metadata(buf, sizeof(buf));

	len = strlen(buf);

	mystat.st_size = len;
	mystat.st_mtime = my_time(0);

	stream = ds_open(ds_ctxt, XTRABACKUP_METADATA_FILENAME, &mystat);
	if (stream == NULL) {
		msg("xtrabackup: Error: cannot open output stream "
		    "for %s\n", XTRABACKUP_METADATA_FILENAME);
		return(FALSE);
	}

	if (ds_write(stream, buf, len)) {
		rc = FALSE;
	}

	if (ds_close(stream)) {
		rc = FALSE;
	}

	return(rc);
}


static
my_bool write_to_file(const char *filepath, const char *data)
{
	size_t len = strlen(data);
	FILE *fp = fopen(filepath, "w");
	if(!fp) {
		msg("xtrabackup: Error: cannot open %s\n", filepath);
		return(FALSE);
	}
	if (fwrite(data, len, 1, fp) < 1) {
		fclose(fp);
		return(FALSE);
	}

	fclose(fp);
	return TRUE;
}


/***********************************************************************
Write backup meta info to a specified file.
@return TRUE on success, FALSE on failure. */
static
my_bool
xtrabackup_write_metadata(const char *filepath)
{
	char		buf[1024];

	xtrabackup_print_metadata(buf, sizeof(buf));
	return write_to_file(filepath, buf);
}

/***********************************************************************
Read meta info for an incremental delta.
@return TRUE on success, FALSE on failure. */
static my_bool
xb_read_delta_metadata(const char *filepath, xb_delta_info_t *info)
{
	FILE*	fp;
	char	key[51];
	char	value[51];
	my_bool	r			= TRUE;

	/* set defaults */
	info->page_size = ULINT_UNDEFINED;
	info->zip_size = ULINT_UNDEFINED;
	info->space_id = ULINT_UNDEFINED;
	info->space_flags = 0;

	fp = fopen(filepath, "r");
	if (!fp) {
		/* Meta files for incremental deltas are optional */
		return(TRUE);
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
		msg("xtrabackup: page_size is required in %s\n", filepath);
		r = FALSE;
	}
	if (info->space_id == ULINT_UNDEFINED) {
		msg("xtrabackup: Warning: This backup was taken with XtraBackup 2.0.1 "
			"or earlier, some DDL operations between full and incremental "
			"backups may be handled incorrectly\n");
	}

	return(r);
}

/***********************************************************************
Write meta info for an incremental delta.
@return TRUE on success, FALSE on failure. */
my_bool
xb_write_delta_metadata(const char *filename, const xb_delta_info_t *info)
{
	ds_file_t	*f;
	char		buf[200];
	my_bool		ret;
	size_t		len;
	MY_STAT		mystat;

	snprintf(buf, sizeof(buf),
		 "page_size = %lu\n"
		 "zip_size = %lu\n"
		 "space_id = %lu\n"
		 "space_flags = %lu\n",
		 info->page_size, info->zip_size, info->space_id,
		 info->space_flags);
	len = strlen(buf);

	mystat.st_size = len;
	mystat.st_mtime = my_time(0);

	f = ds_open(ds_meta, filename, &mystat);
	if (f == NULL) {
		msg("xtrabackup: Error: cannot open output stream for %s\n",
		    filename);
		return(FALSE);
	}

	ret = (ds_write(f, buf, len) == 0);

	if (ds_close(f)) {
		ret = FALSE;
	}

	return(ret);
}

static my_bool
xtrabackup_write_info(const char *filepath)
{
	char *xtrabackup_info_data = get_xtrabackup_info(mysql_connection);
	if (!xtrabackup_info_data) {
		return FALSE;
	}

	my_bool result = write_to_file(filepath, xtrabackup_info_data);

	free(xtrabackup_info_data);
	return result;
}

/* ================= backup ================= */
void
xtrabackup_io_throttling(void)
{
	if (xtrabackup_throttle && (--io_ticket) < 0) {
		os_event_reset(wait_throttle);
		os_event_wait(wait_throttle);
	}
}

static
my_bool regex_list_check_match(
	const regex_list_t& list,
	const char* name)
{
	xb_regmatch_t tables_regmatch[1];
	for (regex_list_t::const_iterator i = list.begin(), end = list.end();
	     i != end; ++i) {
		const xb_regex_t& regex = *i;
		int regres = xb_regexec(&regex, name, 1, tables_regmatch, 0);

		if (regres != REG_NOMATCH) {
			return(TRUE);
		}
	}
	return(FALSE);
}

static
my_bool
find_filter_in_hashtable(
	const char* name,
	hash_table_t* table,
	xb_filter_entry_t** result
)
{
	xb_filter_entry_t* found = NULL;
	HASH_SEARCH(name_hash, table, ut_fold_string(name),
		    xb_filter_entry_t*,
		    found, (void) 0,
		    !strcmp(found->name, name));

	if (found && result) {
		*result = found;
	}
	return (found != NULL);
}

/************************************************************************
Checks if a given table name matches any of specifications given in
regex_list or tables_hash.

@return TRUE on match or both regex_list and tables_hash are empty.*/
static my_bool
check_if_table_matches_filters(const char *name,
	const regex_list_t& regex_list,
	hash_table_t* tables_hash)
{
	if (regex_list.empty() && !tables_hash) {
		return(FALSE);
	}

	if (regex_list_check_match(regex_list, name)) {
		return(TRUE);
	}

	if (tables_hash && find_filter_in_hashtable(name, tables_hash, NULL)) {
		return(TRUE);
	}

	return FALSE;
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

@return TRUE if entire database should be skipped,
	FALSE otherwise.
*/
static
skip_database_check_result
check_if_skip_database(
	const char* name  /*!< in: path to the database */
)
{
	/* There are some filters for databases, check them */
	xb_filter_entry_t*	database = NULL;

	if (databases_exclude_hash &&
		find_filter_in_hashtable(name, databases_exclude_hash,
					 &database) &&
		!database->has_tables) {
		/* Database is found and there are no tables specified,
		   skip entire db. */
		return DATABASE_SKIP;
	}

	if (databases_include_hash) {
		if (!find_filter_in_hashtable(name, databases_include_hash,
					      &database)) {
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

@return TRUE if the table should be skipped. */
my_bool
check_if_skip_database_by_path(
	const char* path /*!< in: path to the db directory. */
)
{
	if (databases_include_hash == NULL &&
		databases_exclude_hash == NULL) {
		return(FALSE);
	}

	const char* db_name = strrchr(path, OS_PATH_SEPARATOR);
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

@return TRUE if the table should be skipped. */
bool
check_if_skip_table(
/******************/
	const char*	name)	/*!< in: path to the table */
{
	char buf[FN_REFLEN];
	const char *dbname, *tbname;
	const char *ptr;
	char *eptr;

	if (regex_exclude_list.empty() &&
		regex_include_list.empty() &&
		tables_include_hash == NULL &&
		tables_exclude_hash == NULL &&
		databases_include_hash == NULL &&
		databases_exclude_hash == NULL) {
		return(false);
	}

	dbname = NULL;
	tbname = name;
	while ((ptr = strchr(tbname, OS_PATH_SEPARATOR)) != NULL) {
		dbname = tbname;
		tbname = ptr + 1;
	}

	if (dbname == NULL) {
		return(false);
	}

	strncpy(buf, dbname, FN_REFLEN);
	buf[tbname - 1 - dbname] = 0;

	const skip_database_check_result skip_database =
			check_if_skip_database(buf);
	if (skip_database == DATABASE_SKIP) {
		return(true);
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
		return(true);
	}
	if (check_if_table_matches_filters(buf, regex_include_list,
					   tables_include_hash)) {
		return(false);
	}
	if ((eptr = strstr(buf, "#P#")) != NULL) {
		*eptr = 0;

		if (check_if_table_matches_filters(buf, regex_exclude_list,
						   tables_exclude_hash)) {
			return(true);
		}
		if (check_if_table_matches_filters(buf, regex_include_list,
						   tables_include_hash)) {
			return(false);
		}
	}

	if (skip_database == DATABASE_DONT_SKIP_UNLESS_EXPLICITLY_EXCLUDED) {
		/* Database is in include-list, and qualified name wasn't
		   found in any of exclusion filters.*/
		return(false);
	}

	if (skip_database == DATABASE_SKIP_SOME_TABLES ||
		!regex_include_list.empty() ||
		tables_include_hash) {

		/* Include lists are present, but qualified name
		   failed to match any.*/
		return(true);
	}

	return(false);
}

/***********************************************************************
Reads the space flags from a given data file and returns the
page size. */
const page_size_t
xb_get_zip_size(pfs_os_file_t file, bool *success)
{
	byte		*buf;
	byte		*page;
	page_size_t	page_size(0, 0, false);
	ibool		ret;
	ulint		space;
	IORequest	read_request(IORequest::READ);

	buf = static_cast<byte *>(ut_malloc_nokey(2 * UNIV_PAGE_SIZE_MAX));
	page = static_cast<byte *>(ut_align(buf, UNIV_PAGE_SIZE_MAX));

	ret = os_file_read(read_request, file, page, 0, UNIV_PAGE_SIZE_MIN);
	if (!ret) {
		*success = false;
		goto end;
	}

	space = mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
	if (space == 0) {
		page_size.copy_from(univ_page_size);
	} else {
		page_size.copy_from(page_size_t(fsp_header_get_flags(page)));
	}
	*success = true;
end:
	ut_free(buf);

	return(page_size);
}

const char*
xb_get_copy_action(const char *dflt)
{
	const char *action;

	if (xtrabackup_stream) {
		if (xtrabackup_compress) {
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
		if (xtrabackup_compress) {
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

	return(action);
}

/* TODO: We may tune the behavior (e.g. by fil_aio)*/

static
my_bool
xtrabackup_copy_datafile(fil_node_t* node, uint thread_n)
{
	char			 dst_name[FN_REFLEN];
	ds_file_t		*dstfile = NULL;
	xb_fil_cur_t		 cursor;
	xb_fil_cur_result_t	 res;
	xb_write_filt_t		*write_filter = NULL;
	xb_write_filt_ctxt_t	 write_filt_ctxt;
	const char		*action;
	xb_read_filt_t		*read_filter;
	ibool			is_system;
	my_bool			rc = FALSE;

	/* Get the name and the path for the tablespace. node->name always
	contains the path (which may be absolute for remote tablespaces in
	5.6+). space->name contains the tablespace name in the form
	"./database/table.ibd" (in 5.5-) or "database/table" (in 5.6+). For a
	multi-node shared tablespace, space->name contains the name of the first
	node, but that's irrelevant, since we only need node_name to match them
	against filters, and the shared tablespace is always copied regardless
	of the filters value. */

	const char* const node_name = node->space->name;
	const char* const node_path = node->name;

	is_system = !fil_is_user_tablespace_id(node->space->id);

	if (!is_system && opt_lock_ddl_per_table) {
		mdl_lock_table(node->space->id);
	}

	if (!is_system && check_if_skip_table(node_name)) {
		msg("[%02u] Skipping %s.\n", thread_n, node_name);
		return(FALSE);
	}

	if (!changed_page_bitmap) {
		read_filter = &rf_pass_through;
	}
	else {
		read_filter = &rf_bitmap;
	}
	res = xb_fil_cur_open(&cursor, read_filter, node, thread_n);
	if (res == XB_FIL_CUR_SKIP) {
		goto skip;
	} else if (res == XB_FIL_CUR_ERROR) {
		goto error;
	}

	if (!is_system) {
		snprintf(dst_name, sizeof(dst_name), "%s.ibd", node_name);
	} else {
		strncpy(dst_name, cursor.rel_path, sizeof(dst_name));
	}

	/* Setup the page write filter */
	if (xtrabackup_incremental) {
		write_filter = &wf_incremental;
	} else if (xtrabackup_compact) {
		write_filter = &wf_compact;
	} else {
		write_filter = &wf_write_through;
	}

	memset(&write_filt_ctxt, 0, sizeof(xb_write_filt_ctxt_t));
	ut_a(write_filter->process != NULL);

	if (write_filter->init != NULL &&
	    !write_filter->init(&write_filt_ctxt, dst_name, &cursor)) {
		msg("[%02u] xtrabackup: error: "
		    "failed to initialize page write filter.\n", thread_n);
		goto error;
	}

	dstfile = ds_open(ds_data, dst_name, &cursor.statinfo);
	if (dstfile == NULL) {
		msg("[%02u] xtrabackup: error: "
		    "cannot open the destination stream for %s\n",
		    thread_n, dst_name);
		goto error;
	}

	action = xb_get_copy_action();

	if (xtrabackup_stream) {
		msg_ts("[%02u] %s %s\n", thread_n, action, node_path);
	} else {
		msg_ts("[%02u] %s %s to %s\n", thread_n, action,
		       node_path, dstfile->path);
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

	if (write_filter->finalize
	    && !write_filter->finalize(&write_filt_ctxt, dstfile)) {
		goto error;
	}

	/* close */
	msg_ts("[%02u]        ...done\n", thread_n);
	xb_fil_cur_close(&cursor);
	if (ds_close(dstfile)) {
		rc = TRUE;
	}
	if (write_filter && write_filter->deinit) {
		write_filter->deinit(&write_filt_ctxt);
	}
	return(rc);

error:
	xb_fil_cur_close(&cursor);
	if (dstfile != NULL) {
		ds_close(dstfile);
	}
	if (write_filter && write_filter->deinit) {
		write_filter->deinit(&write_filt_ctxt);
	}
	msg("[%02u] xtrabackup: Error: "
	    "xtrabackup_copy_datafile() failed.\n", thread_n);
	return(TRUE); /*ERROR*/

skip:

	if (dstfile != NULL) {
		ds_close(dstfile);
	}
	if (write_filter && write_filter->deinit) {
		write_filter->deinit(&write_filt_ctxt);
	}
	msg("[%02u] xtrabackup: Warning: We assume the "
	    "table was dropped during xtrabackup execution "
	    "and ignore the file.\n", thread_n);
	msg("[%02u] xtrabackup: Warning: skipping tablespace %s.\n",
	    thread_n, node_name);
	return(FALSE);
}

static
void
xtrabackup_choose_lsn_offset(lsn_t start_lsn)
{
	ulint no, expected_no;
	ulint blocks_in_group;
	lsn_t end_lsn;
	log_group_t *group;

	start_lsn = ut_uint64_align_down(start_lsn, OS_FILE_LOG_BLOCK_SIZE);
	end_lsn = start_lsn + RECV_SCAN_SIZE;

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	if (mysql_server_version < 50500) {
		/* server doesn't support log files larger than 4G */
		return;
	}

	if (server_flavor == FLAVOR_PERCONA_SERVER &&
	    (mysql_server_version > 50500 && mysql_server_version < 50600)) {
		/* it is Percona Server 5.5 */
		group->lsn_offset = group->lsn_offset_ps55;
		return;
	}

	if (group->lsn_offset_ps55 == group->lsn_offset ||
	    group->lsn_offset_ps55 == (lsn_t) -1) {
		/* we have only one option */
		return;
	}

	no = (ulint) -1;

	blocks_in_group = log_block_convert_lsn_to_no(
		log_group_get_capacity(group)) - 1;

	lsn_t offsets[2] = {group->lsn_offset,
			    group->lsn_offset_ps55};

	expected_no = log_block_convert_lsn_to_no(start_lsn);

	for (int i = 0; i < 2; i++) {
		group->lsn_offset = offsets[i];

		/* read log block number */
		if (group->lsn_offset < group->file_size * group->n_files &&
		    (log_group_calc_lsn_offset(start_lsn, group) %
		     UNIV_PAGE_SIZE) % OS_MIN_LOG_BLOCK_SIZE == 0) {
			log_group_read_log_seg(log_sys->buf, group,
					       start_lsn, end_lsn);
			no = log_block_get_hdr_no(log_sys->buf);
		}

		if ((no <= expected_no &&
			((expected_no - no) % blocks_in_group) == 0) ||
		    ((expected_no | 0x40000000UL) - no) % blocks_in_group == 0) {
			/* offset looks ok */
			return;
		}
	}
}

/*******************************************************//**
Scans log from a buffer and writes new log data to the outpud datasinc.
@return true if success */
static
bool
xtrabackup_scan_log_recs(
/*===============*/
	log_group_t*	group,		/*!< in: log group */
	bool		is_last,	/*!< in: whether it is last segment
					to copy */
	lsn_t		start_lsn,	/*!< in: buffer start lsn */
	lsn_t*		contiguous_lsn,	/*!< in/out: it is known that all log
					groups contain contiguous log data up
					to this lsn */
	lsn_t*		group_scanned_lsn,/*!< out: scanning succeeded up to
					this lsn */
	lsn_t		checkpoint_lsn,	/*!< in: latest checkpoint LSN */
	bool*		finished)	/*!< out: false if is not able to scan
					any more in this log group */
{
	lsn_t		scanned_lsn;
	ulint		data_len;
	ulint		write_size;
	const byte*	log_block;
	bool		more_data	= false;

	ulint		scanned_checkpoint_no = 0;

	*finished = false;
	scanned_lsn = start_lsn;
	log_block = log_sys->buf;

	while (log_block < log_sys->buf + RECV_SCAN_SIZE && !*finished) {
		ulint	no = log_block_get_hdr_no(log_block);
		ulint	scanned_no = log_block_convert_lsn_to_no(scanned_lsn);
		ibool	checksum_is_ok = log_block_checksum_is_ok(log_block);

		if (no != scanned_no && checksum_is_ok) {
			ulint blocks_in_group;

			blocks_in_group = log_block_convert_lsn_to_no(
				log_group_get_capacity(group)) - 1;

			if ((no < scanned_no &&
			    ((scanned_no - no) % blocks_in_group) == 0) ||
			    no == 0 ||
			    /* Log block numbers wrap around at 0x3FFFFFFF */
			    ((scanned_no | 0x40000000UL) - no) %
			    blocks_in_group == 0) {

				/* old log block, do nothing */
				*finished = true;
				break;
			}

			msg("xtrabackup: error:"
			    " log block numbers mismatch:\n"
			    "xtrabackup: error: expected log block no. %lu,"
			    " but got no. %lu from the log file.\n",
			    (ulong) scanned_no, (ulong) no);

			if ((no - scanned_no) % blocks_in_group == 0) {
				msg("xtrabackup: error:"
				    " it looks like InnoDB log has wrapped"
				    " around before xtrabackup could"
				    " process all records due to either"
				    " log copying being too slow, or "
				    " log files being too small.\n");
			}

			return(false);
		} else if (!checksum_is_ok) {
			/* Garbage or an incompletely written log block */

			msg("xtrabackup: warning: Log block checksum mismatch"
			    " (block no %lu at lsn " LSN_PF "): \n"
			    "expected %lu, calculated checksum %lu\n",
				(ulong) no,
				scanned_lsn,
				(ulong) log_block_get_checksum(log_block),
				(ulong) log_block_calc_checksum(log_block));
			msg("xtrabackup: warning: this is possible when the "
			    "log block has not been fully written by the "
			    "server, will retry later.\n");
			*finished = true;
			break;
		}

		if (log_block_get_flush_bit(log_block)) {
			/* This block was a start of a log flush operation:
			we know that the previous flush operation must have
			been completed for all log groups before this block
			can have been flushed to any of the groups. Therefore,
			we know that log data is contiguous up to scanned_lsn
			in all non-corrupt log groups. */

			if (scanned_lsn > *contiguous_lsn) {

				*contiguous_lsn = scanned_lsn;
			}
		}

		data_len = log_block_get_data_len(log_block);

		if (
		    (scanned_checkpoint_no > 0)
		    && (log_block_get_checkpoint_no(log_block)
		       < scanned_checkpoint_no)
		    && (scanned_checkpoint_no
			- log_block_get_checkpoint_no(log_block)
			> 0x80000000UL)) {

			/* Garbage from a log buffer flush which was made
			before the most recent database recovery */

			*finished = true;
			break;
		}

		if (!recv_sys->parse_start_lsn
		    && (log_block_get_first_rec_group(log_block) > 0)) {

			/* We found a point from which to start the parsing
			of log records */

			recv_sys->parse_start_lsn = scanned_lsn
				+ log_block_get_first_rec_group(log_block);
			recv_sys->scanned_lsn = recv_sys->parse_start_lsn;
			recv_sys->recovered_lsn = recv_sys->parse_start_lsn;
		}

		scanned_lsn = scanned_lsn + data_len;
		scanned_checkpoint_no = log_block_get_checkpoint_no(log_block);

		if (scanned_lsn > recv_sys->scanned_lsn) {

			/* We were able to find more log data: add it to the
			parsing buffer if parse_start_lsn is already
			non-zero */

			if (recv_sys->len + 4 * OS_FILE_LOG_BLOCK_SIZE
			    >= RECV_PARSING_BUF_SIZE) {
				ib::error() << "Log parsing buffer overflow."
					" Recovery may have failed!";

				recv_sys->found_corrupt_log = true;

			} else if (!recv_sys->found_corrupt_log) {
				more_data = recv_sys_add_to_parsing_buf(
					log_block, scanned_lsn);
			}

			recv_sys->scanned_lsn = scanned_lsn;
			recv_sys->scanned_checkpoint_no
				= log_block_get_checkpoint_no(log_block);
		}

		if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
			/* Log data for this group ends here */

			*finished = true;
		} else {
			log_block += OS_FILE_LOG_BLOCK_SIZE;
		}
	}

	*group_scanned_lsn = scanned_lsn;

	/* ===== write log to 'xtrabackup_logfile' ====== */
	if (!*finished) {
		write_size = RECV_SCAN_SIZE;
	} else {
		write_size = ut_uint64_align_up(scanned_lsn,
					OS_FILE_LOG_BLOCK_SIZE) - start_lsn;
		if (!is_last && scanned_lsn % OS_FILE_LOG_BLOCK_SIZE) {
			write_size -= OS_FILE_LOG_BLOCK_SIZE;
		}
	}

	if (ds_write(dst_log_file, log_sys->buf, write_size)) {
		msg("xtrabackup: Error: "
		    "write to logfile failed\n");
		return(false);
	}

	if (more_data && !recv_sys->found_corrupt_log) {
		/* Try to parse more log records */

		if (recv_parse_log_recs(checkpoint_lsn,	/*!< in: latest checkpoint LSN */
					STORE_NO)) {
			ut_ad(recv_sys->found_corrupt_log
			      || recv_sys->found_corrupt_fs
			      || recv_sys->mlog_checkpoint_lsn
			      == recv_sys->recovered_lsn);
			return(true);
		}

		if (recv_sys->recovered_offset > RECV_PARSING_BUF_SIZE / 4) {
			/* Move parsing buffer data to the buffer start */

			recv_sys_justify_left_parsing_buf();
		}
	}

	return(true);
}

static my_bool
xtrabackup_copy_logfile(lsn_t from_lsn, my_bool is_last)
{
	/* definition from recv_recovery_from_checkpoint_start() */
	log_group_t*	group;
	lsn_t		group_scanned_lsn;
	lsn_t		contiguous_lsn;

	ut_a(dst_log_file != NULL);

	/* read from checkpoint_lsn_start to current */
	contiguous_lsn = ut_uint64_align_down(from_lsn, OS_FILE_LOG_BLOCK_SIZE);

	/* TODO: We must check the contiguous_lsn still exists in log file.. */

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		bool	finished;
		lsn_t	start_lsn;
		lsn_t	end_lsn;

		/* reference recv_group_scan_log_recs() */
		finished = false;

		start_lsn = contiguous_lsn;

		while (!finished) {

			end_lsn = start_lsn + RECV_SCAN_SIZE;

			xtrabackup_io_throttling();

			mutex_enter(&log_sys->mutex);

			log_group_read_log_seg(log_sys->buf,
					       group, start_lsn, end_lsn);

			 if (!xtrabackup_scan_log_recs(group, is_last,
				start_lsn, &contiguous_lsn, &group_scanned_lsn,
				from_lsn, &finished)) {
				goto error;
			 }

			mutex_exit(&log_sys->mutex);

			start_lsn = end_lsn;

		}

		group->scanned_lsn = group_scanned_lsn;

		msg_ts(">> log scanned up to (" LSN_PF ")\n",
		       group->scanned_lsn);

		group = UT_LIST_GET_NEXT(log_groups, group);

		/* update global variable*/
		log_copy_scanned_lsn = group_scanned_lsn;

		/* innodb_mirrored_log_groups must be 1, no other groups */
		ut_a(group == NULL);

		debug_sync_point("xtrabackup_copy_logfile_pause");

	}


	return(FALSE);

error:
	mutex_exit(&log_sys->mutex);
	ds_close(dst_log_file);
	msg("xtrabackup: Error: xtrabackup_copy_logfile() failed.\n");
	return(TRUE);
}

static
#ifndef __WIN__
void*
#else
ulint
#endif
log_copying_thread(
	void*	arg __attribute__((unused)))
{
	/*
	  Initialize mysys thread-specific memory so we can
	  use mysys functions in this thread.
	*/
	my_thread_init();

	ut_a(dst_log_file != NULL);

	log_copying_running = TRUE;

	while(log_copying) {
		os_event_reset(log_copying_stop);
		os_event_wait_time_low(log_copying_stop,
				       xtrabackup_log_copy_interval * 1000ULL,
				       0);
		if (log_copying) {
			if(xtrabackup_copy_logfile(log_copy_scanned_lsn,
						   FALSE)) {

				exit(EXIT_FAILURE);
			}
		}
	}

	/* last copying */
	if(xtrabackup_copy_logfile(log_copy_scanned_lsn, TRUE)) {

		exit(EXIT_FAILURE);
	}

	log_copying_running = FALSE;
	my_thread_end();
	os_thread_exit();

	return(0);
}

/* io throttle watching (rough) */
static
#ifndef __WIN__
void*
#else
ulint
#endif
io_watching_thread(
	void*	arg)
{
	(void)arg;
	/* currently, for --backup only */
	ut_a(xtrabackup_backup);

	io_watching_thread_running = TRUE;

	while (log_copying) {
		os_thread_sleep(1000000); /*1 sec*/
		io_ticket = xtrabackup_throttle;
		os_event_set(wait_throttle);
	}

	/* stop io throttle */
	xtrabackup_throttle = 0;
	os_event_set(wait_throttle);

	io_watching_thread_running = FALSE;

	os_thread_exit();

	return(0);
}

/************************************************************************
I/o-handler thread function. */
static

#ifndef __WIN__
void*
#else
ulint
#endif
io_handler_thread(
/*==============*/
	void*	arg)
{
	ulint	segment;


	segment = *((ulint*)arg);

 	while (srv_shutdown_state != SRV_SHUTDOWN_EXIT_THREADS) {
		fil_aio_wait(segment);
	}

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit.
	The thread actually never comes here because it is exited in an
	os_event_wait(). */

	os_thread_exit();

#ifndef __WIN__
	return(NULL);				/* Not reached */
#else
	return(0);
#endif
}

/**************************************************************************
Datafiles copying thread.*/
static
os_thread_ret_t
data_copy_thread_func(
/*==================*/
	void *arg) /* thread context */
{
	data_thread_ctxt_t	*ctxt = (data_thread_ctxt_t *) arg;
	uint			num = ctxt->num;
	fil_node_t*		node;

	/*
	  Initialize mysys thread-specific memory so we can
	  use mysys functions in this thread.
	*/
	my_thread_init();

	debug_sync_point("data_copy_thread_func");

	while ((node = datafiles_iter_next(ctxt->it)) != NULL &&
		!*(ctxt->error)) {

		/* copy the datafile */
		if(xtrabackup_copy_datafile(node, num)) {
			msg("[%02u] xtrabackup: Error: "
			    "failed to copy datafile.\n", num);
			*(ctxt->error) = true;
		}
	}

	mutex_enter(ctxt->count_mutex);
	(*ctxt->count)--;
	mutex_exit(ctxt->count_mutex);

	my_thread_end();
	os_thread_exit();
	OS_THREAD_DUMMY_RETURN;
}

/************************************************************************
Initialize the appropriate datasink(s). Both local backups and streaming in the
'xbstream' format allow parallel writes so we can write directly.

Otherwise (i.e. when streaming in the 'tar' format) we need 2 separate datasinks
for the data stream (and don't allow parallel data copying) and for metainfo
files (including xtrabackup_logfile). The second datasink writes to temporary
files first, and then streams them in a serialized way when closed. */
static void
xtrabackup_init_datasinks(void)
{
	if (xtrabackup_parallel > 1 && xtrabackup_stream &&
	    xtrabackup_stream_fmt == XB_STREAM_FMT_TAR) {
		msg("xtrabackup: warning: the --parallel option does not have "
		    "any effect when streaming in the 'tar' format. "
		    "You can use the 'xbstream' format instead.\n");
		xtrabackup_parallel = 1;
	}

	/* Start building out the pipelines from the terminus back */
	if (xtrabackup_stream) {
		/* All streaming goes to stdout */
		ds_data = ds_meta = ds_redo = ds_create(xtrabackup_target_dir,
						        DS_TYPE_STDOUT);
	} else {
		/* Local filesystem */
		ds_data = ds_meta = ds_redo = ds_create(xtrabackup_target_dir,
						        DS_TYPE_LOCAL);
	}

	/* Track it for destruction */
	xtrabackup_add_datasink(ds_data);

	/* Stream formatting */
	if (xtrabackup_stream) {
		ds_ctxt_t	*ds;
		if (xtrabackup_stream_fmt == XB_STREAM_FMT_TAR) {
			ds = ds_create(xtrabackup_target_dir, DS_TYPE_ARCHIVE);
		} else if (xtrabackup_stream_fmt == XB_STREAM_FMT_XBSTREAM) {
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
			ds_redo = ds_meta = ds_create(xtrabackup_target_dir,
						      DS_TYPE_TMPFILE);
			xtrabackup_add_datasink(ds_meta);
			ds_set_pipe(ds_meta, ds);
		} else {
			ds_redo = ds_meta = ds_data;
		}
	}

	/* Encryption */
	if (xtrabackup_encrypt) {
		ds_ctxt_t	*ds;

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

	/* Compression for ds_data and ds_redo */
	if (xtrabackup_compress) {
		ds_ctxt_t	*ds;

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

		ds = ds_create(xtrabackup_target_dir, DS_TYPE_COMPRESS);
		xtrabackup_add_datasink(ds);
		ds_set_pipe(ds, ds_data);
		if (ds_data != ds_redo) {
			ds_data = ds;
			ds = ds_create(xtrabackup_target_dir, DS_TYPE_COMPRESS);
			xtrabackup_add_datasink(ds);
			ds_set_pipe(ds, ds_redo);
			ds_redo = ds;
		} else {
			ds_redo = ds_data = ds;
		}
	}
}

/************************************************************************
Destroy datasinks.

Destruction is done in the specific order to not violate their order in the
pipeline so that each datasink is able to flush data down the pipeline. */
static void xtrabackup_destroy_datasinks(void)
{
	for (uint i = actual_datasinks; i > 0; i--) {
		ds_destroy(datasinks[i-1]);
		datasinks[i-1] = NULL;
	}
	ds_data = NULL;
	ds_meta = NULL;
	ds_redo = NULL;
}

#define SRV_N_PENDING_IOS_PER_THREAD 	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100

/************************************************************************
@return TRUE if table should be opened. */
static
bool
xb_check_if_open_tablespace(
	const char*	db,
	const char*	table)
{
	char buf[FN_REFLEN];

	snprintf(buf, sizeof(buf), "%s/%s", db, table);

	return !check_if_skip_table(buf);
}

/************************************************************************
Initializes the I/O and tablespace cache subsystems. */
static
void
xb_fil_io_init(void)
/*================*/
{
	srv_n_file_io_threads = srv_n_read_io_threads;

	os_aio_init(srv_n_read_io_threads,
		    srv_n_write_io_threads,
		    SRV_MAX_N_PENDING_SYNC_IOS);

	fil_init(srv_file_per_table ? 50000 : 5000, LONG_MAX);

	fsp_init();
}

Datafile *xb_new_datafile(
	const char *name,
	bool is_remote)
{
	if (is_remote) {
		RemoteDatafile *remote_file = new RemoteDatafile();
		remote_file->set_name(name);
		return(remote_file);
	} else {
		Datafile *file = new Datafile();
		file->set_name(name);
		file->make_filepath(".", name, IBD);
		return(file);
	}
}

void
xb_load_single_table_tablespace(
	const char *dirname,
	const char *filname,
	bool is_remote)
{
	/* Ignore .isl files on XtraBackup recovery. All tablespaces must be
	local. */
	if (is_remote && !srv_backup_mode) {
		return;
	}

	/* The name ends in .ibd or .isl;
	try opening the file */
	char*	name;
	size_t	dirlen		= dirname == NULL ? 0 : strlen(dirname);
	size_t	namelen		= strlen(filname);
	ulint	pathlen		= dirname == NULL ? namelen + 1: dirlen + namelen + 2;
	lsn_t	flush_lsn;
	dberr_t	err;
	fil_space_t	*space;

	name = static_cast<char*>(ut_malloc_nokey(pathlen));

	if (dirname != NULL) {
		ut_snprintf(name, pathlen, "%s/%s", dirname, filname);
		name[pathlen - 5] = 0;
	} else {
		ut_snprintf(name, pathlen, "%s", filname);
		name[pathlen - 5] = 0;
	}

	Datafile *file = xb_new_datafile(name, is_remote);

	if (file->open_read_only(true) != DB_SUCCESS) {
		ut_free(name);
		exit(EXIT_FAILURE);
	}

	err = file->validate_first_page(&flush_lsn, false);

	if (err == DB_SUCCESS) {

		os_offset_t	node_size = os_file_get_size(file->handle());
		bool		is_tmp = FSP_FLAGS_GET_TEMPORARY(file->flags());
		os_offset_t	n_pages;

		ut_a(node_size != (os_offset_t) -1);

		n_pages = node_size / page_size_t(file->flags()).physical();

		space = fil_space_create(
			name, file->space_id(), file->flags(),
			is_tmp ? FIL_TYPE_TEMPORARY : FIL_TYPE_TABLESPACE);

		ut_a(space != NULL);

		/* For encrypted tablespace, initialize encryption
		information.*/
		if (FSP_FLAGS_GET_ENCRYPTION(file->flags())) {
			if (srv_backup_mode || !use_dumped_tablespace_keys) {
				byte*	key = file->m_encryption_key;
				byte*	iv = file->m_encryption_iv;

				if (key && iv) {
					err = fil_set_encryption(
						space->id,
						Encryption::AES, key, iv);
				}
			} else {
				byte key[ENCRYPTION_KEY_LEN];
				byte iv[ENCRYPTION_KEY_LEN];

				xb_fetch_tablespace_key(space->id, key, iv);

				err = fil_set_encryption(space->id,
					Encryption::AES, key, iv);
			}

			ut_ad(err == DB_SUCCESS);
		}

		if (!fil_node_create(file->filepath(), n_pages, space,
				     false, false)) {
			ut_error;
		}

		/* by opening the tablespace we forcing node and space objects
		in the cache to be populated with fields from space header */
		fil_space_open(space->name);

		if (!srv_backup_mode || srv_close_files) {
			fil_space_close(space->name);
		}
	} else {
		/* allow corrupted first page for xtrabackup, it could be just
		zero-filled page, which we'll restore from redo log later */
		if (xtrabackup_backup && err != DB_PAGE_IS_BLANK) {
			exit(EXIT_FAILURE);
		}
		if (xtrabackup_prepare) {
			exit(EXIT_FAILURE);
		}
	}

	ut_free(name);

	delete file;
}

static
bool
is_remote_tablespace_name(const char *path)
{
	size_t len = strlen(path);
	return len > 4 && strcmp(path + len - 4, ".isl") == 0;
}

static
bool
is_local_tablespace_name(const char *path)
{
	size_t len = strlen(path);
	return len > 4 && strcmp(path + len - 4, ".ibd") == 0;
}

static
bool
is_tablespace_name(const char *path)
{
	return is_remote_tablespace_name(path)
		|| is_local_tablespace_name(path);
}

/********************************************************************//**
At the server startup, if we need crash recovery, scans the database
directories under the MySQL datadir, looking for .ibd files. Those files are
single-table tablespaces. We need to know the space id in each of them so that
we know into which file we should look to check the contents of a page stored
in the doublewrite buffer, also to know where to apply log records where the
space id is != 0.
@return	DB_SUCCESS or error number */
UNIV_INTERN
dberr_t
xb_load_single_table_tablespaces(bool (*pred)(const char*, const char*))
/*===================================*/
{
	int		ret;
	char*		dbpath		= NULL;
	ulint		dbpath_len	= 100;
	os_file_dir_t	dir;
	os_file_dir_t	dbdir;
	os_file_stat_t	dbinfo;
	os_file_stat_t	fileinfo;
	dberr_t		err		= DB_SUCCESS;

	/* The datadir of MySQL is always the default directory of mysqld */

	dir = os_file_opendir(fil_path_to_mysql_datadir, true);

	if (dir == NULL) {

		return(DB_ERROR);
	}

	dbpath = static_cast<char*>(ut_malloc_nokey(dbpath_len));

	/* Scan all directories under the datadir. They are the database
	directories of MySQL. */

	ret = fil_file_readdir_next_file(&err, fil_path_to_mysql_datadir, dir,
					 &dbinfo);
	while (ret == 0) {
		ulint len;
		bool is_tablespace;
		bool is_remote;

		is_tablespace = is_tablespace_name(dbinfo.name);
		is_remote = is_remote_tablespace_name(dbinfo.name);

		/* General tablespaces are always at the first level of the
		data home dir */
		if (dbinfo.type == OS_FILE_TYPE_FILE && is_tablespace &&
		    !(pred && !pred(".", dbinfo.name))) {
			xb_load_single_table_tablespace(
				NULL, dbinfo.name, is_remote);
		}

		if (dbinfo.type == OS_FILE_TYPE_FILE
		    || dbinfo.type == OS_FILE_TYPE_UNKNOWN) {

			goto next_datadir_item;
		}

		/* We found a symlink or a directory; try opening it to see
		if a symlink is a directory */

		len = strlen(fil_path_to_mysql_datadir)
			+ strlen (dbinfo.name) + 2;
		if (len > dbpath_len) {
			dbpath_len = len;

			if (dbpath) {
				ut_free(dbpath);
			}

			dbpath = static_cast<char*>(ut_malloc_nokey(dbpath_len));
		}
		ut_snprintf(dbpath, dbpath_len,
			    "%s/%s", fil_path_to_mysql_datadir, dbinfo.name);
		os_normalize_path(dbpath);

		if (check_if_skip_database_by_path(dbpath)) {
			fprintf(stderr, "Skipping db: %s\n", dbpath);
			goto next_datadir_item;
		}

		/* We want wrong directory permissions to be a fatal error for
		XtraBackup. */
		dbdir = os_file_opendir(dbpath, true);

		if (dbdir != NULL) {

			/* We found a database directory; loop through it,
			looking for possible .ibd and .isl files in it */

			ret = fil_file_readdir_next_file(&err, dbpath, dbdir,
							 &fileinfo);
			while (ret == 0) {
				if (fileinfo.type == OS_FILE_TYPE_DIR) {
					goto next_file_item;
				}

				is_tablespace = is_tablespace_name(
					fileinfo.name);
				is_remote = is_remote_tablespace_name(
					fileinfo.name);

				/* We found a symlink or a file */
				if (is_tablespace
				    && (!pred
					|| pred(dbinfo.name, fileinfo.name))) {
					xb_load_single_table_tablespace(
						dbinfo.name, fileinfo.name,
						is_remote);
				}
next_file_item:
				ret = fil_file_readdir_next_file(&err,
								 dbpath, dbdir,
								 &fileinfo);
			}

			if (0 != os_file_closedir(dbdir)) {
				fputs("InnoDB: Warning: could not"
				      " close database directory ", stderr);
				fputs(dbpath, stderr);
				putc('\n', stderr);

				err = DB_ERROR;
			}

		} else {

			err = DB_ERROR;
			break;

		}

next_datadir_item:
		ret = fil_file_readdir_next_file(&err,
						 fil_path_to_mysql_datadir,
						 dir, &dbinfo);
	}

	ut_free(dbpath);

	if (0 != os_file_closedir(dir)) {
		fprintf(stderr,
			"InnoDB: Error: could not close MySQL datadir\n");

		return(DB_ERROR);
	}

	return(err);
}

/****************************************************************************
Populates the tablespace memory cache by scanning for and opening data files.
@returns DB_SUCCESS or error code.*/
static
ulint
xb_load_tablespaces(void)
/*=====================*/
{
	ulint	i;
	ulint	err;
	ulint   sum_of_new_sizes;
        lsn_t	flush_lsn;
        bool	create_new_db;

	for (i = 0; i < srv_n_file_io_threads; i++) {
		thread_nr[i] = i;

		os_thread_create(io_handler_thread, thread_nr + i,
				 thread_ids + i);
    	}

	os_thread_sleep(200000); /*0.2 sec*/

	err = srv_sys_space.check_file_spec(&create_new_db, 0);

	/* create_new_db must not be true. */
	if (err != DB_SUCCESS || create_new_db) {
		msg("xtrabackup: could not find data files at the "
		    "specified datadir\n");
		return(DB_ERROR);
	}

	err = srv_sys_space.open_or_create(false, false, &sum_of_new_sizes,
					   &flush_lsn);

	if (err != DB_SUCCESS) {
		msg("xtrabackup: Could not open or create data files.\n"
		    "xtrabackup: If you tried to add new data files, and it "
		    "failed here,\n"
		    "xtrabackup: you should now edit innodb_data_file_path in "
		    "my.cnf back\n"
		    "xtrabackup: to what it was, and remove the new ibdata "
		    "files InnoDB created\n"
		    "xtrabackup: in this failed attempt. InnoDB only wrote "
		    "those files full of\n"
		    "xtrabackup: zeros, but did not yet use them in any way. "
		    "But be careful: do not\n"
		    "xtrabackup: remove old data files which contain your "
		    "precious data!\n");
		return(err);
	}

	/* Add separate undo tablespaces to fil_system */

	err = srv_undo_tablespaces_init(FALSE,
					TRUE,
					srv_undo_tablespaces,
					&srv_undo_tablespaces_open);
	if (err != DB_SUCCESS) {
		return(err);
	}

	/* It is important to call fil_load_single_table_tablespace() after
	srv_undo_tablespaces_init(), because fil_is_user_tablespace_id() *
	relies on srv_undo_tablespaces_open to be properly initialized */

	msg("xtrabackup: Generating a list of tablespaces\n");

	err = xb_load_single_table_tablespaces(xb_check_if_open_tablespace);
	if (err != DB_SUCCESS) {
		return(err);
	}

	debug_sync_point("xtrabackup_load_tablespaces_pause");

	return(DB_SUCCESS);
}

/************************************************************************
Initialize the tablespace memory cache and populate it by scanning for and
opening data files.
@returns DB_SUCCESS or error code.*/
ulint
xb_data_files_init(void)
/*====================*/
{
	xb_fil_io_init();

	return(xb_load_tablespaces());
}

/************************************************************************
Destroy the tablespace memory cache. */
void
xb_data_files_close(void)
/*====================*/
{
	ulint	i;

	srv_shutdown_state = SRV_SHUTDOWN_EXIT_THREADS;

	/* All threads end up waiting for certain events. Put those events
	to the signaled state. Then the threads will exit themselves after
	os_event_wait(). */
	for (i = 0; i < 1000; i++) {

		if (!buf_page_cleaner_is_active
		    && os_aio_all_slots_free()) {
			os_aio_wake_all_threads_at_shutdown();
		}

		/* f. dict_stats_thread is signaled from
		logs_empty_and_mark_files_at_shutdown() and should have
		already quit or is quitting right now. */

		bool	active = os_thread_active();

		os_thread_sleep(100000);

		if (!active) {
			break;
		}
	}

	if (i == 1000) {
		ib::warn() << os_thread_count << " threads created by InnoDB"
			" had not exited at shutdown!";
	}

	os_aio_free();

	fil_close_all_files();

	fil_close();

	/* Free the double write data structures. */
	if (buf_dblwr) {
		buf_dblwr_free();
	}

	/* Reset srv_file_io_threads to its default value to avoid confusing
	warning on --prepare in innobase_start_or_create_for_mysql()*/
	srv_n_file_io_threads = 4;

	srv_shutdown_state = SRV_SHUTDOWN_NONE;
}

/***********************************************************************
Allocate and initialize the entry for databases and tables filtering
hash tables. If memory allocation is not successful, terminate program.
@return pointer to the created entry.  */
static
xb_filter_entry_t *
xb_new_filter_entry(
/*================*/
	const char*	name)	/*!< in: name of table/database */
{
	xb_filter_entry_t	*entry;
	ulint namelen = strlen(name);

	ut_a(namelen <= NAME_LEN * 2 + 1);

	entry = static_cast<xb_filter_entry_t *>
		(ut_zalloc_nokey(sizeof(xb_filter_entry_t) + namelen + 1));
	entry->name = ((char*)entry) + sizeof(xb_filter_entry_t);
	strcpy(entry->name, name);
	entry->has_tables = FALSE;

	return entry;
}

/***********************************************************************
Add entry to hash table. If hash table is NULL, allocate and initialize
new hash table */
static
xb_filter_entry_t*
xb_add_filter(
/*========================*/
	const char*	name,	/*!< in: name of table/database */
	hash_table_t**	hash)	/*!< in/out: hash to insert into */
{
	xb_filter_entry_t*	entry;

	entry = xb_new_filter_entry(name);

	if (UNIV_UNLIKELY(*hash == NULL)) {
		*hash = hash_create(1000);
	}
	HASH_INSERT(xb_filter_entry_t,
		name_hash, *hash,
		ut_fold_string(entry->name),
		entry);

	return entry;
}

/***********************************************************************
Validate name of table or database. If name is invalid, program will
be finished with error code */
static
void
xb_validate_name(
/*=============*/
	const char*	name,	/*!< in: name */
	size_t		len)	/*!< in: length of name */
{
	const char*	p;

	/* perform only basic validation. validate length and
	path symbols */
	if (len > NAME_LEN) {
		msg("xtrabackup: name `%s` is too long.\n", name);
		exit(EXIT_FAILURE);
	}
	p = strpbrk(name, "/\\~");
	if (p && p - name < NAME_LEN) {
		msg("xtrabackup: name `%s` is not valid.\n", name);
		exit(EXIT_FAILURE);
	}
}

/***********************************************************************
Register new filter entry which can be either database
or table name.  */
static
void
xb_register_filter_entry(
/*=====================*/
	const char*	name,	/*!< in: name */
	hash_table_t** databases_hash,
	hash_table_t** tables_hash)
{
	const char*		p;
	size_t			namelen;
	xb_filter_entry_t*	db_entry = NULL;

	namelen = strlen(name);
	if ((p = strchr(name, '.')) != NULL) {
		char dbname[NAME_LEN + 1];

		xb_validate_name(name, p - name);
		xb_validate_name(p + 1, namelen - (p - name));

		strncpy(dbname, name, p - name);
		dbname[p - name] = 0;

		if (*databases_hash) {
			HASH_SEARCH(name_hash, (*databases_hash),
					ut_fold_string(dbname),
					xb_filter_entry_t*,
					db_entry, (void) 0,
					!strcmp(db_entry->name, dbname));
		}
		if (!db_entry) {
			db_entry = xb_add_filter(dbname, databases_hash);
		}
		db_entry->has_tables = TRUE;
		xb_add_filter(name, tables_hash);
	} else {
		xb_validate_name(name, namelen);

		xb_add_filter(name, databases_hash);
	}
}

static
void
xb_register_include_filter_entry(
	const char* name)
{
	xb_register_filter_entry(name, &databases_include_hash,
				 &tables_include_hash);
}

static
void
xb_register_exclude_filter_entry(
	const char* name)
{
	xb_register_filter_entry(name, &databases_exclude_hash,
				 &tables_exclude_hash);
}

/***********************************************************************
Register new table for the filter.  */
static
void
xb_register_table(
/*==============*/
	const char* name)	/*!< in: name of table */
{
	if (strchr(name, '.') == NULL) {
		msg("xtrabackup: `%s` is not fully qualified name.\n", name);
		exit(EXIT_FAILURE);
	}

	xb_register_include_filter_entry(name);
}

static
bool compile_regex(
	const char* regex_string,
	const char* error_context,
	xb_regex_t* compiled_re)
{
	char	errbuf[100];
	int	ret = xb_regcomp(compiled_re, regex_string, REG_EXTENDED);
	if (ret != 0) {
		xb_regerror(ret, compiled_re, errbuf, sizeof(errbuf));
		msg("xtrabackup: error: %s regcomp(%s): %s\n",
			error_context, regex_string, errbuf);
		return false;
	}
	return true;
}

static
void
xb_add_regex_to_list(
	const char* regex,  /*!< in: regex */
	const char* error_context,  /*!< in: context to error message */
	regex_list_t* list) /*! in: list to put new regex to */
{
	xb_regex_t compiled_regex;
	if (!compile_regex(regex, error_context, &compiled_regex)) {
		exit(EXIT_FAILURE);
	}

	list->push_back(compiled_regex);
}

/***********************************************************************
Register new regex for the include filter.  */
static
void
xb_register_include_regex(
/*==============*/
	const char* regex)	/*!< in: regex */
{
	xb_add_regex_to_list(regex, "tables", &regex_include_list);
}

/***********************************************************************
Register new regex for the exclude filter.  */
static
void
xb_register_exclude_regex(
/*==============*/
	const char* regex)	/*!< in: regex */
{
	xb_add_regex_to_list(regex, "tables-exclude", &regex_exclude_list);
}

typedef void (*insert_entry_func_t)(const char*);

/***********************************************************************
Scan string and load filter entries from it.  */
static
void
xb_load_list_string(
/*================*/
	char* list,			/*!< in: string representing a list */
	const char* delimiters,		/*!< in: delimiters of entries */
	insert_entry_func_t ins)	/*!< in: callback to add entry */
{
	char*	p;
	char*	saveptr;

	p = strtok_r(list, delimiters, &saveptr);
	while (p) {

		ins(p);

		p = strtok_r(NULL, delimiters, &saveptr);
	}
}

/***********************************************************************
Scan file and load filter entries from it.  */
static
void
xb_load_list_file(
/*==============*/
	const char* filename,		/*!< in: name of file */
	insert_entry_func_t ins)	/*!< in: callback to add entry */
{
	char	name_buf[NAME_LEN*2+2];
	FILE*	fp;

	/* read and store the filenames */
	fp = fopen(filename, "r");
	if (!fp) {
		msg("xtrabackup: cannot open %s\n",
		    filename);
		exit(EXIT_FAILURE);
	}
	while (fgets(name_buf, sizeof(name_buf), fp) != NULL) {
		char*	p = strchr(name_buf, '\n');
		if (p) {
			*p = '\0';
		} else {
			msg("xtrabackup: `%s...` name is too long", name_buf);
			exit(EXIT_FAILURE);
		}

		ins(name_buf);
	}

	fclose(fp);
}


static
void
xb_filters_init()
{
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
		xb_load_list_string(xtrabackup_tables, ",",
				    xb_register_include_regex);
	}

	if (xtrabackup_tables_file) {
		xb_load_list_file(xtrabackup_tables_file, xb_register_table);
	}

	if (xtrabackup_tables_exclude) {
		xb_load_list_string(xtrabackup_tables_exclude, ",",
				    xb_register_exclude_regex);
	}
}

static
void
xb_filter_hash_free(hash_table_t* hash)
{
	ulint	i;

	/* free the hash elements */
	for (i = 0; i < hash_get_n_cells(hash); i++) {
		xb_filter_entry_t*	table;

		table = static_cast<xb_filter_entry_t *>
			(HASH_GET_FIRST(hash, i));

		while (table) {
			xb_filter_entry_t*	prev_table = table;

			table = static_cast<xb_filter_entry_t *>
				(HASH_GET_NEXT(name_hash, prev_table));

			HASH_DELETE(xb_filter_entry_t, name_hash, hash,
				ut_fold_string(prev_table->name), prev_table);
			ut_free(prev_table);
		}
	}

	/* free hash */
	hash_table_free(hash);
}

static void xb_regex_list_free(regex_list_t* list)
{
	while (list->size() > 0) {
		xb_regfree(&list->front());
		list->pop_front();
	}
}

/************************************************************************
Destroy table filters for partial backup. */
static
void
xb_filters_free()
{
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

/*********************************************************************//**
Creates or opens the log files and closes them.
@return	DB_SUCCESS or error code */
static
ulint
open_or_create_log_file(
/*====================*/
	ibool	create_new_db,		/*!< in: TRUE if we should create a
					new database */
	ibool*	log_file_created,	/*!< out: TRUE if new log file
					created */
	ibool	log_file_has_been_opened,/*!< in: TRUE if a log file has been
					opened before: then it is an error
					to try to create another log file */
	ulint	k,			/*!< in: log group number */
	ulint	i,			/*!< in: log file number in group */
	fil_space_t**	log_space)	/*!< out: log space */
{
	bool		ret;
	os_offset_t	size;
	char		name[10000];
	ulint		dirnamelen;

	UT_NOT_USED(create_new_db);
	UT_NOT_USED(log_file_has_been_opened);
	UT_NOT_USED(k);
	ut_ad(k == 0);

	*log_file_created = FALSE;

	os_normalize_path(srv_log_group_home_dir);

	dirnamelen = strlen(srv_log_group_home_dir);
	ut_a(dirnamelen < (sizeof name) - 10 - sizeof "ib_logfile");
	memcpy(name, srv_log_group_home_dir, dirnamelen);

	/* Add a path separator if needed. */
	if (dirnamelen && name[dirnamelen - 1] != OS_PATH_SEPARATOR) {
		name[dirnamelen++] = OS_PATH_SEPARATOR;
	}

	sprintf(name + dirnamelen, "%s%lu", "ib_logfile", (ulong) i);

	files[i] = os_file_create(0, name,
				  OS_FILE_OPEN, OS_FILE_NORMAL,
				  OS_LOG_FILE, true, &ret);
	if (!ret) {
		fprintf(stderr, "InnoDB: Error in opening %s\n", name);

		return(DB_ERROR);
	}

	size = os_file_get_size(files[i]);

	if (size != srv_log_file_size * UNIV_PAGE_SIZE) {

		fprintf(stderr,
			"InnoDB: Error: log file %s is"
			" of different size " UINT64PF " bytes\n"
			"InnoDB: than specified in the .cnf"
			" file " UINT64PF " bytes!\n",
			name, size, srv_log_file_size * UNIV_PAGE_SIZE);

		return(DB_ERROR);
	}

	ret = os_file_close(files[i]);
	ut_a(ret);

	if (i == 0) {
		/* Create in memory the file space object
		which is for this log group */

		*log_space = fil_space_create(
			name, 2 * k + SRV_LOG_SPACE_FIRST_ID,
			fsp_flags_set_page_size(0, univ_page_size),
			FIL_TYPE_LOG);
	}

	ut_a(*log_space != NULL);
	ut_a(fil_validate());

	ut_a(fil_node_create(name, srv_log_file_size, *log_space,
			     false, false) != NULL);
	if (i == 0) {
		ut_a(log_group_init(k, srv_n_log_files,
				    srv_log_file_size * UNIV_PAGE_SIZE,
				    2 * k + SRV_LOG_SPACE_FIRST_ID));
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Normalizes init parameter values to use units we use inside InnoDB.
@return	DB_SUCCESS or error code */
static
void
xb_normalize_init_values(void)
/*==========================*/
{
	srv_sys_space.normalize();

	srv_tmp_space.normalize();

	srv_log_file_size /= UNIV_PAGE_SIZE;

	srv_log_buffer_size /= UNIV_PAGE_SIZE;

	srv_lock_table_size = 5 * (srv_buf_pool_size / UNIV_PAGE_SIZE);
}

/***********************************************************************
Set the open files limit. Based on set_max_open_files().

@return the resulting open files limit. May be less or more than the requested
value.  */
static uint
xb_set_max_open_files(
/*==================*/
	uint max_file_limit)	/*!<in: open files limit */
{
#if defined(RLIMIT_NOFILE)
	struct rlimit rlimit;
	uint old_cur;

	if (getrlimit(RLIMIT_NOFILE, &rlimit)) {

		goto end;
	}

	old_cur = (uint) rlimit.rlim_cur;

	if (rlimit.rlim_cur == RLIM_INFINITY) {

		rlimit.rlim_cur = max_file_limit;
	}

	if (rlimit.rlim_cur >= max_file_limit) {

		max_file_limit = rlimit.rlim_cur;
		goto end;
	}

	rlimit.rlim_cur = rlimit.rlim_max = max_file_limit;

	if (setrlimit(RLIMIT_NOFILE, &rlimit)) {

		max_file_limit = old_cur;	/* Use original value */
	} else {

		rlimit.rlim_cur = 0;	/* Safety if next call fails */

		(void) getrlimit(RLIMIT_NOFILE, &rlimit);

		if (rlimit.rlim_cur) {

			/* If call didn't fail */
			max_file_limit = (uint) rlimit.rlim_cur;
		}
	}

end:
	return(max_file_limit);
#else
	return(0);
#endif
}

/**************************************************************************
Prints a warning for every table that uses unsupported engine and
hence will not be backed up. */
static void
xb_tables_compatibility_check()
{
	const char* query = "SELECT\n"
			    "  CONCAT(table_schema, '/', table_name), engine\n"
			    "FROM information_schema.tables\n"
			    "WHERE engine NOT IN (\n"
			    "  'MyISAM', 'InnoDB', 'CSV', 'MRG_MYISAM'\n"
			    ")\n"
			    "AND table_schema NOT IN (\n"
			    "  'performance_schema', 'information_schema',"
			    "  'mysql'\n"
			    ");";

	MYSQL_RES* result = xb_mysql_query(mysql_connection, query, true, true);
	MYSQL_ROW row;
	if (!result) {
		return;
	}

	ut_a(mysql_num_fields(result) == 2);
	while ((row = mysql_fetch_row(result))) {
		if (!check_if_skip_table(row[0])) {
			*strchr(row[0], '/') = '.';
			msg("Warning: \"%s\" uses engine \"%s\" "
			    "and will not be backed up.\n", row[0], row[1]);
		}
	}

	mysql_free_result(result);
}

void
xtrabackup_backup_func(void)
{
	MY_STAT			 stat_info;
	lsn_t			 latest_cp;
	uint			 i;
	uint			 count;
	ib_mutex_t		 count_mutex;
	data_thread_ctxt_t 	*data_threads;

	recv_is_making_a_backup = true;

#ifdef USE_POSIX_FADVISE
	msg("xtrabackup: uses posix_fadvise().\n");
#endif

	/* cd to datadir */

	if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
	{
		msg("xtrabackup: cannot my_setwd %s\n", mysql_real_data_home);
		exit(EXIT_FAILURE);
	}
	msg("xtrabackup: cd to %s\n", mysql_real_data_home);

	msg("xtrabackup: open files limit requested %u, set to %u\n",
	    (uint) xb_open_files_limit,
	    xb_set_max_open_files(xb_open_files_limit));

	mysql_data_home= mysql_data_home_buff;
	mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
	mysql_data_home[1]=0;

	srv_read_only_mode = TRUE;

	srv_backup_mode = TRUE;
	/* We can safely close files if we don't allow DDL during the
	backup */
	srv_close_files = xb_close_files || opt_lock_ddl;

	if (xb_close_files)
		msg("xtrabackup: warning: close-files specified. Use it "
		    "at your own risk. If there are DDL operations like table DROP TABLE "
		    "or RENAME TABLE during the backup, inconsistent backup will be "
		    "produced.\n");

	/* initialize components */
        if(innodb_init_param())
                exit(EXIT_FAILURE);

	xb_normalize_init_values();

#ifndef __WIN__
	if (srv_file_flush_method_str == NULL) {
        	/* These are the default options */
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "fsync")) {
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DSYNC")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DIRECT")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DIRECT;
		msg("xtrabackup: using O_DIRECT\n");
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "littlesync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_LITTLESYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "nosync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_NOSYNC;
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "ALL_O_DIRECT")) {
		srv_unix_file_flush_method = SRV_UNIX_ALL_O_DIRECT;
		msg("xtrabackup: using ALL_O_DIRECT\n");
	} else if (0 == ut_strcmp(srv_file_flush_method_str,
				  "O_DIRECT_NO_FSYNC")) {
		srv_unix_file_flush_method = SRV_UNIX_O_DIRECT_NO_FSYNC;
		msg("xtrabackup: using O_DIRECT_NO_FSYNC\n");
	} else {
	  	msg("xtrabackup: Unrecognized value %s for "
		    "innodb_flush_method\n", srv_file_flush_method_str);
	  	exit(EXIT_FAILURE);
	}
#else /* __WIN__ */
	/* We can only use synchronous unbuffered IO on Windows for now */
	if (srv_file_flush_method_str != NULL) {
		msg("xtrabackupp: Warning: "
		    "ignoring innodb_flush_method = %s on Windows.\n");
	}

	srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
	srv_use_native_aio = FALSE;
#endif

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
		srv_max_n_threads = 1000;       /* saves several MB of memory,
                                                especially in 64-bit
                                                computers */
        }

	srv_general_init();
	ut_crc32_init();
	crc_init();

	xb_filters_init();

	if (!xb_keyring_init_for_backup(mysql_connection)) {
		msg("xtrabackup: Error: failed to init keyring plugin\n");
		exit(EXIT_FAILURE);
	}

	if (opt_tables_compatibility_check) {
		xb_tables_compatibility_check();
	}

	{
	ibool	log_file_created;
	ibool	log_created	= FALSE;
	ibool	log_opened	= FALSE;
	ulint	err;
	ulint	i;
	fil_space_t *log_space;

	xb_fil_io_init();

	log_init();

	recv_sys_create();
	recv_sys_init(buf_pool_get_curr_size());

	lock_sys_create(srv_lock_table_size);

	for (i = 0; i < srv_n_log_files; i++) {
		err = open_or_create_log_file(FALSE, &log_file_created,
					      log_opened, 0, i, &log_space);
		if (err != DB_SUCCESS) {

			//return((int) err);
			exit(EXIT_FAILURE);
		}

		if (log_file_created) {
			log_created = TRUE;
		} else {
			log_opened = TRUE;
		}
		if ((log_opened && log_created)) {
			msg(
	"xtrabackup: Error: all log files must be created at the same time.\n"
	"xtrabackup: All log files must be created also in database creation.\n"
	"xtrabackup: If you want bigger or smaller log files, shut down the\n"
	"xtrabackup: database and make sure there were no errors in shutdown.\n"
	"xtrabackup: Then delete the existing log files. Edit the .cnf file\n"
	"xtrabackup: and start the database again.\n");

			//return(DB_ERROR);
			exit(EXIT_FAILURE);
		}
	}

	/* log_file_created must not be TRUE, if online */
	if (log_file_created) {
		msg("xtrabackup: Something wrong with source files...\n");
		exit(EXIT_FAILURE);
	}

	}

	/* create extra LSN dir if it does not exist. */
	if (xtrabackup_extra_lsndir
		&&!my_stat(xtrabackup_extra_lsndir,&stat_info,MYF(0))
		&& (my_mkdir(xtrabackup_extra_lsndir,0777,MYF(0)) < 0)) {
		msg("xtrabackup: Error: cannot mkdir %d: %s\n",
		    my_errno(), xtrabackup_extra_lsndir);
		exit(EXIT_FAILURE);
	}

	/* create target dir if not exist */
	if (!my_stat(xtrabackup_target_dir,&stat_info,MYF(0))
		&& (my_mkdir(xtrabackup_target_dir,0777,MYF(0)) < 0)){
		msg("xtrabackup: Error: cannot mkdir %d: %s\n",
		    my_errno(), xtrabackup_target_dir);
		exit(EXIT_FAILURE);
	}


	if (opt_dump_innodb_buffer_pool) {
		dump_innodb_buffer_pool(mysql_connection);
	}

        {
        fil_system_t*   f_system = fil_system;

	/* definition from recv_recovery_from_checkpoint_start() */
	log_group_t*	max_cp_group;
	ulint		max_cp_field;
	byte*		buf;
	byte*		log_hdr_buf_;
	byte*		log_hdr_buf;
	ulint		err;
	bool		data_copying_error = false;

	/* start back ground thread to copy newer log */
	os_thread_id_t log_copying_thread_id;
	datafiles_iter_t *it;

	log_hdr_buf_ = static_cast<byte *>
		(ut_malloc_nokey(LOG_FILE_HDR_SIZE + UNIV_PAGE_SIZE_MAX));
	log_hdr_buf = static_cast<byte *>
		(ut_align(log_hdr_buf_, UNIV_PAGE_SIZE_MAX));

	/* get current checkpoint_lsn */
	/* Look for the latest checkpoint from any of the log groups */

	mutex_enter(&log_sys->mutex);

	err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

	if (err != DB_SUCCESS) {

		ut_free(log_hdr_buf_);
		exit(EXIT_FAILURE);
	}

	log_group_header_read(max_cp_group, max_cp_field);
	buf = log_sys->checkpoint_buf;

	checkpoint_lsn_start = mach_read_from_8(buf + LOG_CHECKPOINT_LSN);
	checkpoint_no_start = mach_read_from_8(buf + LOG_CHECKPOINT_NO);

	mutex_exit(&log_sys->mutex);

reread_log_header:
	fil_io(IORequest(IORequest::READ), true,
	       page_id_t(max_cp_group->space_id, 0),
	       univ_page_size,
	       0, LOG_FILE_HDR_SIZE,
	       log_hdr_buf, max_cp_group);

	/* check consistency of log file header to copy */
	mutex_enter(&log_sys->mutex);

	err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

        if (err != DB_SUCCESS) {

		ut_free(log_hdr_buf_);
                exit(EXIT_FAILURE);
        }

        log_group_header_read(max_cp_group, max_cp_field);
        buf = log_sys->checkpoint_buf;

	if(checkpoint_no_start != mach_read_from_8(buf + LOG_CHECKPOINT_NO)) {

		checkpoint_lsn_start = mach_read_from_8(buf + LOG_CHECKPOINT_LSN);
		checkpoint_no_start = mach_read_from_8(buf + LOG_CHECKPOINT_NO);
		mutex_exit(&log_sys->mutex);
		goto reread_log_header;
	}

	mutex_exit(&log_sys->mutex);

	xtrabackup_init_datasinks();

	if (!select_history()) {
		exit(EXIT_FAILURE);
	}

	/* open the log file */
	memset(&stat_info, 0, sizeof(MY_STAT));
	dst_log_file = ds_open(ds_redo, XB_LOG_FILENAME, &stat_info);
	if (dst_log_file == NULL) {
		msg("xtrabackup: error: failed to open the target stream for "
		    "'%s'.\n", XB_LOG_FILENAME);
		ut_free(log_hdr_buf_);
		exit(EXIT_FAILURE);
	}

	/* label it */
	strcpy((char*) log_hdr_buf + LOG_HEADER_CREATOR, "xtrabkup ");
	ut_sprintf_timestamp(
		(char*) log_hdr_buf + (LOG_HEADER_CREATOR
				+ (sizeof "xtrabkup ") - 1));

	if (ds_write(dst_log_file, log_hdr_buf, LOG_FILE_HDR_SIZE)) {
		msg("xtrabackup: error: write to logfile failed\n");
		ut_free(log_hdr_buf_);
		exit(EXIT_FAILURE);
	}

	ut_free(log_hdr_buf_);

	/* start flag */
	log_copying = TRUE;

	/* start io throttle */
	if(xtrabackup_throttle) {
		os_thread_id_t io_watching_thread_id;

		io_ticket = xtrabackup_throttle;
		wait_throttle = os_event_create("wait_throttle");

		os_thread_create(io_watching_thread, NULL,
				 &io_watching_thread_id);
	}

	mutex_enter(&log_sys->mutex);
	xtrabackup_choose_lsn_offset(checkpoint_lsn_start);
	mutex_exit(&log_sys->mutex);

	/* copy log file by current position */
	if(xtrabackup_copy_logfile(checkpoint_lsn_start, FALSE))
		exit(EXIT_FAILURE);


	log_copying_stop = os_event_create("log_copying_stop");
	os_thread_create(log_copying_thread, NULL, &log_copying_thread_id);

	/* Populate fil_system with tablespaces to copy */
	err = xb_load_tablespaces();
	if (err != DB_SUCCESS) {
		msg("xtrabackup: error: xb_load_tablespaces() failed with"
		    "error code %lu\n", err);
		exit(EXIT_FAILURE);
	}

	/* FLUSH CHANGED_PAGE_BITMAPS call */
	if (!flush_changed_page_bitmaps()) {
		exit(EXIT_FAILURE);
	}
	debug_sync_point("xtrabackup_suspend_at_start");

	if (xtrabackup_incremental) {
		if (!xtrabackup_incremental_force_scan &&
		    have_changed_page_bitmaps) {
			changed_page_bitmap = xb_page_bitmap_init();
		}
		if (!changed_page_bitmap) {
			msg("xtrabackup: using the full scan for incremental "
			    "backup\n");
		} else if (incremental_lsn != checkpoint_lsn_start) {
			/* Do not print that bitmaps are used when dummy bitmap
			is build for an empty LSN range. */
			msg("xtrabackup: using the changed page bitmap\n");
		}
	}

	ut_a(xtrabackup_parallel > 0);

	if (xtrabackup_parallel > 1) {
		msg("xtrabackup: Starting %u threads for parallel data "
		    "files transfer\n", xtrabackup_parallel);
	}

	if (opt_lock_ddl_per_table) {
		mdl_lock_init();
	}

	it = datafiles_iter_new(f_system);
	if (it == NULL) {
		msg("xtrabackup: Error: datafiles_iter_new() failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Create data copying threads */
	data_threads = (data_thread_ctxt_t *)
		ut_malloc_nokey(sizeof(data_thread_ctxt_t) *
                                xtrabackup_parallel);
	count = xtrabackup_parallel;
	mutex_create(LATCH_ID_XTRA_COUNT_MUTEX, &count_mutex);

	for (i = 0; i < (uint) xtrabackup_parallel; i++) {
		data_threads[i].it = it;
		data_threads[i].num = i+1;
		data_threads[i].count = &count;
		data_threads[i].count_mutex = &count_mutex;
		data_threads[i].error = &data_copying_error;
		os_thread_create(data_copy_thread_func, data_threads + i,
				 &data_threads[i].id);
	}

	/* Wait for threads to exit */
	while (1) {
		os_thread_sleep(1000000);
		mutex_enter(&count_mutex);
		if (count == 0) {
			mutex_exit(&count_mutex);
			break;
		}
		mutex_exit(&count_mutex);
	}

	mutex_free(&count_mutex);
	ut_free(data_threads);
	datafiles_iter_free(it);

	if (data_copying_error) {
		exit(EXIT_FAILURE);
	}

	if (changed_page_bitmap) {
		xb_page_bitmap_deinit(changed_page_bitmap);
	}
	}

	if (!backup_start()) {
		exit(EXIT_FAILURE);
	}

	/* read the latest checkpoint lsn */
	latest_cp = 0;
	{
		log_group_t*	max_cp_group;
		ulint	max_cp_field;
		ulint	err;

		mutex_enter(&log_sys->mutex);

		err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

		if (err != DB_SUCCESS) {
			msg("xtrabackup: Error: recv_find_max_checkpoint() failed.\n");
			mutex_exit(&log_sys->mutex);
			goto skip_last_cp;
		}

		log_group_header_read(max_cp_group, max_cp_field);

		xtrabackup_choose_lsn_offset(checkpoint_lsn_start);

		latest_cp = mach_read_from_8(log_sys->checkpoint_buf +
					     LOG_CHECKPOINT_LSN);

		mutex_exit(&log_sys->mutex);

		msg("xtrabackup: The latest check point (for incremental): "
		    "'" LSN_PF "'\n", latest_cp);
	}
skip_last_cp:
	/* stop log_copying_thread */
	log_copying = FALSE;
	os_event_set(log_copying_stop);
	msg("xtrabackup: Stopping log copying thread.\n");
	while (log_copying_running) {
		msg(".");
		os_thread_sleep(200000); /*0.2 sec*/
	}
	msg("\n");

	os_event_destroy(log_copying_stop);
	if (ds_close(dst_log_file)) {
		exit(EXIT_FAILURE);
	}

	if(!xtrabackup_incremental) {
		strcpy(metadata_type, "full-backuped");
		metadata_from_lsn = 0;
	} else {
		strcpy(metadata_type, "incremental");
		metadata_from_lsn = incremental_lsn;
	}
	metadata_to_lsn = latest_cp;
	metadata_last_lsn = log_copy_scanned_lsn;

	if (!xtrabackup_stream_metadata(ds_meta)) {
		msg("xtrabackup: Error: failed to stream metadata.\n");
		exit(EXIT_FAILURE);
	}

	if (!backup_finish()) {
		exit(EXIT_FAILURE);
	}

	if (xtrabackup_extra_lsndir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_extra_lsndir,
			XTRABACKUP_METADATA_FILENAME);
		if (!xtrabackup_write_metadata(filename)) {
			msg("xtrabackup: Error: failed to write metadata "
			    "to '%s'.\n", filename);
			exit(EXIT_FAILURE);
		}

		sprintf(filename, "%s/%s", xtrabackup_extra_lsndir,
			XTRABACKUP_INFO);
		if (!xtrabackup_write_info(filename)) {
			msg("xtrabackup: Error: failed to write info "
			    "to '%s'.\n", filename);
			exit(EXIT_FAILURE);
		}

	}

	if (opt_lock_ddl_per_table) {
		mdl_unlock_all();
	}

	if (opt_transition_key != NULL || opt_generate_transition_key) {
		if (!xb_tablespace_keys_dump(ds_data, opt_transition_key,
                    opt_transition_key != NULL ?
                    	strlen(opt_transition_key) : 0)) {
			msg("xtrabackup: Error: failed to dump "
			    "tablespace keys.\n");
			exit(EXIT_FAILURE);
		}
	}

	xtrabackup_destroy_datasinks();

	if (wait_throttle) {
		/* wait for io_watching_thread completion */
		while (io_watching_thread_running) {
			os_thread_sleep(1000000);
		}
		os_event_destroy(wait_throttle);
		wait_throttle = NULL;
	}

	msg("xtrabackup: Transaction log of lsn (" LSN_PF ") to (" LSN_PF
	    ") was copied.\n", checkpoint_lsn_start, log_copy_scanned_lsn);
	xb_filters_free();

	xb_data_files_close();

	recv_sys_debug_free();

	log_shutdown();

	trx_pool_close();

	lock_sys_close();

	os_thread_free();

	row_mysql_close();

	sync_check_close();

	xb_keyring_shutdown();

	/* Make sure that the latest checkpoint made it to xtrabackup_logfile */
	if (latest_cp > log_copy_scanned_lsn) {
		msg("xtrabackup: error: last checkpoint LSN (" LSN_PF
		    ") is larger than last copied LSN (" LSN_PF ").\n",
		    latest_cp, log_copy_scanned_lsn);
		exit(EXIT_FAILURE);
	}
}

/* ================= stats ================= */
static my_bool
xtrabackup_stats_level(
	dict_index_t*	index,
	ulint		level)
{
	ulint	space;
	page_t*	page;

	rec_t*	node_ptr;

	ulint	right_page_no;

	page_cur_t	cursor;

	mtr_t	mtr;
	mem_heap_t*	heap	= mem_heap_create(256);

	ulint*	offsets = NULL;

	ulonglong n_pages, n_pages_extern;
	ulonglong sum_data, sum_data_extern;
	ulonglong n_recs;
	buf_block_t*	block;
	page_size_t	page_size(0, 0, false);
	bool		found;

	n_pages = sum_data = n_recs = 0;
	n_pages_extern = sum_data_extern = 0;


	if (level == 0)
		fprintf(stdout, "        leaf pages: ");
	else
		fprintf(stdout, "     level %lu pages: ", level);

	mtr_start(&mtr);

	mtr_x_lock(&(index->lock), &mtr);
	block = btr_root_block_get(index, RW_X_LATCH, &mtr);
	page = buf_block_get_frame(block);

	space = page_get_space_id(page);
	page_size.copy_from(fil_space_get_page_size(space, &found));

	ut_a(found);

	while (level != btr_page_get_level(page, &mtr)) {

		ut_a(space == block->page.id.space());
		ut_a(space == page_get_space_id(page));
		ut_a(!page_is_leaf(page));

		page_cur_set_before_first(block, &cursor);
		page_cur_move_to_next(&cursor);

		node_ptr = page_cur_get_rec(&cursor);
		offsets = rec_get_offsets(node_ptr, index, offsets,
					ULINT_UNDEFINED, &heap);
		block = btr_node_ptr_get_child(node_ptr, index, offsets, &mtr);
		page = buf_block_get_frame(block);
	}

loop:
	mem_heap_empty(heap);
	offsets = NULL;
	mtr_x_lock(&(index->lock), &mtr);

	right_page_no = btr_page_get_next(page, &mtr);


	/*=================================*/
	//fprintf(stdout, "%lu ", (ulint) buf_frame_get_page_no(page));

	n_pages++;
	sum_data += page_get_data_size(page);
	n_recs += page_get_n_recs(page);


	if (level == 0) {
		page_cur_t	cur;
		ulint	n_fields;
		ulint	i;
		mem_heap_t*	local_heap	= NULL;
		ulint	offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*	local_offsets	= offsets_;

		*offsets_ = (sizeof offsets_) / sizeof *offsets_;

		page_cur_set_before_first(block, &cur);
		page_cur_move_to_next(&cur);

		for (;;) {
			if (page_cur_is_after_last(&cur)) {
				break;
			}

			local_offsets = rec_get_offsets(cur.rec, index, local_offsets,
						ULINT_UNDEFINED, &local_heap);
			n_fields = rec_offs_n_fields(local_offsets);

			for (i = 0; i < n_fields; i++) {
				if (rec_offs_nth_extern(local_offsets, i)) {
					page_t*	local_page;
					ulint	space_id;
					ulint	page_no;
					ulint	offset;
					byte*	blob_header;
					ulint	part_len;
					mtr_t	local_mtr;
					ulint	local_len;
					byte*	data;
					buf_block_t*	local_block;

					data = rec_get_nth_field(cur.rec, local_offsets, i, &local_len);

					ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);
					local_len -= BTR_EXTERN_FIELD_REF_SIZE;

					space_id = mach_read_from_4(data + local_len + BTR_EXTERN_SPACE_ID);
					page_no = mach_read_from_4(data + local_len + BTR_EXTERN_PAGE_NO);
					offset = mach_read_from_4(data + local_len + BTR_EXTERN_OFFSET);

					if (offset != FIL_PAGE_DATA)
						msg("\nWarning: several record may share same external page.\n");

					for (;;) {
						mtr_start(&local_mtr);

						local_block = btr_block_get(page_id_t(space_id, page_no), page_size, RW_S_LATCH, index, &local_mtr);
						local_page = buf_block_get_frame(local_block);
						blob_header = local_page + offset;
#define BTR_BLOB_HDR_PART_LEN		0
#define BTR_BLOB_HDR_NEXT_PAGE_NO	4
						//part_len = btr_blob_get_part_len(blob_header);
						part_len = mach_read_from_4(blob_header + BTR_BLOB_HDR_PART_LEN);

						//page_no = btr_blob_get_next_page_no(blob_header);
						page_no = mach_read_from_4(blob_header + BTR_BLOB_HDR_NEXT_PAGE_NO);

						offset = FIL_PAGE_DATA;




						/*=================================*/
						//fprintf(stdout, "[%lu] ", (ulint) buf_frame_get_page_no(page));

						n_pages_extern++;
						sum_data_extern += part_len;


						mtr_commit(&local_mtr);

						if (page_no == FIL_NULL)
							break;
					}
				}
			}

			page_cur_move_to_next(&cur);
		}
	}




	mtr_commit(&mtr);
	if (right_page_no != FIL_NULL) {
		mtr_start(&mtr);
		block = btr_block_get(page_id_t(space,
						dict_index_get_page(index)),
				      page_size,
				      RW_X_LATCH, index, &mtr);
		page = buf_block_get_frame(block);
		goto loop;
	}
	mem_heap_free(heap);

	if (level == 0)
		fprintf(stdout, "recs=%llu, ", n_recs);

	fprintf(stdout, "pages=%llu, data=%llu bytes, data/pages=%lld%%",
		n_pages, sum_data,
		((sum_data * 100) / page_size.physical()) / n_pages);


	if (level == 0 && n_pages_extern) {
		putc('\n', stdout);
		/* also scan blob pages*/
		fprintf(stdout, "    external pages: ");

		fprintf(stdout, "pages=%llu, data=%llu bytes, data/pages=%lld%%",
			n_pages_extern, sum_data_extern,
			((sum_data_extern * 100) / page_size.physical())
				/ n_pages_extern);
	}

	putc('\n', stdout);

	if (level > 0) {
		xtrabackup_stats_level(index, level - 1);
	}

	return(TRUE);
}

static void
xtrabackup_stats_func(int argc, char **argv)
{
	ulint n;

	/* cd to datadir */

	if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
	{
		msg("xtrabackup: cannot my_setwd %s\n", mysql_real_data_home);
		exit(EXIT_FAILURE);
	}
	msg("xtrabackup: cd to %s\n", mysql_real_data_home);

	mysql_data_home= mysql_data_home_buff;
	mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
	mysql_data_home[1]=0;

	/* set read only */
	srv_read_only_mode = TRUE;

	if (!xb_keyring_init_for_stats(argc, argv)) {
		msg("xtrabackup: error: failed to init keyring plugin.\n");
		exit(EXIT_FAILURE);
	}

	/* initialize components */
	if(innodb_init_param()) {
		exit(EXIT_FAILURE);
	}

	/* Check if the log files have been created, otherwise innodb_init()
	will crash when called with srv_read_only == TRUE */
	for (n = 0; n < srv_n_log_files; n++) {
		char		logname[FN_REFLEN];
		bool		exists;
		os_file_type_t	type;

		snprintf(logname, sizeof(logname), "%s%c%s%lu",
			srv_log_group_home_dir, OS_PATH_SEPARATOR,
			"ib_logfile", (ulong) n);
		os_normalize_path(logname);

		if (!os_file_status(logname, &exists, &type) || !exists ||
		    type != OS_FILE_TYPE_FILE) {
			msg("xtrabackup: Error: "
			    "Cannot find log file %s.\n", logname);
			msg("xtrabackup: Error: "
			    "to use the statistics feature, you need a "
			    "clean copy of the database including "
			    "correctly sized log files, so you need to "
			    "execute with --prepare twice to use this "
			    "functionality on a backup.\n");
			exit(EXIT_FAILURE);
		}
	}

	msg("xtrabackup: Starting 'read-only' InnoDB instance to gather "
	    "index statistics.\n"
	    "xtrabackup: Using %lld bytes for buffer pool (set by "
	    "--use-memory parameter)\n", xtrabackup_use_memory);

	if(innodb_init())
		exit(EXIT_FAILURE);

	xb_filters_init();

	fprintf(stdout, "\n\n<INDEX STATISTICS>\n");

	/* gather stats */

	{
	dict_table_t*	sys_tables;
	dict_index_t*	sys_index;
	dict_table_t*	table;
	btr_pcur_t	pcur;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	mtr_t		mtr;

	/* Enlarge the fatal semaphore wait timeout during the InnoDB table
	monitor printout */

	os_atomic_increment_ulint(
		&srv_fatal_semaphore_wait_threshold,
		72000);

	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);

	btr_pcur_open_at_index_side(TRUE, sys_index, BTR_SEARCH_LEAF, &pcur,
				    TRUE, 0, &mtr);
loop:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur))
	{
		/* end of index */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);

		mutex_exit(&(dict_sys->mutex));

		/* Restore the fatal semaphore wait timeout */
		os_atomic_increment_ulint(
			&srv_fatal_semaphore_wait_threshold,
			-72000);

		goto end;
	}

	field = rec_get_nth_field_old(rec, 0, &len);

	if (!rec_get_deleted_flag(rec, 0)) {

		/* We found one */

                char*	table_name = mem_strdupl((char*) field, len);

		btr_pcur_store_position(&pcur, &mtr);

		mtr_commit(&mtr);

		table = dict_table_get_low(table_name);
		ut_free(table_name);

		if (table && check_if_skip_table(table->name.m_name))
			goto skip;


		if (table == NULL) {
			fputs("InnoDB: Failed to load table ", stderr);
			ut_print_name(stderr, NULL, (char*) field);
			putc('\n', stderr);
		} else {
			dict_index_t*	index;

			/* The table definition was corrupt if there
			is no index */

			if (dict_table_get_first_index(table)) {
				dict_stats_update_transient(table);
			}

			//dict_table_print_low(table);

			index = UT_LIST_GET_FIRST(table->indexes);
			while (index != NULL) {
{
	ib_uint64_t	n_vals;
	bool		found;

	if (index->n_user_defined_cols > 0) {
		n_vals = index->stat_n_diff_key_vals[
					index->n_user_defined_cols];
	} else {
		n_vals = index->stat_n_diff_key_vals[1];
	}

	fprintf(stdout,
		"  table: %s, index: %s, space id: %lu, root page: %lu"
		", zip size: %lu"
		"\n  estimated statistics in dictionary:\n"
		"    key vals: %lu, leaf pages: %lu, size pages: %lu\n"
		"  real statistics:\n",
		table->name.m_name, index->name(),
		(ulong) index->space,
		(ulong) index->page,
		(ulong) fil_space_get_page_size(index->space, &found).physical(),
		(ulong) n_vals,
		(ulong) index->stat_n_leaf_pages,
		(ulong) index->stat_index_size);

	{
		mtr_t	local_mtr;
		page_t*	root;
		ulint	page_level;

		mtr_start(&local_mtr);

		mtr_x_lock(&(index->lock), &local_mtr);
		root = btr_root_get(index, &local_mtr);
		page_level = btr_page_get_level(root, &local_mtr);

		xtrabackup_stats_level(index, page_level);

		mtr_commit(&local_mtr);
	}

	putc('\n', stdout);
}
				index = UT_LIST_GET_NEXT(indexes, index);
			}
		}

skip:
		mtr_start(&mtr);

		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
	}

	goto loop;
	}

end:
	putc('\n', stdout);

	fflush(stdout);

	xb_filters_free();

	/* shutdown InnoDB */
	if(innodb_end())
		exit(EXIT_FAILURE);

	xb_keyring_shutdown();
}

/* ================= prepare ================= */

static
bool
check_log_header_checksum_0(
/*========================*/
	const byte*	buf)	/*!< in: buffer containing checkpoint info */
{
	/** Offset of the first checkpoint checksum */
	static const uint CHECKSUM_1 = 288;
	/** Offset of the second checkpoint checksum */
	static const uint CHECKSUM_2 = CHECKSUM_1 + 4;

	if (static_cast<uint32_t>(ut_fold_binary(buf, CHECKSUM_1))
	    != mach_read_from_4(buf + CHECKSUM_1)
	    || static_cast<uint32_t>(
		    ut_fold_binary(buf + LOG_CHECKPOINT_LSN,
				   CHECKSUM_2 - LOG_CHECKPOINT_LSN))
	    != mach_read_from_4(buf + CHECKSUM_2)) {
		return(false);
	}

	return(true);
}

static
void
update_log_temp_checkpoint_0(
	byte*	buf,
	lsn_t	lsn)
{
	/** Offset of the first checkpoint checksum */
	static const uint CHECKSUM_1 = 288;
	/** Offset of the second checkpoint checksum */
	static const uint CHECKSUM_2 = CHECKSUM_1 + 4;
	/** Most significant bits of the checkpoint offset */
	static const uint OFFSET_HIGH32 = CHECKSUM_2 + 12;
	/** Least significant bits of the checkpoint offset */
	static const uint OFFSET_LOW32 = 16;

	ulint		fold;

	mach_write_to_8(buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN, lsn);
	mach_write_to_4(buf + LOG_CHECKPOINT_1 + OFFSET_LOW32,
			LOG_FILE_HDR_SIZE +
			(lsn -
			 ut_uint64_align_down(lsn, OS_FILE_LOG_BLOCK_SIZE)));
	mach_write_to_4(buf + LOG_CHECKPOINT_1 + OFFSET_HIGH32, 0);
	fold = ut_fold_binary(buf + LOG_CHECKPOINT_1, CHECKSUM_1);
	mach_write_to_4(buf + LOG_CHECKPOINT_1 + CHECKSUM_1, fold);

	fold = ut_fold_binary(buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN,
		CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(buf + LOG_CHECKPOINT_1 + CHECKSUM_2, fold);

	mach_write_to_8(buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN, lsn);
	mach_write_to_4(buf + LOG_CHECKPOINT_2 + OFFSET_LOW32,
			LOG_FILE_HDR_SIZE +
			(lsn -
			 ut_uint64_align_down(lsn,
					      OS_FILE_LOG_BLOCK_SIZE)));
	mach_write_to_4(buf + LOG_CHECKPOINT_2
			    + LOG_CHECKPOINT_OFFSET_HIGH32, 0);
        fold = ut_fold_binary(buf + LOG_CHECKPOINT_2, CHECKSUM_1);
        mach_write_to_4(buf + LOG_CHECKPOINT_2 + CHECKSUM_1, fold);

        fold = ut_fold_binary(buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN,
			      CHECKSUM_2 - LOG_CHECKPOINT_LSN);
        mach_write_to_4(buf + LOG_CHECKPOINT_2 + CHECKSUM_2, fold);
}

static
void
update_log_temp_checkpoint(
	byte*	buf,
	lsn_t	lsn)
{
	if (redo_log_version == REDO_LOG_V0) {
		update_log_temp_checkpoint_0(buf, lsn);
		return;
	}

	/* Overwrite the both checkpoint area. */

	lsn_t lsn_offset;

	lsn_offset = LOG_FILE_HDR_SIZE + (lsn -
			ut_uint64_align_down(lsn, OS_FILE_LOG_BLOCK_SIZE));

	mach_write_to_8(buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN,
			lsn);
	mach_write_to_8(buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_OFFSET,
			lsn_offset);

	mach_write_to_8(buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN,
			lsn);
	mach_write_to_8(buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_OFFSET,
			lsn_offset);

	log_block_set_checksum(buf, log_block_calc_checksum_crc32(buf));
	log_block_set_checksum(buf + LOG_CHECKPOINT_1,
		log_block_calc_checksum_crc32(buf + LOG_CHECKPOINT_1));
	log_block_set_checksum(buf + LOG_CHECKPOINT_2,
		log_block_calc_checksum_crc32(buf + LOG_CHECKPOINT_2));
}

static my_bool
xtrabackup_init_temp_log(void)
{
	pfs_os_file_t	src_file = XB_FILE_UNDEFINED;
	char		src_path[FN_REFLEN];
	char		dst_path[FN_REFLEN];
	bool		success;

	ulint		field;
	byte		*log_buf;

	ib_uint64_t	file_size;

	lsn_t		max_no;
	lsn_t		max_lsn;
	lsn_t		checkpoint_no;

	bool		checkpoint_found;

	IORequest	read_request(IORequest::READ);
	IORequest	write_request(IORequest::WRITE);

	max_no = 0;

	log_buf = static_cast<byte*>(ut_malloc_nokey(UNIV_PAGE_SIZE_MAX * 128));
	if (log_buf == NULL) {
		goto error;
	}

	if (!xb_init_log_block_size()) {
		goto error;
	}

	if(!xtrabackup_incremental_dir) {
		sprintf(dst_path, "%s/ib_logfile0", xtrabackup_target_dir);
		sprintf(src_path, "%s/%s", xtrabackup_target_dir,
			XB_LOG_FILENAME);
	} else {
		sprintf(dst_path, "%s/ib_logfile0", xtrabackup_incremental_dir);
		sprintf(src_path, "%s/%s", xtrabackup_incremental_dir,
			XB_LOG_FILENAME);
	}

	os_normalize_path(dst_path);
	os_normalize_path(src_path);
retry:
	src_file = os_file_create_simple_no_error_handling(0, src_path,
							   OS_FILE_OPEN,
							   OS_FILE_READ_WRITE,
							   srv_read_only_mode,
							   &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		msg("xtrabackup: Warning: cannot open %s. will try to find.\n",
		    src_path);

		/* check if ib_logfile0 may be xtrabackup_logfile */
		src_file = os_file_create_simple_no_error_handling(0, dst_path,
								   OS_FILE_OPEN,
								   OS_FILE_READ_WRITE,
								   srv_read_only_mode,
								   &success);
		if (!success) {
			os_file_get_last_error(TRUE);
			msg("  xtrabackup: Fatal error: cannot find %s.\n",
			    src_path);

			goto error;
		}

		success = os_file_read(read_request, src_file, log_buf, 0,
				       LOG_FILE_HDR_SIZE);
		if (!success) {
			goto error;
		}

		if (ut_memcmp(log_buf + LOG_HEADER_CREATOR, (byte*)"xtrabkup",
			      (sizeof "xtrabkup") - 1) == 0) {
			msg("  xtrabackup: 'ib_logfile0' seems to be "
			    "'xtrabackup_logfile'. will retry.\n");

			os_file_close(src_file);
			src_file = XB_FILE_UNDEFINED;

			/* rename and try again */
			success = os_file_rename(0, dst_path, src_path);
			if (!success) {
				goto error;
			}

			goto retry;
		}

		msg("  xtrabackup: Fatal error: cannot find %s.\n",
		src_path);

		os_file_close(src_file);
		src_file = XB_FILE_UNDEFINED;

		goto error;
	}

	file_size = os_file_get_size(src_file);


	/* TODO: We should skip the following modifies, if it is not the first time. */

	/* read log file header */
	success = os_file_read(read_request, src_file, log_buf, 0,
			       LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	if (ut_memcmp(log_buf + LOG_HEADER_CREATOR, (byte *)"xtrabkup",
		      (sizeof "xtrabkup") - 1) != 0) {
		if (xtrabackup_incremental_dir) {
			msg("xtrabackup: error: xtrabackup_logfile was already used "
			    "to '--prepare'.\n");
			goto error;
		}
		msg("xtrabackup: notice: xtrabackup_logfile was already used "
		    "to '--prepare'.\n");
		goto skip_modify;
	}

	checkpoint_found = false;

	/* read last checkpoint lsn */
	for (field = LOG_CHECKPOINT_1; field <= LOG_CHECKPOINT_2;
			field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {

		/* InnoDB using CRC32 by default since 5.7.9+ */
		if (log_block_get_checksum(log_buf + field)
		    == log_block_calc_checksum_crc32(log_buf + field) &&
		    mach_read_from_4(log_buf + LOG_HEADER_FORMAT)
		    == LOG_HEADER_FORMAT_CURRENT) {
			redo_log_version = REDO_LOG_V1;
			if (!innodb_log_checksum_algorithm_specified) {
				srv_log_checksum_algorithm =
					SRV_CHECKSUM_ALGORITHM_CRC32;
			}
			if (!innodb_checksum_algorithm_specified) {
				srv_checksum_algorithm =
					SRV_CHECKSUM_ALGORITHM_CRC32;
			}
		} else if (check_log_header_checksum_0(log_buf + field)) {
			redo_log_version = REDO_LOG_V0;
			if (!innodb_log_checksum_algorithm_specified) {
				srv_log_checksum_algorithm =
					SRV_CHECKSUM_ALGORITHM_INNODB;
			}
			if (!innodb_checksum_algorithm_specified) {
				srv_checksum_algorithm =
					SRV_CHECKSUM_ALGORITHM_INNODB;
			}
		} else {
			goto not_consistent;
		}

		checkpoint_no = mach_read_from_8(log_buf + field +
						 LOG_CHECKPOINT_NO);

		if (checkpoint_no >= max_no) {

			max_no = checkpoint_no;
			max_lsn = mach_read_from_8(log_buf + field +
						   LOG_CHECKPOINT_LSN);
			checkpoint_found = true;
		}
not_consistent:
		;
	}

	if (!checkpoint_found) {
		msg("xtrabackup: No valid checkpoint found.\n");
		goto error;
	}

	mach_write_to_4(log_buf + LOG_HEADER_FORMAT,
			redo_log_version == REDO_LOG_V0 ?
			0 : LOG_HEADER_FORMAT_CURRENT);
	update_log_temp_checkpoint(log_buf, max_lsn);

	success = os_file_write(write_request, src_path, src_file, log_buf, 0,
				LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	/* expand file size (9/8) and align to UNIV_PAGE_SIZE_MAX */

	if (file_size % UNIV_PAGE_SIZE_MAX) {
		ulint n = UNIV_PAGE_SIZE_MAX -
				(ulint) (file_size % UNIV_PAGE_SIZE_MAX);
		memset(log_buf, 0, UNIV_PAGE_SIZE_MAX);
		success = os_file_write(write_request, src_path, src_file,
					log_buf, file_size, n);
		if (!success) {
			goto error;
		}

		file_size = os_file_get_size(src_file);
	}

	/* TODO: We should judge whether the file is already expanded or not... */
	{
		ulint	expand;

		memset(log_buf, 0, UNIV_PAGE_SIZE_MAX * 128);
		expand = (ulint) (file_size / UNIV_PAGE_SIZE_MAX / 8);

		for (; expand > 128; expand -= 128) {
			success = os_file_write(write_request, src_path,
						src_file, log_buf,
						file_size,
						UNIV_PAGE_SIZE_MAX * 128);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE_MAX * 128;
		}

		if (expand) {
			success = os_file_write(write_request, src_path,
						src_file, log_buf,
						file_size,
						expand * UNIV_PAGE_SIZE_MAX);
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
			success = os_file_write(write_request, src_path,
						src_file, log_buf,
						file_size,
						UNIV_PAGE_SIZE_MAX);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE_MAX;
		}
		file_size = os_file_get_size(src_file);
	}

	msg("xtrabackup: xtrabackup_logfile detected: size=" UINT64PF ", "
	    "start_lsn=(" LSN_PF ")\n", file_size, max_lsn);

	os_file_close(src_file);
	src_file = XB_FILE_UNDEFINED;

	/* fake InnoDB */
	innobase_log_files_in_group_save = innobase_log_files_in_group;
	srv_log_group_home_dir_save = srv_log_group_home_dir;
	innobase_log_file_size_save = innobase_log_file_size;

	srv_log_group_home_dir = NULL;
	innobase_log_file_size      = file_size;
	innobase_log_files_in_group = 1;

	srv_thread_concurrency = 0;

	/* rename 'xtrabackup_logfile' to 'ib_logfile0' */
	success = os_file_rename(0, src_path, dst_path);
	if (!success) {
		goto error;
	}
	xtrabackup_logfile_is_renamed = TRUE;

	if (log_buf != NULL) {
		ut_free(log_buf);
	}

	return(FALSE);

skip_modify:
	if (log_buf != NULL) {
		ut_free(log_buf);
	}
	os_file_close(src_file);
	src_file = XB_FILE_UNDEFINED;
	return(FALSE);

error:
	if (log_buf != NULL) {
		ut_free(log_buf);
	}
	if (src_file != XB_FILE_UNDEFINED)
		os_file_close(src_file);
	msg("xtrabackup: Error: xtrabackup_init_temp_log() failed.\n");
	return(TRUE); /*ERROR*/
}

/***********************************************************************
Generates path to the meta file path from a given path to an incremental .delta
by replacing trailing ".delta" with ".meta", or returns error if 'delta_path'
does not end with the ".delta" character sequence.
@return TRUE on success, FALSE on error. */
static
ibool
get_meta_path(
	const char	*delta_path,	/* in: path to a .delta file */
	char 		*meta_path)	/* out: path to the corresponding .meta
					file */
{
	size_t		len = strlen(delta_path);

	if (len <= 6 || strcmp(delta_path + len - 6, ".delta")) {
		return FALSE;
	}
	memcpy(meta_path, delta_path, len - 6);
	strcpy(meta_path + len - 6, XB_DELTA_INFO_SUFFIX);

	return TRUE;
}

/****************************************************************//**
Create a new tablespace on disk and return the handle to its opened
file. Code adopted from fiL_ibd_create with
the main difference that only disk file is created without updating
the InnoDB in-memory dictionary data structures.

@return TRUE on success, FALSE on error.  */
static
bool
xb_space_create_file(
/*==================*/
	const char*	path,		/*!<in: path to tablespace */
	ulint		space_id,	/*!<in: space id */
	ulint		flags __attribute__((unused)),/*!<in: tablespace
					flags */
	pfs_os_file_t*	file)		/*!<out: file handle */
{
	const ulint	size = FIL_IBD_FILE_INITIAL_SIZE;
	dberr_t		err;
	byte*		buf2;
	byte*		page;
	bool		success;

	IORequest	write_request(IORequest::WRITE);

	ut_ad(!is_system_tablespace(space_id));
	ut_ad(!srv_read_only_mode);
	ut_a(space_id < SRV_LOG_SPACE_FIRST_ID);
	ut_a(fsp_flags_is_valid(flags));

	/* Create the subdirectories in the path, if they are
	not there already. */
	err = os_file_create_subdirs_if_needed(path);
	if (err != DB_SUCCESS) {
		return(false);
	}

	*file = os_file_create(
		innodb_data_file_key, path,
		OS_FILE_CREATE | OS_FILE_ON_ERROR_NO_EXIT,
		OS_FILE_NORMAL,
		OS_DATA_FILE,
		srv_read_only_mode,
		&success);

	if (!success) {
		/* The following call will print an error message */
		ulint	error = os_file_get_last_error(true);

		ib::error() << "Cannot create file '" << path << "'";

		if (error == OS_FILE_ALREADY_EXISTS) {
			ib::error() << "The file '" << path << "'"
				" already exists though the"
				" corresponding table did not exist"
				" in the InnoDB data dictionary."
				" Have you moved InnoDB .ibd files"
				" around without using the SQL commands"
				" DISCARD TABLESPACE and IMPORT TABLESPACE,"
				" or did mysqld crash in the middle of"
				" CREATE TABLE?"
				" You can resolve the problem by removing"
				" the file '" << path
				<< "' under the 'datadir' of MySQL.";

			return(false);
		}

		return(false);
	}


#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
	if (fil_fusionio_enable_atomic_write(*file)) {

		/* This is required by FusionIO HW/Firmware */
		int	ret = posix_fallocate(file->m_file, 0, size * UNIV_PAGE_SIZE);

		if (ret != 0) {

			ib::error() <<
				"posix_fallocate(): Failed to preallocate"
				" data for file " << path
				<< ", desired size "
				<< size * UNIV_PAGE_SIZE
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

		success = os_file_set_size(
			path, *file, size * UNIV_PAGE_SIZE, srv_read_only_mode);
	}
#else
	success = os_file_set_size(
		path, *file, size * UNIV_PAGE_SIZE, srv_read_only_mode);
#endif /* !NO_FALLOCATE && UNIV_LINUX */

	if (!success) {
		os_file_close(*file);
		os_file_delete(innodb_data_file_key, path);
		return(false);
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

	buf2 = static_cast<byte*>(ut_malloc_nokey(3 * UNIV_PAGE_SIZE));
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = static_cast<byte*>(ut_align(buf2, UNIV_PAGE_SIZE));

	memset(page, '\0', UNIV_PAGE_SIZE);

	/* Add the UNIV_PAGE_SIZE to the table flags and write them to the
	tablespace header. */
	flags = fsp_flags_set_page_size(flags, univ_page_size);
	fsp_header_init_fields(page, space_id, flags);
	mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

	const page_size_t	page_size(flags);

	if (!page_size.is_compressed()) {
		buf_flush_init_for_writing(
			NULL, page, NULL, 0,
			fsp_is_checksum_disabled(space_id));
		success = os_file_write(write_request, path, *file, page, 0,
					page_size.physical());
	} else {
		page_zip_des_t	page_zip;

		page_zip_set_size(&page_zip, page_size.physical());
		page_zip.data = page + UNIV_PAGE_SIZE;
#ifdef UNIV_DEBUG
		page_zip.m_start =
#endif /* UNIV_DEBUG */
			page_zip.m_end = page_zip.m_nonempty =
			page_zip.n_blobs = 0;

		buf_flush_init_for_writing(
			NULL, page, &page_zip, 0,
			fsp_is_checksum_disabled(space_id));
		success = os_file_write(write_request, path, *file,
					page_zip.data, 0,
					page_size.physical());
	}

	ut_free(buf2);

	if (!success) {
		ib::error() << "Could not write the first page to"
			<< " tablespace '" << path << "'";
		os_file_close(*file);
		os_file_delete(innodb_data_file_key, path);
		return(false);
	}

	success = os_file_flush(*file);

	if (!success) {
		ib::error() << "File flush of tablespace '"
			<< path << "' failed";
		os_file_close(*file);
		os_file_delete(innodb_data_file_key, path);
		return(false);
	}

	return(true);
}

/***********************************************************************
Searches for matching tablespace file for given .delta file and space_id
in given directory. When matching tablespace found, renames it to match the
name of .delta file. If there was a tablespace with matching name and
mismatching ID, renames it to xtrabackup_tmp_#ID.ibd. If there was no
matching file, creates a new tablespace.
@return file handle of matched or created file */
static
pfs_os_file_t
xb_delta_open_matching_space(
	const char*	dbname,		/* in: path to destination database dir */
	const char*	name,		/* in: name of delta file (without .delta) */
	ulint		space_id,	/* in: space id of delta file */
	ulint		space_flags,	/* in: space flags of delta file */
	ulint		zip_size,	/* in: zip_size of tablespace */
	char*		real_name,	/* out: full path of destination file */
	size_t		real_name_len,	/* out: buffer size for real_name */
	bool* 		success)	/* out: indicates error. TRUE = success */
{
	char			dest_dir[FN_REFLEN];
	char			dest_space_name[FN_REFLEN];
	bool			ok;
	fil_space_t*		fil_space;
	pfs_os_file_t		file	= XB_FILE_UNDEFINED;
	xb_filter_entry_t*	table;

	*success = false;

	if (dbname) {
		snprintf(dest_dir, FN_REFLEN, "%s/%s",
			xtrabackup_target_dir, dbname);
		os_normalize_path(dest_dir);

		snprintf(dest_space_name, FN_REFLEN, "%s/%s", dbname, name);
	} else {
		snprintf(dest_dir, FN_REFLEN, "%s", xtrabackup_target_dir);
		os_normalize_path(dest_dir);

		snprintf(dest_space_name, FN_REFLEN, "%s", name);
	}

	snprintf(real_name, real_name_len,
		 "%s/%s",
		 xtrabackup_target_dir, dest_space_name);
	os_normalize_path(real_name);
	/* Truncate ".ibd" */
	dest_space_name[strlen(dest_space_name) - 4] = '\0';

	/* Create the database directory if it doesn't exist yet */
	if (!os_file_create_directory(dest_dir, FALSE)) {
		msg("xtrabackup: error: cannot create dir %s\n", dest_dir);
		return file;
	}

	if (space_id != ULINT_UNDEFINED
	    && !fil_is_user_tablespace_id(space_id)) {
		goto found;
	}

	/* remember space name for further reference */
	table = static_cast<xb_filter_entry_t *>
		(ut_malloc_nokey(sizeof(xb_filter_entry_t) +
			strlen(dest_space_name) + 1));

	table->name = ((char*)table) + sizeof(xb_filter_entry_t);
	strcpy(table->name, dest_space_name);
	HASH_INSERT(xb_filter_entry_t, name_hash, inc_dir_tables_hash,
			ut_fold_string(table->name), table);

	mutex_enter(&fil_system->mutex);
	fil_space = fil_space_get_by_name(dest_space_name);
	mutex_exit(&fil_system->mutex);

	if (fil_space != NULL) {
		if (fil_space->id == space_id || space_id == ULINT_UNDEFINED) {
			/* we found matching space */
			goto found;
		} else {

			char		tmpname[FN_REFLEN];
			char		*oldpath;
			bool		exists;
			os_file_type_t	type;

			if (dbname != NULL) {
				snprintf(tmpname, FN_REFLEN,
					 "%s/xtrabackup_tmp_#%lu",
					 dbname, fil_space->id);
			} else {
				snprintf(tmpname, FN_REFLEN,
					 "./xtrabackup_tmp_#%lu",
					 fil_space->id);
			}

			oldpath = mem_strdup(
				UT_LIST_GET_FIRST(fil_space->chain)->name);

			msg("xtrabackup: Renaming %s to %s.ibd\n",
				fil_space->name, tmpname);


			ut_a(os_file_status(oldpath, &exists, &type));

			if (exists && !fil_rename_tablespace(fil_space->id,
						   oldpath, tmpname, NULL))
			{
				msg("xtrabackup: Cannot rename %s to %s\n",
					fil_space->name, tmpname);
				ut_free(oldpath);
				goto exit;
			}
			ut_free(oldpath);
		}
	}

	if (space_id == ULINT_UNDEFINED)
	{
		msg("xtrabackup: Error: Cannot handle DDL operation on "
		    "tablespace %s\n", dest_space_name);
		exit(EXIT_FAILURE);
	}
	mutex_enter(&fil_system->mutex);
	fil_space = fil_space_get_by_id(space_id);
	mutex_exit(&fil_system->mutex);
	if (fil_space != NULL) {
		char		tmpname[FN_REFLEN];
		char		*oldpath;
		bool		exists;
		os_file_type_t	type;

		strncpy(tmpname, dest_space_name, FN_REFLEN);

		oldpath = mem_strdup(UT_LIST_GET_FIRST(fil_space->chain)->name);

		msg("xtrabackup: Renaming %s to %s\n",
		    fil_space->name, dest_space_name);

		ut_a(os_file_status(oldpath, &exists, &type));

		if (exists && !fil_rename_tablespace(fil_space->id, oldpath,
					   tmpname, NULL))
		{
			msg("xtrabackup: Cannot rename %s to %s\n",
				fil_space->name, dest_space_name);
			ut_free(oldpath);
			goto exit;
		}
		ut_free(oldpath);

		goto found;
	}

	/* No matching space found. create the new one.  */

	if (!fil_space_create(dest_space_name, space_id, space_flags,
			      FIL_TYPE_TABLESPACE)) {
		msg("xtrabackup: Cannot create tablespace %s\n",
			dest_space_name);
		goto exit;
	}

	/* Calculate correct tablespace flags for compressed tablespaces.  */
	if (zip_size != 0 && zip_size != ULINT_UNDEFINED) {
		space_flags
			|= (get_bit_shift(zip_size >> PAGE_ZIP_MIN_SIZE_SHIFT
					 << 1)
			   << DICT_TF_ZSSIZE_SHIFT)
			| DICT_TF_COMPACT
			| (DICT_TF_FORMAT_ZIP << DICT_TF_FORMAT_SHIFT);
		ut_a(page_size_t(space_flags).physical() == zip_size);
	}
	*success = xb_space_create_file(real_name, space_id, space_flags,
					&file);
	goto exit;

found:
	/* open the file and return it's handle */

	file = os_file_create_simple_no_error_handling(0, real_name,
						       OS_FILE_OPEN,
						       OS_FILE_READ_WRITE,
						       srv_read_only_mode,
						       &ok);

	if (ok) {
		*success = true;
	} else {
		msg("xtrabackup: Cannot open file %s\n", real_name);
	}

exit:

	return file;
}

/************************************************************************
Applies a given .delta file to the corresponding data file.
@return TRUE on success */
static
ibool
xtrabackup_apply_delta(
	const char*	dirname,	/* in: dir name of incremental */
	const char*	dbname,		/* in: database name (ibdata: NULL) */
	const char*	filename,	/* in: file name (not a path),
					including the .delta extension */
	void*		/*data*/)
{
	pfs_os_file_t	src_file = XB_FILE_UNDEFINED;
	pfs_os_file_t	dst_file = XB_FILE_UNDEFINED;
	char	src_path[FN_REFLEN * 2 + 2];
	char	dst_path[FN_REFLEN * 2 + 2];
	char	meta_path[FN_REFLEN];
	char	space_name[FN_REFLEN];
	bool	success;

	ibool	last_buffer = FALSE;
	ulint	page_in_buffer;
	ulint	incremental_buffers = 0;

	xb_delta_info_t info;
	ulint		page_size;
	ulint		page_size_shift;
	byte*		incremental_buffer_base = NULL;
	byte*		incremental_buffer;

	size_t		offset;

	IORequest	read_request(IORequest::READ);
	IORequest	write_request(IORequest::WRITE);

	ut_a(xtrabackup_incremental);

	if (dbname) {
		snprintf(src_path, sizeof(src_path), "%s/%s/%s",
			 dirname, dbname, filename);
		snprintf(dst_path, sizeof(dst_path), "%s/%s/%s",
			 xtrabackup_real_target_dir, dbname, filename);
	} else {
		snprintf(src_path, sizeof(src_path), "%s/%s",
			 dirname, filename);
		snprintf(dst_path, sizeof(dst_path), "%s/%s",
			 xtrabackup_real_target_dir, filename);
	}
	dst_path[strlen(dst_path) - 6] = '\0';

	strncpy(space_name, filename, FN_REFLEN);
	space_name[strlen(space_name) -  6] = 0;

	if (!get_meta_path(src_path, meta_path)) {
		goto error;
	}

	os_normalize_path(dst_path);
	os_normalize_path(src_path);
	os_normalize_path(meta_path);

	if (!xb_read_delta_metadata(meta_path, &info)) {
		goto error;
	}

	page_size = info.page_size;
	page_size_shift = get_bit_shift(page_size);
	msg("xtrabackup: page size for %s is %lu bytes space id is %lu\n",
	    src_path, page_size, info.space_id);
	if (page_size_shift < 10 ||
	    page_size_shift > UNIV_PAGE_SIZE_SHIFT_MAX) {
		msg("xtrabackup: error: invalid value of page_size "
		    "(%lu bytes) read from %s\n", page_size, meta_path);
		goto error;
	}

	src_file = os_file_create_simple_no_error_handling(0, src_path,
							   OS_FILE_OPEN,
							   OS_FILE_READ_WRITE,
							   srv_read_only_mode,
							   &success);
	if (!success) {
		os_file_get_last_error(TRUE);
		msg("xtrabackup: error: cannot open %s\n", src_path);
		goto error;
	}

	posix_fadvise(src_file.m_file, 0, 0, POSIX_FADV_SEQUENTIAL);

	os_file_set_nocache(src_file.m_file, src_path, "OPEN");

	dst_file = xb_delta_open_matching_space(dbname, space_name,
						info.space_id, info.space_flags,
						info.zip_size, dst_path,
						sizeof(dst_path), &success);
	if (!success) {
		msg("xtrabackup: error: cannot open %s\n", dst_path);
		goto error;
	}

	posix_fadvise(dst_file.m_file, 0, 0, POSIX_FADV_DONTNEED);

	os_file_set_nocache(dst_file.m_file, dst_path, "OPEN");

	/* allocate buffer for incremental backup */
	incremental_buffer_base = static_cast<byte *>
		(ut_malloc_nokey((page_size / 4 + 1) * page_size +
			UNIV_PAGE_SIZE_MAX));
	incremental_buffer = static_cast<byte *>
		(ut_align(incremental_buffer_base, UNIV_PAGE_SIZE_MAX));

	msg("Applying %s to %s...\n", src_path, dst_path);

	while (!last_buffer) {
		ulint cluster_header;

		/* read to buffer */
		/* first block of block cluster */
		offset = ((incremental_buffers * (page_size / 4))
			 << page_size_shift);
		success = os_file_read(read_request, src_file,
				       incremental_buffer, offset, page_size);
		if (!success) {
			goto error;
		}

		cluster_header = mach_read_from_4(incremental_buffer);
		switch(cluster_header) {
			case 0x78747261UL: /*"xtra"*/
				break;
			case 0x58545241UL: /*"XTRA"*/
				last_buffer = TRUE;
				break;
			default:
				msg("xtrabackup: error: %s seems not "
				    ".delta file.\n", src_path);
				goto error;
		}

		for (page_in_buffer = 1; page_in_buffer < page_size / 4;
		     page_in_buffer++) {
			if (mach_read_from_4(incremental_buffer +
					     page_in_buffer * 4)
			    == 0xFFFFFFFFUL)
				break;
		}

		ut_a(last_buffer || page_in_buffer == page_size / 4);

		/* read whole of the cluster */
		success = os_file_read(read_request, src_file,
				       incremental_buffer, offset,
				       page_in_buffer * page_size);
		if (!success) {
			goto error;
		}

		posix_fadvise(src_file.m_file, offset,
			      page_in_buffer * page_size,
			      POSIX_FADV_DONTNEED);

		for (page_in_buffer = 1; page_in_buffer < page_size / 4;
		     page_in_buffer++) {
			ulint offset_on_page;

			offset_on_page = mach_read_from_4(incremental_buffer +
							  page_in_buffer * 4);

			if (offset_on_page == 0xFFFFFFFFUL)
				break;

			success = os_file_write(write_request, dst_path,
						dst_file,
						incremental_buffer +
						page_in_buffer * page_size,
						(offset_on_page <<
						 page_size_shift),
						page_size);
			if (!success) {
				goto error;
			}
		}

		incremental_buffers++;
	}

	if (incremental_buffer_base)
		ut_free(incremental_buffer_base);
	if (src_file != XB_FILE_UNDEFINED)
		os_file_close(src_file);
	if (dst_file != XB_FILE_UNDEFINED)
		os_file_close(dst_file);
	return TRUE;

error:
	if (incremental_buffer_base)
		ut_free(incremental_buffer_base);
	if (src_file != XB_FILE_UNDEFINED)
		os_file_close(src_file);
	if (dst_file != XB_FILE_UNDEFINED)
		os_file_close(dst_file);
	msg("xtrabackup: Error: xtrabackup_apply_delta(): "
	    "failed to apply %s to %s.\n", src_path, dst_path);
	return FALSE;
}

/************************************************************************
Callback to handle datadir entry. Function of this type will be called
for each entry which matches the mask by xb_process_datadir.
@return should return TRUE on success */
typedef ibool (*handle_datadir_entry_func_t)(
/*=========================================*/
	const char*	data_home_dir,		/*!<in: path to datadir */
	const char*	db_name,		/*!<in: database name */
	const char*	file_name,		/*!<in: file name with suffix */
	void*		arg);			/*!<in: caller-provided data */

/************************************************************************
Callback to handle datadir entry. Deletes entry if it has no matching
fil_space in fil_system directory.
@return FALSE if delete attempt was unsuccessful */
static
ibool
rm_if_not_found(
	const char*	data_home_dir,		/*!<in: path to datadir */
	const char*	db_name,		/*!<in: database name */
	const char*	file_name,		/*!<in: file name with suffix */
	void*		arg __attribute__((unused)))
{
	char			name[FN_REFLEN];
	xb_filter_entry_t*	table;

	if (db_name != NULL) {
		snprintf(name, FN_REFLEN, "%s/%s", db_name, file_name);
	} else {
		snprintf(name, FN_REFLEN, "%s", file_name);
	}
	/* Truncate ".ibd" */
	name[strlen(name) - 4] = '\0';

	HASH_SEARCH(name_hash, inc_dir_tables_hash, ut_fold_string(name),
		    xb_filter_entry_t*,
		    table, (void) 0,
		    !strcmp(table->name, name));

	if (!table) {
		if (db_name != NULL) {
			snprintf(name, FN_REFLEN, "%s/%s/%s", data_home_dir,
				 db_name, file_name);
		} else {
			snprintf(name, FN_REFLEN, "%s/%s", data_home_dir,
				 file_name);
		}
		return os_file_delete(0, name);
	}

	return(TRUE);
}

/************************************************************************
Function enumerates files in datadir (provided by path) which are matched
by provided suffix. For each entry callback is called.
@return FALSE if callback for some entry returned FALSE */
static
ibool
xb_process_datadir(
	const char*			path,	/*!<in: datadir path */
	const char*			suffix,	/*!<in: suffix to match
						against */
	handle_datadir_entry_func_t	func,	/*!<in: callback */
	void*				data)	/*!<in: additional argument for
						callback */
{
	ulint		ret;
	char		dbpath[FN_REFLEN];
	os_file_dir_t	dir;
	os_file_dir_t	dbdir;
	os_file_stat_t	dbinfo;
	os_file_stat_t	fileinfo;
	ulint		suffix_len;
	dberr_t		err 		= DB_SUCCESS;
	static char	current_dir[2];

	current_dir[0] = FN_CURLIB;
	current_dir[1] = 0;
	srv_data_home = current_dir;

	suffix_len = strlen(suffix);

	/* datafile */
	dbdir = os_file_opendir(path, false);

	if (dbdir != NULL) {
		ret = fil_file_readdir_next_file(&err, path, dbdir,
							&fileinfo);
		while (ret == 0) {
			if (fileinfo.type == OS_FILE_TYPE_DIR) {
				goto next_file_item_1;
			}

			if (strlen(fileinfo.name) > suffix_len
			    && 0 == strcmp(fileinfo.name + 
					strlen(fileinfo.name) - suffix_len,
					suffix)) {
				if (!func(
					    path, NULL,
					    fileinfo.name, data))
				{
					return(FALSE);
				}
			}
next_file_item_1:
			ret = fil_file_readdir_next_file(&err,
							path, dbdir,
							&fileinfo);
		}

		os_file_closedir(dbdir);
	} else {
		msg("xtrabackup: Cannot open dir %s\n",
		    path);
	}

	/* single table tablespaces */
	dir = os_file_opendir(path, false);

	if (dir == NULL) {
		msg("xtrabackup: Cannot open dir %s\n",
		    path);
	}

		ret = fil_file_readdir_next_file(&err, path, dir,
								&dbinfo);
	while (ret == 0) {
		if (dbinfo.type == OS_FILE_TYPE_FILE
		    || dbinfo.type == OS_FILE_TYPE_UNKNOWN) {

		        goto next_datadir_item;
		}

		fn_format(dbpath, dbinfo.name, path, "", MYF(0));
		os_normalize_path(dbpath);

		dbdir = os_file_opendir(dbpath, false);

		if (dbdir != NULL) {

			ret = fil_file_readdir_next_file(&err, dbpath, dbdir,
								&fileinfo);
			while (ret == 0) {

			        if (fileinfo.type == OS_FILE_TYPE_DIR) {

				        goto next_file_item_2;
				}

				if (strlen(fileinfo.name) > suffix_len
				    && 0 == strcmp(fileinfo.name + 
						strlen(fileinfo.name) -
								suffix_len,
						suffix)) {
					/* The name ends in suffix; process
					the file */
					if (!func(
						    path,
						    dbinfo.name,
						    fileinfo.name, data))
					{
						return(FALSE);
					}
				}
next_file_item_2:
				ret = fil_file_readdir_next_file(&err,
								dbpath, dbdir,
								&fileinfo);
			}

			os_file_closedir(dbdir);
		}
next_datadir_item:
		ret = fil_file_readdir_next_file(&err,
						path,
								dir, &dbinfo);
	}

	os_file_closedir(dir);

	return(TRUE);
}

/************************************************************************
Applies all .delta files from incremental_dir to the full backup.
@return TRUE on success. */
static
ibool
xtrabackup_apply_deltas()
{
	return xb_process_datadir(xtrabackup_incremental_dir, ".delta",
		xtrabackup_apply_delta, NULL);
}

static my_bool
xtrabackup_close_temp_log(my_bool clear_flag)
{
	pfs_os_file_t	src_file = XB_FILE_UNDEFINED;
	char		src_path[FN_REFLEN];
	char		dst_path[FN_REFLEN];
	bool		success;
	byte		log_buf[UNIV_PAGE_SIZE_MAX];
	IORequest	read_request(IORequest::READ);
	IORequest	write_request(IORequest::WRITE);

	if (!xtrabackup_logfile_is_renamed)
		return(FALSE);

	/* rename 'ib_logfile0' to 'xtrabackup_logfile' */
	if(!xtrabackup_incremental_dir) {
		sprintf(dst_path, "%s/ib_logfile0", xtrabackup_target_dir);
		sprintf(src_path, "%s/%s", xtrabackup_target_dir,
			XB_LOG_FILENAME);
	} else {
		sprintf(dst_path, "%s/ib_logfile0", xtrabackup_incremental_dir);
		sprintf(src_path, "%s/%s", xtrabackup_incremental_dir,
			XB_LOG_FILENAME);
	}

	os_normalize_path(dst_path);
	os_normalize_path(src_path);

	success = os_file_rename(0, dst_path, src_path);
	if (!success) {
		goto error;
	}
	xtrabackup_logfile_is_renamed = FALSE;

	if (!clear_flag)
		return(FALSE);

	/* clear LOG_HEADER_CREATOR field */
	src_file = os_file_create_simple_no_error_handling(0, src_path,
							   OS_FILE_OPEN,
							   OS_FILE_READ_WRITE,
							   srv_read_only_mode,
							   &success);
	if (!success) {
		goto error;
	}

	success = os_file_read(read_request, src_file, log_buf, 0,
			       LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	memset(log_buf + LOG_HEADER_CREATOR, ' ', 4);

	success = os_file_write(write_request, src_path, src_file, log_buf, 0,
				LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	// os_file_close(src_file);
	src_file = XB_FILE_UNDEFINED;

	innobase_log_files_in_group = innobase_log_files_in_group_save;
	srv_log_group_home_dir = srv_log_group_home_dir_save;
	innobase_log_file_size = innobase_log_file_size_save;

	return(FALSE);
error:
	if (src_file != XB_FILE_UNDEFINED)
		os_file_close(src_file);
	msg("xtrabackup: Error: xtrabackup_close_temp_log() failed.\n");
	return(TRUE); /*ERROR*/
}


/*********************************************************************//**
Write the meta data (index user fields) config file.
@return true in case of success otherwise false. */
static
bool
xb_export_cfg_write_index_fields(
/*===========================*/
	const dict_index_t*	index,	/*!< in: write the meta data for
					this index */
	FILE*			file)	/*!< in: file to write to */
{
	byte			row[sizeof(ib_uint32_t) * 2];

	for (ulint i = 0; i < index->n_fields; ++i) {
		byte*			ptr = row;
		const dict_field_t*	field = &index->fields[i];

		mach_write_to_4(ptr, field->prefix_len);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, field->fixed_len);

		if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {

			msg("xtrabackup: Error: writing index fields.");

			return(false);
		}

		/* Include the NUL byte in the length. */
		ib_uint32_t	len = strlen(field->name) + 1;
		ut_a(len > 1);

		mach_write_to_4(row, len);

		if (fwrite(row, 1,  sizeof(len), file) != sizeof(len)
		    || fwrite(field->name, 1, len, file) != len) {

			msg("xtrabackup: Error: writing index column.");

			return(false);
		}
	}

	return(true);
}

/*********************************************************************//**
Write the meta data config file index information.
@return true in case of success otherwise false. */
static	__attribute__((nonnull, warn_unused_result))
bool
xb_export_cfg_write_indexes(
/*======================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file)	/*!< in: file to write to */
{
	{
		byte		row[sizeof(ib_uint32_t)];

		/* Write the number of indexes in the table. */
		mach_write_to_4(row, UT_LIST_GET_LEN(table->indexes));

		if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
			msg("xtrabackup: Error: writing index count.");

			return(false);
		}
	}

	bool			ret = true;

	/* Write the index meta data. */
	for (const dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
	     index != 0 && ret;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		byte*		ptr;
		byte		row[sizeof(ib_uint64_t)
				    + sizeof(ib_uint32_t) * 8];

		ptr = row;

		ut_ad(sizeof(ib_uint64_t) == 8);
		mach_write_to_8(ptr, index->id);
		ptr += sizeof(ib_uint64_t);

		mach_write_to_4(ptr, index->space);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->page);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->type);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->trx_id_offset);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->n_user_defined_cols);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->n_uniq);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->n_nullable);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->n_fields);

		if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {

			msg("xtrabackup: Error: writing index meta-data.");

			return(false);
		}

		/* Write the length of the index name.
		NUL byte is included in the length. */
		ib_uint32_t	len = strlen(index->name) + 1;
		ut_a(len > 1);

		mach_write_to_4(row, len);

		if (fwrite(row, 1, sizeof(len), file) != sizeof(len)
		    || fwrite(index->name, 1, len, file) != len) {

			msg("xtrabackup: Error: writing index name.");

			return(false);
		}

		ret = xb_export_cfg_write_index_fields(index, file);
	}

	return(ret);
}

/*********************************************************************//**
Write the meta data (table columns) config file. Serialise the contents of
dict_col_t structure, along with the column name. All fields are serialized
as ib_uint32_t.
@return true in case of success otherwise false. */
static	__attribute__((nonnull, warn_unused_result))
bool
xb_export_cfg_write_table(
/*====================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file)	/*!< in: file to write to */
{
	dict_col_t*		col;
	byte			row[sizeof(ib_uint32_t) * 7];

	col = table->cols;

	for (ulint i = 0; i < table->n_cols; ++i, ++col) {
		byte*		ptr = row;

		mach_write_to_4(ptr, col->prtype);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->mtype);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->len);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->mbminmaxlen);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->ind);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->ord_part);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->max_prefix);

		if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
			msg("xtrabackup: Error: writing table column data.");

			return(false);
		}

		/* Write out the column name as [len, byte array]. The len
		includes the NUL byte. */
		ib_uint32_t	len;
		const char*	col_name;

		col_name = dict_table_get_col_name(table, dict_col_get_no(col));

		/* Include the NUL byte in the length. */
		len = strlen(col_name) + 1;
		ut_a(len > 1);

		mach_write_to_4(row, len);

		if (fwrite(row, 1,  sizeof(len), file) != sizeof(len)
		    || fwrite(col_name, 1, len, file) != len) {

			msg("xtrabackup: Error: writing column name.");

			return(false);
		}
	}

	return(true);
}

/*********************************************************************//**
Write the meta data config file header.
@return true in case of success otherwise false. */
static	__attribute__((nonnull, warn_unused_result))
bool
xb_export_cfg_write_header(
/*=====================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file)	/*!< in: file to write to */
{
	byte			value[sizeof(ib_uint32_t)];

	/* Write the meta-data version number. */
	mach_write_to_4(value, IB_EXPORT_CFG_VERSION_V1);

	if (fwrite(&value, 1, sizeof(value), file) != sizeof(value)) {
		msg("xtrabackup: Error: writing meta-data version number.");

		return(false);
	}

	/* Write the server hostname. */
	ib_uint32_t		len;
	const char*		hostname = "Hostname unknown";

	/* The server hostname includes the NUL byte. */
	len = strlen(hostname) + 1;
	mach_write_to_4(value, len);

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)
	    || fwrite(hostname, 1,  len, file) != len) {

		msg("xtrabackup: Error: writing hostname.");

		return(false);
	}

	/* The table name includes the NUL byte. */
	ut_a(table->name.m_name != NULL);
	len = strlen(table->name.m_name) + 1;

	/* Write the table name. */
	mach_write_to_4(value, len);

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)
	    || fwrite(table->name.m_name, 1,  len, file) != len) {

		msg("xtrabackup: Error: writing table name.");

		return(false);
	}

	byte		row[sizeof(ib_uint32_t) * 3];

	/* Write the next autoinc value. */
	mach_write_to_8(row, table->autoinc);

	if (fwrite(row, 1, sizeof(ib_uint64_t), file) != sizeof(ib_uint64_t)) {
		msg("xtrabackup: Error: writing table autoinc value.");

		return(false);
	}

	byte*		ptr = row;

	/* Write the system page size. */
	mach_write_to_4(ptr, UNIV_PAGE_SIZE);
	ptr += sizeof(ib_uint32_t);

	/* Write the table->flags. */
	mach_write_to_4(ptr, table->flags);
	ptr += sizeof(ib_uint32_t);

	/* Write the number of columns in the table. */
	mach_write_to_4(ptr, table->n_cols);

	if (fwrite(row, 1,  sizeof(row), file) != sizeof(row)) {
		msg("xtrabackup: Error: writing table meta-data.");

		return(false);
	}

	return(true);
}

/*********************************************************************//**
Write MySQL 5.6-style meta data config file.
@return true in case of success otherwise false. */
static
bool
xb_export_cfg_write(
	const fil_node_t*	node,
	const dict_table_t*	table)	/*!< in: write the meta data for
					this table */
{
	char	file_path[FN_REFLEN];
	FILE*	file;
	bool	success;

	strcpy(file_path, node->name);
	strcpy(file_path + strlen(file_path) - 4, ".cfg");

	file = fopen(file_path, "w+b");

	if (file == NULL) {
		msg("xtrabackup: Error: cannot open %s\n", node->name);

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
			msg("xtrabackup: Error: cannot close %s\n", node->name);
			success = false;
		}

	}

	return(success);

}

/** Write the transfer key to CFP file.
@param[in]	table		write the data for this table
@param[in]	file		file to write to
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
dberr_t
xb_export_write_transfer_key(
	const dict_table_t*	table,
	FILE*			file)
{
	byte		key_size[sizeof(ib_uint32_t)];
	byte		row[ENCRYPTION_KEY_LEN * 3];
	byte*		ptr = row;
	byte*		transfer_key = ptr;
	lint		elen;

	ut_ad(table->encryption_key != NULL
	      && table->encryption_iv != NULL);

	/* Write the encryption key size. */
	mach_write_to_4(key_size, ENCRYPTION_KEY_LEN);

	if (fwrite(&key_size, 1,  sizeof(key_size), file)
		!= sizeof(key_size)) {
		msg("IO Write error: (%d, %s) %s",
			errno, strerror(errno),
			"while writing key size.");

		return(DB_IO_ERROR);
	}

	/* Generate and write the transfer key. */
	Encryption::random_value(transfer_key);
	if (fwrite(transfer_key, 1, ENCRYPTION_KEY_LEN, file)
		!= ENCRYPTION_KEY_LEN) {
		msg("IO Write error: (%d, %s) %s",
			errno, strerror(errno),
			"while writing transfer key.");

		return(DB_IO_ERROR);
	}

	ptr += ENCRYPTION_KEY_LEN;

	/* Encrypt tablespace key. */
	elen = my_aes_encrypt(
		reinterpret_cast<unsigned char*>(table->encryption_key),
		ENCRYPTION_KEY_LEN,
		ptr,
		reinterpret_cast<unsigned char*>(transfer_key),
		ENCRYPTION_KEY_LEN,
		my_aes_256_ecb,
		NULL, false);

	if (elen == MY_AES_BAD_DATA) {
		msg("IO Write error: (%d, %s) %s",
			errno, strerror(errno),
			"while encrypt tablespace key.");
		return(DB_ERROR);
	}

	/* Write encrypted tablespace key */
	if (fwrite(ptr, 1, ENCRYPTION_KEY_LEN, file)
		!= ENCRYPTION_KEY_LEN) {
		msg("IO Write error: (%d, %s) %s",
			errno, strerror(errno),
			"while writing encrypted tablespace key.");

		return(DB_IO_ERROR);
	}
	ptr += ENCRYPTION_KEY_LEN;

	/* Encrypt tablespace iv. */
	elen = my_aes_encrypt(
		reinterpret_cast<unsigned char*>(table->encryption_iv),
		ENCRYPTION_KEY_LEN,
		ptr,
		reinterpret_cast<unsigned char*>(transfer_key),
		ENCRYPTION_KEY_LEN,
		my_aes_256_ecb,
		NULL, false);

	if (elen == MY_AES_BAD_DATA) {
		msg("IO Write error: (%d, %s) %s",
			errno, strerror(errno),
			"while encrypt tablespace iv.");
		return(DB_ERROR);
	}

	/* Write encrypted tablespace iv */
	if (fwrite(ptr, 1, ENCRYPTION_KEY_LEN, file)
		!= ENCRYPTION_KEY_LEN) {
		msg("IO Write error: (%d, %s) %s",
			errno, strerror(errno),
			"while writing encrypted tablespace iv.");

		return(DB_IO_ERROR);
	}

	return(DB_SUCCESS);
}

/** Write the encryption data after quiesce.
@param[in]	table		write the data for this table
@return DB_SUCCESS or error code */
static	__attribute__((nonnull, warn_unused_result))
dberr_t
xb_export_cfp_write(
	dict_table_t*	table)
{
	dberr_t			err;
	char			name[OS_FILE_MAX_PATH];

	/* If table is not encrypted, return. */
	if (!dict_table_is_encrypted(table)) {
		return(DB_SUCCESS);
	}

	/* Get the encryption key and iv from space */
	/* For encrypted table, before we discard the tablespace,
	we need save the encryption information into table, otherwise,
	this information will be lost in fil_discard_tablespace along
	with fil_space_free(). */
	if (table->encryption_key == NULL) {
		table->encryption_key =
			static_cast<byte*>(mem_heap_alloc(table->heap,
							  ENCRYPTION_KEY_LEN));

		table->encryption_iv =
			static_cast<byte*>(mem_heap_alloc(table->heap,
							  ENCRYPTION_KEY_LEN));

		fil_space_t*	space = fil_space_get(table->space);
		ut_ad(space != NULL && FSP_FLAGS_GET_ENCRYPTION(space->flags));

		memcpy(table->encryption_key,
		       space->encryption_key,
		       ENCRYPTION_KEY_LEN);
		memcpy(table->encryption_iv,
		       space->encryption_iv,
		       ENCRYPTION_KEY_LEN);
	}

	srv_get_encryption_data_filename(table, name, sizeof(name));

	ib::info() << "Writing table encryption data to '" << name << "'";

	FILE*	file = fopen(name, "w+b");

	if (file == NULL) {
		msg("Can't create file '%-.200s' (errno: %d - %s)",
			 name, errno, strerror(errno));

		err = DB_IO_ERROR;
	} else {
		err = xb_export_write_transfer_key(table, file);

		if (fflush(file) != 0) {

			char	buf[BUFSIZ];

			ut_snprintf(buf, sizeof(buf), "%s flush() failed",
				    name);

			msg("IO Write error: (%d, %s) %s",
				errno, strerror(errno), buf);

			err = DB_IO_ERROR;
		}

		if (fclose(file) != 0) {
			char	buf[BUFSIZ];

			ut_snprintf(buf, sizeof(buf), "%s flose() failed",
				    name);

			msg("IO Write error: (%d, %s) %s",
				errno, strerror(errno), buf);
			err = DB_IO_ERROR;
		}
	}

	/* Clean the encryption information */
	table->encryption_key = NULL;
	table->encryption_iv = NULL;

	return(err);
}

/*********************************************************************//**
Re-encrypt tablespace keys with newly generated master key having ID
based on new server-id.
@return true in case of success otherwise false. */
static
bool
reencrypt_tablespace_keys(
	ulint new_server_id)
{
	byte*			master_key = NULL;
	bool			ret = false;
	Encryption::Version	version;

	/* Check if keyring loaded and the current master key
	can be fetched. */
	if (Encryption::master_key_id != 0) {
		ulint			master_key_id;

		Encryption::get_master_key(&master_key_id,
					   &master_key,
					   &version);
		if (master_key == NULL) {
			msg("xtrabackup: error: Can't find master key.\n");
			return(false);
		}
		msg("xtrabackup: found master key version %s.\n",
		    version == Encryption::ENCRYPTION_VERSION_1 ?
		    "= 5.7.11" : ">= 5.7.12");
		my_free(master_key);

		if (version != Encryption::ENCRYPTION_VERSION_1) {
			msg("xtrabackup: reencryption is not needed.\n");
			return(true);
		}
	} else {

		/* no encrypted tablespaces */

		return(true);
	}

	master_key = NULL;

	/* Generate the new master key. */
	server_id = new_server_id;
	Encryption::create_master_key_v0(&master_key);

        if (master_key == NULL) {
		msg("xtrabackup: error: Can't create master key.\n");
                return(false);
        }

	ret = fil_encryption_rotate();

	my_free(master_key);

	/* If rotation failure, return error */
	if (!ret) {
		msg("xtrabackup: error: Can't rotate master key.\n");
	} else {
		msg("xtrabackup: Keys reencrypted for server-id %lu.\n",
		    server_id);
	}

	return(ret);
}

#if 0
/********************************************************************//**
Searches archived log files in archived log directory. The min and max
LSN's of found files as well as archived log file size are stored in
xtrabackup_arch_first_file_lsn, xtrabackup_arch_last_file_lsn and
xtrabackup_arch_file_size respectively.
@return true on success
*/
static
bool
xtrabackup_arch_search_files(
/*=========================*/
	ib_uint64_t	start_lsn)		/*!< in: filter out log files
						witch does not contain data
						with lsn < start_lsn */
{
	os_file_dir_t	dir;
	os_file_stat_t	fileinfo;
	ut_ad(innobase_log_arch_dir);

	dir = os_file_opendir(innobase_log_arch_dir, FALSE);
	if (!dir) {
		msg("xtrabackup: error: cannot open archived log directory %s\n",
		    innobase_log_arch_dir);
		return false;
	}

	while(!os_file_readdir_next_file(innobase_log_arch_dir,
					 dir,
					 &fileinfo) ) {
		lsn_t	log_file_lsn;
		char*	log_str_end_lsn_ptr;

		if (strncmp(fileinfo.name,
			    IB_ARCHIVED_LOGS_PREFIX,
			    sizeof(IB_ARCHIVED_LOGS_PREFIX) - 1)) {
			continue;
		}

		log_file_lsn = strtoll(fileinfo.name +
				       sizeof(IB_ARCHIVED_LOGS_PREFIX) - 1,
				       &log_str_end_lsn_ptr, 10);

		if (*log_str_end_lsn_ptr) {
			continue;
		}

		if (log_file_lsn + (fileinfo.size - LOG_FILE_HDR_SIZE)	< start_lsn) {
			continue;
		}

		if (!xtrabackup_arch_first_file_lsn ||
		    log_file_lsn < xtrabackup_arch_first_file_lsn) {
			xtrabackup_arch_first_file_lsn = log_file_lsn;
		}
		if (log_file_lsn > xtrabackup_arch_last_file_lsn) {
			xtrabackup_arch_last_file_lsn = log_file_lsn;
		}

		//TODO: find the more suitable way to extract archived log file
		//size
		if (fileinfo.size > (int64_t)xtrabackup_arch_file_size) {
			xtrabackup_arch_file_size = fileinfo.size;
		}
	}

	return xtrabackup_arch_first_file_lsn != 0;
}
#endif

static
void
innodb_free_param()
{
	srv_sys_space.shutdown();
	srv_tmp_space.shutdown();
	free(internal_innobase_data_file_path);
	internal_innobase_data_file_path = NULL;
	free_tmpdir(&mysql_tmpdir_list);
}


/**************************************************************************
Store the current binary log coordinates in a specified file.
@return 'false' on error. */
static bool
store_binlog_info(
/*==============*/
	const char *filename)	/*!< in: output file name */
{
	FILE *fp;

	if (trx_sys_mysql_bin_log_name[0] == '\0') {
		return(true);
	}

	fp = fopen(filename, "w");

	if (!fp) {
		msg("xtrabackup: failed to open '%s'\n", filename);
		return(false);
	}

	fprintf(fp, "%s\t" UINT64PF "\n",
		trx_sys_mysql_bin_log_name, trx_sys_mysql_bin_log_pos);
	fclose(fp);

	return(true);
}


/**************************************************************************
Store current master key ID.
@return 'false' on error. */
static bool
store_master_key_id(
/*==============*/
	const char *filename)	/*!< in: output file name */
{
	FILE *fp;

	fp = fopen(filename, "w");

	if (!fp) {
		msg("xtrabackup: failed to open '%s'\n", filename);
		return(false);
	}

	fprintf(fp, "%lu", Encryption::master_key_id);
	fclose(fp);

	return(true);
}

static void
xtrabackup_prepare_func(int argc, char **argv)
{
	ulint	err;
	datafiles_iter_t	*it;
	fil_node_t		*node;
	fil_space_t		*space;
	char			 metadata_path[FN_REFLEN];
	IORequest		write_request(IORequest::WRITE);

	mysql_mutex_init(key_LOCK_global_system_variables,
			 &LOCK_global_system_variables, MY_MUTEX_INIT_FAST);

	/* cd to target-dir */

	if (my_setwd(xtrabackup_real_target_dir,MYF(MY_WME)))
	{
		msg("xtrabackup: cannot my_setwd %s\n",
		    xtrabackup_real_target_dir);
		exit(EXIT_FAILURE);
	}
	msg("xtrabackup: cd to %s\n", xtrabackup_real_target_dir);

	xtrabackup_target_dir= mysql_data_home_buff;
	xtrabackup_target_dir[0]=FN_CURLIB;		// all paths are relative from here
	xtrabackup_target_dir[1]=0;

	/*
	  read metadata of target, we don't need metadata reading in the case
	  archived logs applying
	*/
	sprintf(metadata_path, "%s/%s", xtrabackup_target_dir,
		XTRABACKUP_METADATA_FILENAME);

	if (!xtrabackup_read_metadata(metadata_path)) {
		msg("xtrabackup: Error: failed to read metadata from '%s'\n",
		    metadata_path);
		exit(EXIT_FAILURE);
	}

	if (!innobase_log_arch_dir)
	{
		if (!strcmp(metadata_type, "full-backuped")) {
			msg("xtrabackup: This target seems to be not prepared "
			    "yet.\n");
		} else if (!strcmp(metadata_type, "log-applied")) {
			msg("xtrabackup: This target seems to be already "
			    "prepared with --apply-log-only.\n");
			goto skip_check;
		} else if (!strcmp(metadata_type, "full-prepared")) {
			msg("xtrabackup: This target seems to be already "
			    "prepared.\n");
		} else {
			msg("xtrabackup: This target seems not to have correct "
			    "metadata...\n");
			exit(EXIT_FAILURE);
		}

		if (xtrabackup_incremental) {
			msg("xtrabackup: error: applying incremental backup "
			    "needs target prepared with --apply-log-only.\n");
			exit(EXIT_FAILURE);
		}
skip_check:
		if (xtrabackup_incremental
		    && metadata_to_lsn != incremental_lsn) {
			msg("xtrabackup: error: This incremental backup seems "
			    "not to be proper for the target.\n"
			    "xtrabackup:  Check 'to_lsn' of the target and "
			    "'from_lsn' of the incremental.\n");
			exit(EXIT_FAILURE);
		}
	}

        if (xtrabackup_incremental) {
		backup_redo_log_flushed_lsn = incremental_flushed_lsn;
        }

        /* Create logfiles for recovery from 'xtrabackup_logfile', before start InnoDB */
	srv_max_n_threads = 1000;
	/* temporally dummy value to avoid crash */
	srv_page_size_shift = 14;
	srv_page_size = (1 << srv_page_size_shift);
	sync_check_init();
#ifdef UNIV_DEBUG
	sync_check_enable();
#endif
	os_thread_init();
	trx_pool_init();
	ut_crc32_init();

	xb_filters_init();

	if(!innobase_log_arch_dir && xtrabackup_init_temp_log())
		goto error_cleanup;

	if(innodb_init_param()) {
		goto error_cleanup;
	}

	if (opt_transition_key && !xb_tablespace_keys_exist()) {
		msg("xtrabackup: Error: --transition-key specified, but "
		    "xtrabackup_keys is not found.\n");
		goto error_cleanup;
	}

	if (opt_transition_key) {
		if (!xb_tablespace_keys_load(
			xtrabackup_incremental,
			opt_transition_key, strlen(opt_transition_key))) {
			msg("xtrabackup: Error: failed to load tablespace "
			    "keys\n");
			goto error_cleanup;
		}
	} else {
		if (!xb_keyring_init_for_prepare(argc, argv)) {
			msg("xtrabackup: Error: failed to init keyring "
			    "plugin\n");
			goto error_cleanup;
		}
		if (xb_tablespace_keys_exist()) {
			use_dumped_tablespace_keys = true;
			if (!xb_tablespace_keys_load(
				xtrabackup_incremental,
				NULL, 0)) {
				msg("xtrabackup: Error: failed to load "
				    "tablespace keys\n");
				goto error_cleanup;
			}
		}
	}

	/* Expand compacted datafiles */

	if (xtrabackup_compact) {
		srv_compact_backup = TRUE;

		if (!xb_expand_datafiles()) {
			goto error_cleanup;
		}

		/* Reset the 'compact' flag in xtrabackup_checkpoints so we
		don't expand on subsequent invocations. */
		xtrabackup_compact = FALSE;
		if (!xtrabackup_write_metadata(metadata_path)) {
			msg("xtrabackup: error: xtrabackup_write_metadata() "
			    "failed\n");
			goto error_cleanup;
		}
	}

	xb_normalize_init_values();

	if (xtrabackup_incremental || innobase_log_arch_dir) {
		err = xb_data_files_init();
		if (err != DB_SUCCESS) {
			msg("xtrabackup: error: xb_data_files_init() failed "
			    "with error code %lu\n", err);
			goto error_cleanup;
		}
	}
	if (xtrabackup_incremental) {
		inc_dir_tables_hash = hash_create(1000);

		if(!xtrabackup_apply_deltas()) {
			xb_data_files_close();
			xb_filter_hash_free(inc_dir_tables_hash);
			goto error_cleanup;
		}
	}
	if (xtrabackup_incremental || innobase_log_arch_dir) {
		xb_data_files_close();
	}
	if (xtrabackup_incremental) {
		/* Cleanup datadir from tablespaces deleted between full and
		incremental backups */

		xb_process_datadir("./", ".ibd", rm_if_not_found, NULL);

		xb_filter_hash_free(inc_dir_tables_hash);
	}
	if (fil_system) {
		fil_close();
	}

	trx_pool_close();

	os_thread_free();

	sync_check_close();

	innodb_free_param();

	/* Reset the configuration as it might have been changed by
	xb_data_files_init(). */
	if(innodb_init_param()) {
		goto error_cleanup;
	}

	srv_apply_log_only = (ibool) xtrabackup_apply_log_only;
	srv_rebuild_indexes = (ibool) xtrabackup_rebuild_indexes;

	/* increase IO threads */
	if(srv_n_file_io_threads < 10) {
		srv_n_read_io_threads = 4;
		srv_n_write_io_threads = 4;
	}

	msg("xtrabackup: Starting InnoDB instance for recovery.\n"
	    "xtrabackup: Using %lld bytes for buffer pool "
	    "(set by --use-memory parameter)\n", xtrabackup_use_memory);

	if(innodb_init())
		goto error_cleanup;

	it = datafiles_iter_new(fil_system);
	if (it == NULL) {
		msg("xtrabackup: Error: datafiles_iter_new() failed.\n");
		exit(EXIT_FAILURE);
	}

	while ((node = datafiles_iter_next(it)) != NULL) {
		byte		*header;
		ulint		 size;
		mtr_t		 mtr;
		buf_block_t	*block;
		ulint		 flags;

		space = node->space;

		/* Align space sizes along with fsp header. We want to process
		each space once, so skip all nodes except the first one in a
		multi-node space. */
		if (UT_LIST_GET_PREV(chain, node) != NULL) {
			continue;
		}

		mtr_start(&mtr);

		mtr_s_lock(fil_space_get_latch(space->id, &flags), &mtr);

		block = buf_page_get(page_id_t(space->id, 0),
				     page_size_t(flags),
				     RW_S_LATCH, &mtr);
		header = FSP_HEADER_OFFSET + buf_block_get_frame(block);

		size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES,
				      &mtr);

		mtr_commit(&mtr);

		fil_space_extend(space, size);
	}

	datafiles_iter_free(it);

	if (xtrabackup_export) {
		msg("xtrabackup: export option is specified.\n");
		pfs_os_file_t	info_file = XB_FILE_UNDEFINED;
		char		info_file_path[FN_REFLEN];
		bool		success;
		char		table_name[FN_REFLEN];

		byte*		page;
		byte*		buf = NULL;

		buf = static_cast<byte *>(ut_malloc_nokey(UNIV_PAGE_SIZE * 2));
		page = static_cast<byte *>(ut_align(buf, UNIV_PAGE_SIZE));

		/* flush insert buffer at shutdwon */
		innobase_fast_shutdown = 0;

		it = datafiles_iter_new(fil_system);
		if (it == NULL) {
			msg("xtrabackup: Error: datafiles_iter_new() "
			    "failed.\n");
			exit(EXIT_FAILURE);
		}
		while ((node = datafiles_iter_next(it)) != NULL) {
			int		 len;
			char		*next, *prev, *p;
			dict_table_t*	 table;
			dict_index_t*	 index;
			ulint		 n_index;

			space = node->space;

			/* treat file_per_table only */
			if (!fil_is_user_tablespace_id(space->id)) {
				continue;
			}

			/* node exist == file exist, here */
			strcpy(info_file_path, node->name);
			strcpy(info_file_path +
			       strlen(info_file_path) -
			       4, ".exp");

			len = strlen(info_file_path);

			p = info_file_path;
			prev = NULL;
			while ((next = strchr(p, OS_PATH_SEPARATOR)) != NULL)
			{
				prev = p;
				p = next + 1;
			}
			info_file_path[len - 4] = 0;
			strncpy(table_name, prev, FN_REFLEN);

			info_file_path[len - 4] = '.';

			mutex_enter(&(dict_sys->mutex));

			table = dict_table_get_low(table_name);
			if (!table) {
				msg("xtrabackup: error: "
				    "cannot find dictionary "
				    "record of table %s\n",
				    table_name);
				goto next_node;
			}

			/* Write MySQL 5.6 .cfg file */
			if (!xb_export_cfg_write(node, table)) {
				goto next_node;
			}

			index = dict_table_get_first_index(table);
			n_index = UT_LIST_GET_LEN(table->indexes);
			if (n_index > 31) {
				msg("xtrabackup: warning: table '%s' has more "
				    "than 31 indexes, .exp file was not "
				    "generated. Table will fail to import "
				    "on server version prior to 5.6.\n",
				    table_name);
				goto next_node;
			}

			/* Write transfer key for tablespace file */
			if (!xb_export_cfp_write(table)) {
				goto next_node;
			}

			/* init exp file */
			memset(page, 0, UNIV_PAGE_SIZE);
			mach_write_to_4(page    , 0x78706f72UL);
			mach_write_to_4(page + 4, 0x74696e66UL);/*"xportinf"*/
			mach_write_to_4(page + 8, n_index);
			strncpy((char *) page + 12,
				table_name, 500);

			msg("xtrabackup: export metadata of "
			    "table '%s' to file `%s` "
			    "(%lu indexes)\n",
			    table_name, info_file_path,
			    n_index);

			n_index = 1;
			while (index) {
				mach_write_to_8(page + n_index * 512, index->id);
				mach_write_to_4(page + n_index * 512 + 8,
						index->page);
				strncpy((char *) page + n_index * 512 +
					12, index->name, 500);

				msg("xtrabackup:     name=%s, "
				    "id.low=%lu, page=%lu\n",
				    index->name(),
				    (ulint)(index->id &
					    0xFFFFFFFFUL),
				    (ulint) index->page);
				index = dict_table_get_next_index(index);
				n_index++;
			}

			os_normalize_path(info_file_path);
			info_file = os_file_create(
				0,
				info_file_path,
				OS_FILE_OVERWRITE,
				OS_FILE_NORMAL, OS_DATA_FILE,
				false,
				&success);
			if (!success) {
				os_file_get_last_error(TRUE);
				goto next_node;
			}
			success = os_file_write(write_request, info_file_path,
						info_file, page,
						0, UNIV_PAGE_SIZE);
			if (!success) {
				os_file_get_last_error(TRUE);
				goto next_node;
			}
			success = os_file_flush(info_file);
			if (!success) {
				os_file_get_last_error(TRUE);
				goto next_node;
			}
next_node:
			if (info_file != XB_FILE_UNDEFINED) {
				os_file_close(info_file);
				info_file = XB_FILE_UNDEFINED;
			}
			mutex_exit(&(dict_sys->mutex));
		}

		ut_free(buf);

		datafiles_iter_free(it);
	}

	/* print the binary log position  */
	trx_sys_print_mysql_binlog_offset();
	msg("\n");

	/* output to xtrabackup_binlog_pos_innodb and (if
	backup_safe_binlog_info was available on the server) to
	xtrabackup_binlog_info. In the latter case xtrabackup_binlog_pos_innodb
	becomes redundant and is created only for compatibility. */
	if (!store_binlog_info("xtrabackup_binlog_pos_innodb") ||
	    (recover_binlog_info &&
	     !store_binlog_info(XTRABACKUP_BINLOG_INFO))) {

		exit(EXIT_FAILURE);
	}

	if (!store_master_key_id("xtrabackup_master_key_id")) {
		exit(EXIT_FAILURE);
	}

	if (innobase_log_arch_dir)
		srv_start_lsn = log_sys->lsn = recv_sys->recovered_lsn;

	/* Check whether the log is applied enough or not. */
	if ((xtrabackup_incremental
	     && srv_start_lsn < incremental_to_lsn)
	    ||(!xtrabackup_incremental
	       && srv_start_lsn < metadata_to_lsn)) {
		msg("xtrabackup: error: "
		    "The transaction log file is corrupted.\n"
		    "xtrabackup: error: "
		    "The log was not applied to the intended LSN!\n");
		msg("xtrabackup: Log applied to lsn " LSN_PF "\n",
		    srv_start_lsn);
		if (xtrabackup_incremental) {
			msg("xtrabackup: The intended lsn is " LSN_PF "\n",
			    incremental_to_lsn);
		} else {
			msg("xtrabackup: The intended lsn is " LSN_PF "\n",
			    metadata_to_lsn);
		}
		exit(EXIT_FAILURE);
	}

	xb_write_galera_info(xtrabackup_incremental);

	if(innodb_end())
		goto error_cleanup;

        innodb_free_param();

	/* re-init necessary components */
	sync_check_init();
#ifdef UNIV_DEBUG
	sync_check_enable();
#endif
	/* Reset the system variables in the recovery module. */
	os_thread_init();
	trx_pool_init();
	que_init();

	if(xtrabackup_close_temp_log(TRUE))
		exit(EXIT_FAILURE);

	/* output to metadata file */
	{
		char	filename[FN_REFLEN];

		strcpy(metadata_type, srv_apply_log_only ?
					"log-applied" : "full-prepared");

		if(xtrabackup_incremental
		   && metadata_to_lsn < incremental_to_lsn)
		{
			metadata_to_lsn = incremental_to_lsn;
			metadata_last_lsn = incremental_last_lsn;
		}

		sprintf(filename, "%s/%s", xtrabackup_target_dir, XTRABACKUP_METADATA_FILENAME);
		if (!xtrabackup_write_metadata(filename)) {

			msg("xtrabackup: Error: failed to write metadata "
			    "to '%s'\n", filename);
			exit(EXIT_FAILURE);
		}

		if(xtrabackup_extra_lsndir) {
			sprintf(filename, "%s/%s", xtrabackup_extra_lsndir, XTRABACKUP_METADATA_FILENAME);
			if (!xtrabackup_write_metadata(filename)) {
				msg("xtrabackup: Error: failed to write "
				    "metadata to '%s'\n", filename);
				exit(EXIT_FAILURE);
			}
		}
	}

	if (!apply_log_finish()) {
		exit(EXIT_FAILURE);
	}

	trx_pool_close();

	if (fil_system) {
		fil_close();
	}

	os_thread_free();

	sync_check_close();

	/* start InnoDB once again to create log files */

	if (!xtrabackup_apply_log_only) {

		/* xtrabackup_incremental_dir is used to indicate that
		we are going to apply incremental backup. Here we already
		applied incremental backup and are about to do final prepare
		of the full backup */
		xtrabackup_incremental_dir = NULL;

		if(innodb_init_param()) {
			goto error;
		}

		srv_apply_log_only = FALSE;
		srv_rebuild_indexes = FALSE;

		/* increase IO threads */
		if(srv_n_file_io_threads < 10) {
			srv_n_read_io_threads = 4;
			srv_n_write_io_threads = 4;
		}

		srv_shutdown_state = SRV_SHUTDOWN_NONE;

		if(innodb_init())
			goto error;

		if (opt_encrypt_for_server_id_specified) {
			if (!reencrypt_tablespace_keys(opt_encrypt_server_id)) {
				msg("xtrabackup: error: "
				    "Tablespace keys are not reencrypted.\n");
				goto error;
			}
		}

		if(innodb_end())
			goto error;

                innodb_free_param();

	}

	if (!use_dumped_tablespace_keys) {
		xb_keyring_shutdown();
	}

	mysql_mutex_destroy(&LOCK_global_system_variables);

	xb_filters_free();

	return;

error_cleanup:

	if (!use_dumped_tablespace_keys) {
		xb_keyring_shutdown();
	}

	xtrabackup_close_temp_log(FALSE);

	mysql_mutex_destroy(&LOCK_global_system_variables);

	xb_filters_free();

error:
	exit(EXIT_FAILURE);
}

/**************************************************************************
Signals-related setup. */
static
void
setup_signals()
/*===========*/
{
	struct sigaction sa;

	/* Print a stacktrace on some signals */
	sa.sa_flags = SA_RESETHAND | SA_NODEFER;
	sigemptyset(&sa.sa_mask);
	sigprocmask(SIG_SETMASK,&sa.sa_mask,NULL);
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
		msg("prctl() failed with errno = %d\n", errno);
		exit(EXIT_FAILURE);
	}
#endif
}

/**************************************************************************
Append group name to xb_load_default_groups list. */
static
void
append_defaults_group(const char *group, const char *default_groups[],
		      size_t default_groups_size)
{
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

bool
xb_init()
{
	const char *mixed_options[4] = {NULL, NULL, NULL, NULL};
	int n_mixed_options;

	/* sanity checks */
	if (opt_lock_ddl && opt_lock_ddl_per_table) {
		msg("Error: %s and %s are mutually exclusive\n",
		    "--lock-ddl", "--lock-ddl-per-table");
		return(false);
	}

	if (opt_slave_info
		&& opt_no_lock
		&& !opt_safe_slave_backup) {
		msg("Error: --slave-info is used with --no-lock but "
			"without --safe-slave-backup. The binlog position "
			"cannot be consistent with the backup data.\n");
		return(false);
	}

	if (opt_rsync && xtrabackup_stream_fmt) {
		msg("Error: --rsync doesn't work with --stream\n");
		return(false);
	}

	if (opt_transition_key && opt_generate_transition_key) {
		msg("Error: options --transition-key and "
		    "--generate-transition-key are mutually exclusive.\n");
		return(false);
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
		msg("Error: %s and %s are mutually exclusive\n",
			mixed_options[0], mixed_options[1]);
		return(false);
	}

	if (xtrabackup_backup) {

#ifdef HAVE_VERSION_CHECK
		if (!opt_noversioncheck) {
			version_check();
		}
#endif

		if ((mysql_connection = xb_mysql_connect()) == NULL) {
			return(false);
		}

		if (!get_mysql_vars(mysql_connection)) {
			return(false);
		}

		if (opt_check_privileges) {
			check_all_privileges();
		}

		history_start_time = time(NULL);

		if (opt_lock_ddl &&
		    !lock_tables_for_backup(mysql_connection,
					    opt_lock_ddl_timeout, 0)) {
			return (false);
		}

		parse_show_engine_innodb_status(mysql_connection);

	}

	return(true);
}

static const char*
normalize_privilege_target_name(const char* name)
{
	if (strcmp(name, "*") == 0) {
		return "\\*";
	} else {
		/* should have no regex special characters. */
		ut_ad(strpbrk(name, ".()[]*+?") == 0);
	}
	return name;
}

/******************************************************************//**
Check if specific privilege is granted.
Uses regexp magic to check if requested privilege is granted for given
database.table or database.* or *.*
or if user has 'ALL PRIVILEGES' granted.
@return true if requested privilege is granted, false otherwise. */
static bool
has_privilege(const std::list<std::string> &granted,
	const char* required,
	const char* db_name,
	const char* table_name)
{
	char buffer[1000];
	xb_regex_t priv_re;
	xb_regmatch_t tables_regmatch[1];
	bool result = false;

	db_name = normalize_privilege_target_name(db_name);
	table_name = normalize_privilege_target_name(table_name);

	int written = snprintf(buffer, sizeof(buffer),
		"GRANT .*(%s)|(ALL PRIVILEGES).* ON (\\*|`%s`)\\.(\\*|`%s`)",
		required, db_name, table_name);
	if (written < 0 || written == sizeof(buffer)
		|| !compile_regex(buffer, "has_privilege", &priv_re)) {
		exit(EXIT_FAILURE);
	}

	typedef std::list<std::string>::const_iterator string_iter;
	for (string_iter i = granted.begin(), e = granted.end(); i != e; ++i) {
		int res = xb_regexec(&priv_re, i->c_str(),
			1, tables_regmatch, 0);

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

/******************************************************************//**
Check if specific privilege is granted.
Prints error message if required privilege is missing.
@return PRIVILEGE_OK if requested privilege is granted, error otherwise. */
static
int check_privilege(
	const std::list<std::string> &granted_priv, /* in: list of
							granted privileges*/
	const char* required,		/* in: required privilege name */
	const char* target_database,	/* in: required privilege target
						database name */
	const char* target_table,	/* in: required privilege target
						table name */
	int error = PRIVILEGE_ERROR)	/* in: return value if privilege
						is not granted */
{
	if (!has_privilege(granted_priv,
		required, target_database, target_table)) {
		msg("xtrabackup: %s: missing required privilege %s on %s.%s\n",
			(error == PRIVILEGE_ERROR ? "Error" : "Warning"),
			required, target_database, target_table);
		return error;
	}
	return PRIVILEGE_OK;
}

/******************************************************************//**
Check DB user privileges according to the intended actions.

Fetches DB user privileges, determines intended actions based on
command-line arguments and prints missing privileges.
May terminate application with EXIT_FAILURE exit code.*/
static void
check_all_privileges()
{
	if (!mysql_connection) {
		/* Not connected, no queries is going to be executed. */
		return;
	}

	/* Fetch effective privileges. */
	std::list<std::string> granted_privileges;
	MYSQL_ROW row = 0;
	MYSQL_RES* result = xb_mysql_query(mysql_connection, "SHOW GRANTS",
		true);
	while((row = mysql_fetch_row(result))) {
		granted_privileges.push_back(*row);
	}
	mysql_free_result(result);

	int check_result = PRIVILEGE_OK;
	bool reload_checked = false;

	/* SHOW DATABASES */
	check_result |= check_privilege(granted_privileges,
		"SHOW DATABASES", "*", "*");

	/* SELECT 'INNODB_CHANGED_PAGES', COUNT(*) FROM INFORMATION_SCHEMA.PLUGINS */
	check_result |= check_privilege(
		granted_privileges,
		"SELECT", "INFORMATION_SCHEMA", "PLUGINS");

	/* SHOW ENGINE INNODB STATUS */
	/* SHOW FULL PROCESSLIST */
	check_result |= check_privilege(granted_privileges,
		"PROCESS", "*", "*");

	if (xb_mysql_numrows(mysql_connection,
		"SHOW DATABASES LIKE 'PERCONA_SCHEMA';",
		false) == 0) {
		/* CREATE DATABASE IF NOT EXISTS PERCONA_SCHEMA */
		check_result |= check_privilege(
			granted_privileges,
			"CREATE", "*", "*");
	} else if (xb_mysql_numrows(mysql_connection,
		"SHOW TABLES IN PERCONA_SCHEMA "
		"LIKE 'xtrabackup_history';",
		false) == 0) {
		/* CREATE TABLE IF NOT EXISTS PERCONA_SCHEMA.xtrabackup_history */
		check_result |= check_privilege(
			granted_privileges,
			"CREATE", "PERCONA_SCHEMA", "*");
	}

	/* FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS */
	if (have_flush_engine_logs
		/* FLUSH NO_WRITE_TO_BINLOG TABLES */
		|| (opt_lock_wait_timeout && !opt_kill_long_queries_timeout
			&& !opt_no_lock)
		/* FLUSH TABLES WITH READ LOCK */
		|| !opt_no_lock
		/* LOCK BINLOG FOR BACKUP */
		/* UNLOCK BINLOG */
		|| (have_backup_locks && !opt_no_lock)) {
		check_result |= check_privilege(
			granted_privileges,
			"RELOAD", "*", "*");
		reload_checked = true;
	}


	/* FLUSH TABLES WITH READ LOCK */
	if (!opt_no_lock
		/* LOCK TABLES FOR BACKUP */
		/* UNLOCK TABLES */
		&& ((have_backup_locks && !opt_no_lock) || opt_slave_info
		|| opt_binlog_info == BINLOG_INFO_ON)) {
		check_result |= check_privilege(
			granted_privileges,
			"LOCK TABLES", "*", "*");
	}

	/* SELECT innodb_to_lsn FROM PERCONA_SCHEMA.xtrabackup_history ... */
	if (opt_incremental_history_name || opt_incremental_history_uuid) {
		check_result |= check_privilege(
			granted_privileges,
			"SELECT", "PERCONA_SCHEMA", "xtrabackup_history");
	}

	if (!reload_checked
		/* FLUSH BINARY LOGS */
		&& opt_galera_info) {
		check_result |= check_privilege(
			granted_privileges,
			"RELOAD", "*", "*",
			PRIVILEGE_WARNING);
	}

	/* KILL ... */
	if (opt_kill_long_queries_timeout
		/* START SLAVE SQL_THREAD */
		/* STOP SLAVE SQL_THREAD */
		|| opt_safe_slave_backup) {
		check_result |= check_privilege(
			granted_privileges,
			"SUPER", "*", "*",
			PRIVILEGE_WARNING);
	}

	/* SHOW MASTER STATUS */
	/* SHOW SLAVE STATUS */
	if (opt_galera_info || opt_slave_info
		|| (opt_no_lock && opt_safe_slave_backup)
		/* LOCK BINLOG FOR BACKUP */
		|| (have_backup_locks && !opt_no_lock)) {
		check_result |= check_privilege(granted_privileges,
			"REPLICATION CLIENT", "*", "*",
			PRIVILEGE_WARNING);
	}

	if (check_result & PRIVILEGE_ERROR) {
		exit(EXIT_FAILURE);
	}
}

void
handle_options(int argc, char **argv, int *argc_client, char ***argv_client,
	       int *argc_server, char ***argv_server)
{
	int	i;
	int	ho_error;
	char	conf_file[FN_REFLEN];

	char*	target_dir = NULL;
	bool	prepare = false;

	*argc_client = argc;
	*argc_server = argc;
	*argv_client = argv;
	*argv_server = argv;

	/* scan options for group and config file to load defaults from */
	for (i = 1; i < argc; i++) {

		char *optend = strcend(argv[i], '=');

		if (strncmp(argv[i], "--defaults-group",
			    optend - argv[i]) == 0) {
			defaults_group = optend + 1;
			append_defaults_group(defaults_group,
				xb_server_default_groups,
				array_elements(xb_server_default_groups));
		}

		if (strncmp(argv[i], "--login-path",
			    optend - argv[i]) == 0) {
			append_defaults_group(optend + 1,
				xb_client_default_groups,
				array_elements(xb_client_default_groups));
		}

		if (!strncmp(argv[i], "--prepare",
			     optend - argv[i])) {
			prepare = true;
		}

		if (!strncmp(argv[i], "--apply-log",
			     optend - argv[i])) {
			prepare = true;
		}

		if (!strncmp(argv[i], "--target-dir",
			     optend - argv[i]) && *optend) {
			target_dir = optend + 1;
		}

		if (!*optend && argv[i][0] != '-') {
			target_dir = argv[i];
		}
	}

	snprintf(conf_file, sizeof(conf_file), "my");

	if (prepare && target_dir) {
		snprintf(conf_file, sizeof(conf_file),
			 "%s/backup-my.cnf", target_dir);
	}
	if (load_defaults(conf_file, xb_server_default_groups,
			  argc_server, argv_server)) {
		exit(EXIT_FAILURE);
	}

	print_param_str <<
		"# This MySQL options file was generated by XtraBackup.\n"
		"[" << defaults_group << "]\n";

	/* We want xtrabackup to ignore unknown options, because it only
	recognizes a small subset of server variables */
	my_getopt_skip_unknown = TRUE;

	/* Reset u_max_value for all options, as we don't want the
	--maximum-... modifier to set the actual option values */
	for (my_option *optp= xb_server_options; optp->name; optp++) {
		optp->u_max_value = (G_PTR *) &global_max_value;
	}

	/* Throw a descriptive error if --defaults-file or --defaults-extra-file
	is not the first command line argument */
	for (int i = 2 ; i < argc ; i++) {
		char *optend = strcend((argv)[i], '=');

		if (optend - argv[i] == 15 &&
                    !strncmp(argv[i], "--defaults-file", optend - argv[i])) {

			msg("xtrabackup: Error: --defaults-file "
			    "must be specified first on the command "
			    "line\n");
			exit(EXIT_FAILURE);
		}
                if (optend - argv[i] == 21 &&
		    !strncmp(argv[i], "--defaults-extra-file",
			     optend - argv[i])) {

			msg("xtrabackup: Error: --defaults-extra-file "
			    "must be specified first on the command "
			    "line\n");
			exit(EXIT_FAILURE);
		}
	}

	if (*argc_server > 0
	    && (ho_error=handle_options(argc_server, argv_server,
					xb_server_options, xb_get_one_option)))
		exit(ho_error);

	msg("xtrabackup: recognized server arguments: %s\n",
	    param_str.str().c_str());
	param_str.str("");

	if (load_defaults(conf_file, xb_client_default_groups,
			  argc_client, argv_client)) {
		exit(EXIT_FAILURE);
	}

	if (strcmp(base_name(my_progname), INNOBACKUPEX_BIN_NAME) == 0 &&
	    *argc_client > 0) {
		/* emulate innobackupex script */
		innobackupex_mode = true;
		if (!ibx_handle_options(argc_client, argv_client)) {
			exit(EXIT_FAILURE);
		}
	}

	if (*argc_client > 0
	    && (ho_error=handle_options(argc_client, argv_client,
					xb_client_options, xb_get_one_option)))
		exit(ho_error);

	msg("xtrabackup: recognized client arguments: %s\n",
	    param_str.str().c_str());
	param_str.clear();

	/* Reject command line arguments that don't look like options, i.e. are
	not of the form '-X' (single-character options) or '--option' (long
	options) */
	for (int i = 0 ; i < *argc_client ; i++) {
		const char * const opt = (*argv_client)[i];

		if (strncmp(opt, "--", 2) &&
		    !(strlen(opt) == 2 && opt[0] == '-')) {
			bool server_option = true;

			for (int j = 0; j < *argc_server; j++) {
				if (opt == (*argv_server)[j]) {
					server_option = false;
					break;
				}
			}

			if (!server_option) {
				msg("xtrabackup: Error:"
				    " unknown argument: '%s'\n", opt);
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

void setup_error_messages()
{
	static const char *all_msgs[4000];
	my_default_lc_messages = &my_locale_en_US;
	my_default_lc_messages->errmsgs->errmsgs = all_msgs;

	struct {
		int id;
		const char *fmt;
	}
	xb_msgs[] = {
		{ ER_DATABASE_NAME,"Database" },
		{ ER_TABLE_NAME,"Table"},
		{ ER_PARTITION_NAME, "Partition" },
		{ ER_SUBPARTITION_NAME, "Subpartition" },
		{ ER_TEMPORARY_NAME, "Temporary"},
		{ ER_RENAMED_NAME, "Renamed"},
		{ ER_CANT_FIND_DL_ENTRY, "Can't find symbol '%-.128s' in library"},
		{ ER_CANT_OPEN_LIBRARY, "Can't open shared library '%-.192s' (errno: %d, %-.128s)" },
		{ ER_OUTOFMEMORY, "Out of memory; restart server and try again (needed %d bytes)" },
		{ ER_CANT_OPEN_LIBRARY, "Can't open shared library '%-.192s' (errno: %d, %-.128s)" },
		{ ER_UDF_NO_PATHS, "No paths allowed for shared library" },
		{ ER_CANT_INITIALIZE_UDF,"Can't initialize function '%-.192s'; %-.80s"},
		{ ER_PLUGIN_IS_NOT_LOADED,"Plugin '%-.192s' is not loaded" }
	};

	for (int i = 0; i < (int)array_elements(all_msgs); i++) {
		all_msgs[i] = "Unknown error";
	}

	for (int i = 0; i < (int)array_elements(xb_msgs); i++) {
		all_msgs[xb_msgs[i].id - 1000] = xb_msgs[i].fmt;
	}
}

/* ================= main =================== */

int main(int argc, char **argv)
{
	char **client_defaults, **server_defaults;
	int client_argc, server_argc;
	char cwd[FN_REFLEN];

	setup_signals();

	MY_INIT(argv[0]);

	if (my_create_thread_local_key(&THR_THD,NULL) ||
	    my_create_thread_local_key(&THR_MALLOC,NULL))
	{
		exit(EXIT_FAILURE);
	}
	THR_THD_initialized = true;
	THR_MALLOC_initialized = true;

	my_thread_set_THR_THD(NULL);

	xb_regex_init();

	capture_tool_command(argc, argv);

	if (mysql_server_init(-1, NULL, NULL))
	{
		exit(EXIT_FAILURE);
	}

	system_charset_info= &my_charset_utf8_general_ci;
	key_map_full.set_all();

	handle_options(argc, argv, &client_argc, &client_defaults,
		       &server_argc, &server_defaults);

	xb_libgcrypt_init();

	if (innobackupex_mode) {
		if (!ibx_init()) {
			exit(EXIT_FAILURE);
		}
	}

	if ((!xtrabackup_print_param) && (!xtrabackup_prepare) && (strcmp(mysql_data_home, "./") == 0)) {
		if (!xtrabackup_print_param)
			usage();
		msg("\nxtrabackup: Error: Please set parameter 'datadir'\n");
		exit(EXIT_FAILURE);
	}

	/* Expand target-dir, incremental-basedir, etc. */

	my_getwd(cwd, sizeof(cwd), MYF(0));

	my_load_path(xtrabackup_real_target_dir,
		     xtrabackup_target_dir, cwd);
	unpack_dirname(xtrabackup_real_target_dir,
		       xtrabackup_real_target_dir);
	xtrabackup_target_dir= xtrabackup_real_target_dir;

	if (xtrabackup_incremental_basedir) {
		my_load_path(xtrabackup_real_incremental_basedir,
			     xtrabackup_incremental_basedir, cwd);
		unpack_dirname(xtrabackup_real_incremental_basedir,
			       xtrabackup_real_incremental_basedir);
		xtrabackup_incremental_basedir =
			xtrabackup_real_incremental_basedir;
	}

	if (xtrabackup_incremental_dir) {
		my_load_path(xtrabackup_real_incremental_dir,
			     xtrabackup_incremental_dir, cwd);
		unpack_dirname(xtrabackup_real_incremental_dir,
			       xtrabackup_real_incremental_dir);
		xtrabackup_incremental_dir = xtrabackup_real_incremental_dir;
	}

	if (xtrabackup_extra_lsndir) {
		my_load_path(xtrabackup_real_extra_lsndir,
			     xtrabackup_extra_lsndir, cwd);
		unpack_dirname(xtrabackup_real_extra_lsndir,
			       xtrabackup_real_extra_lsndir);
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
			opt_mysql_tmpdir = const_cast<char*>(DEFAULT_TMPDIR);
		}
	}

	/* temporary setting of enough size */
	srv_page_size_shift = UNIV_PAGE_SIZE_SHIFT_MAX;
	srv_page_size = UNIV_PAGE_SIZE_MAX;
	if (xtrabackup_backup && xtrabackup_incremental) {
		/* direct specification is only for --backup */
		/* and the lsn is prior to the other option */

		char* endchar;
		int error = 0;
		incremental_lsn = strtoll(xtrabackup_incremental, &endchar, 10);
		if (*endchar != '\0')
			error = 1;

		if (error) {
			msg("xtrabackup: value '%s' may be wrong format for "
			    "incremental option.\n", xtrabackup_incremental);
			exit(EXIT_FAILURE);
		}
	} else if (xtrabackup_backup && xtrabackup_incremental_basedir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_incremental_basedir,
			XTRABACKUP_METADATA_FILENAME);

		if (!xtrabackup_read_metadata(filename)) {
			msg("xtrabackup: error: failed to read metadata from "
			    "%s\n", filename);
			exit(EXIT_FAILURE);
		}

		incremental_lsn = metadata_to_lsn;
		xtrabackup_incremental = xtrabackup_incremental_basedir; //dummy
	} else if (xtrabackup_prepare && xtrabackup_incremental_dir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_incremental_dir,
			XTRABACKUP_METADATA_FILENAME);

		if (!xtrabackup_read_metadata(filename)) {
			msg("xtrabackup: error: failed to read metadata from "
			    "%s\n", filename);
			exit(EXIT_FAILURE);
		}

		incremental_lsn = metadata_from_lsn;
		incremental_to_lsn = metadata_to_lsn;
		incremental_last_lsn = metadata_last_lsn;
		incremental_flushed_lsn = backup_redo_log_flushed_lsn;
		xtrabackup_incremental = xtrabackup_incremental_dir; //dummy

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

	print_version();
	if (xtrabackup_incremental) {
		msg("incremental backup from " LSN_PF " is enabled.\n",
		    incremental_lsn);
	}

	if (xtrabackup_export && innobase_file_per_table == FALSE) {
		msg("xtrabackup: auto-enabling --innodb-file-per-table due to "
		    "the --export option\n");
		innobase_file_per_table = TRUE;
	}

	if (xtrabackup_incremental && xtrabackup_stream &&
	    xtrabackup_stream_fmt == XB_STREAM_FMT_TAR) {
		msg("xtrabackup: error: "
		    "streaming incremental backups are incompatible with the \n"
		    "'tar' streaming format. Use --stream=xbstream instead.\n");
		exit(EXIT_FAILURE);
	}

	if ((xtrabackup_compress || xtrabackup_encrypt) && xtrabackup_stream &&
	    xtrabackup_stream_fmt == XB_STREAM_FMT_TAR) {
		msg("xtrabackup: error: "
		    "compressed and encrypted backups are incompatible with the \n"
		    "'tar' streaming format. Use --stream=xbstream instead.\n");
		exit(EXIT_FAILURE);
	}

	if (!xtrabackup_prepare &&
	    (innobase_log_arch_dir || xtrabackup_archived_to_lsn)) {

		/* Default my.cnf can contain innobase_log_arch_dir option set
		for server, reset it to allow backup. */
		innobase_log_arch_dir= NULL;
		xtrabackup_archived_to_lsn= 0;
		msg("xtrabackup: warning: "
		    "as --innodb-log-arch-dir and --to-archived-lsn can be used "
		    "only with --prepare they will be reset\n");
	}
	if (xtrabackup_throttle && !xtrabackup_backup) {
		xtrabackup_throttle = 0;
		msg("xtrabackup: warning: --throttle has effect "
		    "only with --backup\n");
	}

	if (xtrabackup_backup && xtrabackup_compact) {
		msg("xtrabackup: error: compact backups are not supported "
		    "by this version of xtrabackup\n");
		exit(EXIT_FAILURE);
	}

	/* cannot execute both for now */
	{
		int num = 0;

		if (xtrabackup_backup) num++;
		if (xtrabackup_stats) num++;
		if (xtrabackup_prepare) num++;
		if (xtrabackup_copy_back) num++;
		if (xtrabackup_move_back) num++;
		if (xtrabackup_decrypt_decompress) num++;
		if (num != 1) { /* !XOR (for now) */
			usage();
			exit(EXIT_FAILURE);
		}
	}

#ifndef __WIN__
	if (xtrabackup_debug_sync) {
		signal(SIGCONT, sigcont_handler);
	}
#endif

	system_charset_info= &my_charset_utf8_general_ci;
	files_charset_info= &my_charset_utf8_general_ci;
	national_charset_info= &my_charset_utf8_general_ci;
	table_alias_charset= &my_charset_bin;
	character_set_filesystem= &my_charset_bin;

	sys_var_init();
	setup_error_messages();

	/* --backup */
	if (xtrabackup_backup) {
		xtrabackup_backup_func();
	}

	/* --stats */
	if (xtrabackup_stats) {
		xtrabackup_stats_func(server_argc, server_defaults);
	}

	/* --prepare */
	if (xtrabackup_prepare) {
		xtrabackup_prepare_func(server_argc, server_defaults);
	}

	if (xtrabackup_copy_back || xtrabackup_move_back) {
		if (!check_if_param_set("datadir")) {
			msg("Error: datadir must be specified.\n");
			exit(EXIT_FAILURE);
		}
		mysql_mutex_init(key_LOCK_keyring_operations,
				 &LOCK_keyring_operations, MY_MUTEX_INIT_FAST);
		if (!copy_back(server_argc, server_defaults)) {
			exit(EXIT_FAILURE);
		}
		mysql_mutex_destroy(&LOCK_keyring_operations);
	}

	if (xtrabackup_decrypt_decompress && !decrypt_decompress()) {
		exit(EXIT_FAILURE);
	}

	backup_cleanup();

	if (innobackupex_mode) {
		ibx_cleanup();
	}

	xb_regex_end();

	free_defaults(client_defaults);
	free_defaults(server_defaults);

	if (THR_THD)
		(void) pthread_key_delete(THR_THD);

	if (THR_THD_initialized) {
		THR_THD_initialized = false;
		(void) my_delete_thread_local_key(THR_THD);
	}

	if (THR_MALLOC_initialized) {
		THR_MALLOC_initialized= false;
		(void) my_delete_thread_local_key(THR_MALLOC);
	}
	msg_ts("completed OK!\n");

	exit(EXIT_SUCCESS);
}
