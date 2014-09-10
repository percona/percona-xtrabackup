/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009-2013 Percona LLC and/or its affiliates
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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

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

#ifndef XTRABACKUP_VERSION
#define XTRABACKUP_VERSION "undefined"
#endif
#ifndef XTRABACKUP_REVISION
#define XTRABACKUP_REVISION "undefined"
#endif

//#define XTRABACKUP_TARGET_IS_PLUGIN

#include <mysql_version.h>
#include <my_base.h>
#include <my_getopt.h>
#include <mysql_com.h>
#if MYSQL_VERSION_ID >= 50600
#include <my_default.h>
#include <mysqld.h>
#endif

#include <fcntl.h>

#ifdef __linux__
# include <sys/prctl.h>
#endif

#include <sys/resource.h>

#define G_PTR uchar*

#include "common.h"
#include "datasink.h"

#include "xb_regex.h"
#include "innodb_int.h"
#include "fil_cur.h"
#include "write_filt.h"
#include "xtrabackup.h"
#include "ds_buffer.h"
#include "ds_tmpfile.h"
#include "xbstream.h"
#include "changed_page_bitmap.h"
#include "read_filt.h"

/* === File name constants === */
#define XB_FN_SUSPENDED_AT_START "xtrabackup_suspended_1"
#define XB_FN_SUSPENDED_AT_END "xtrabackup_suspended_2"
#define XB_FN_LOG_COPIED "xtrabackup_log_copied"

my_bool innodb_inited= 0;

/* === xtrabackup specific options === */
char xtrabackup_real_target_dir[FN_REFLEN] = "./xtrabackup_backupfiles/";
char *xtrabackup_target_dir= xtrabackup_real_target_dir;
my_bool xtrabackup_version = FALSE;
my_bool xtrabackup_backup = FALSE;
my_bool xtrabackup_stats = FALSE;
my_bool xtrabackup_prepare = FALSE;
my_bool xtrabackup_print_param = FALSE;

my_bool xtrabackup_export = FALSE;
my_bool xtrabackup_apply_log_only = FALSE;

static my_bool	xtrabackup_suspend_at_start = FALSE;
my_bool xtrabackup_suspend_at_end = FALSE;
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
xb_page_bitmap *changed_page_bitmap = NULL;

char *xtrabackup_incremental_basedir = NULL; /* for --backup */
char *xtrabackup_extra_lsndir = NULL; /* for --backup with --extra-lsndir */
char *xtrabackup_incremental_dir = NULL; /* for --prepare */

lsn_t xtrabackup_archived_to_lsn = 0; /* for --archived-to-lsn */

char *xtrabackup_tables = NULL;
int tables_regex_num;
xb_regex_t *tables_regex;
xb_regmatch_t tables_regmatch[1];

char *xtrabackup_tables_file = NULL;
hash_table_t* tables_hash;

hash_table_t* inc_dir_tables_hash;

struct xtrabackup_tables_struct{
	char*		name;
	hash_node_t	name_hash;
};
typedef struct xtrabackup_tables_struct	xtrabackup_tables_t;

#ifdef XTRADB_BASED
static ulint		thread_nr[SRV_MAX_N_IO_THREADS + 6 + 64];
static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 6 + 64];
#else
static ulint		thread_nr[SRV_MAX_N_IO_THREADS + 6];
static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 6];
#endif

lsn_t checkpoint_lsn_start;
lsn_t checkpoint_no_start;
lsn_t log_copy_scanned_lsn;
ibool log_copying = TRUE;
ibool log_copying_running = FALSE;

ibool xtrabackup_logfile_is_renamed = FALSE;

int xtrabackup_parallel;

char *xtrabackup_stream_str = NULL;
xb_stream_fmt_t xtrabackup_stream_fmt;
ibool xtrabackup_stream = FALSE;

static const char *xtrabackup_compress_alg = NULL;
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
char metadata_type[30] = ""; /*[full-backuped|full-prepared|incremental]*/
lsn_t metadata_from_lsn = 0;
lsn_t metadata_to_lsn = 0;
lsn_t metadata_last_lsn = 0;

#define XB_LOG_FILENAME "xtrabackup_logfile"

ds_file_t	*dst_log_file = NULL;

/* === some variables from mysqld === */
#if MYSQL_VERSION_ID < 50600
char mysql_real_data_home[FN_REFLEN] = "./";
MY_TMPDIR mysql_tmpdir_list;
char *mysql_data_home= mysql_real_data_home;
#endif
static char mysql_data_home_buff[2];

const char *defaults_group = "mysqld";

/* === static parameters in ha_innodb.cc */

#define HA_INNOBASE_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_INNOBASE_RANGE_COUNT	  100

ulong 	innobase_large_page_size = 0;

/* The default values for the following, type long or longlong, start-up
parameters are declared in mysqld.cc: */

long innobase_additional_mem_pool_size = 1*1024*1024L;
long innobase_buffer_pool_awe_mem_mb = 0;
long innobase_file_io_threads = 4;
long innobase_read_io_threads = 4;
long innobase_write_io_threads = 4;
long innobase_force_recovery = 0;
long innobase_lock_wait_timeout = 50;
long innobase_log_buffer_size = 1024*1024L;
long innobase_log_files_in_group = 2;
long innobase_open_files = 300L;

longlong innobase_page_size = (1LL << 14); /* 16KB */
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
static ulong innobase_log_block_size = 512;
#endif
my_bool innobase_fast_checksum = FALSE;
my_bool	innobase_extra_undoslots = FALSE;
char*	innobase_doublewrite_file = NULL;
char*	innobase_buffer_pool_filename = NULL;

longlong innobase_buffer_pool_size = 8*1024*1024L;
longlong innobase_log_file_size = DEFAULT_LOG_FILE_SIZE;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

char*	innobase_ignored_opt			= NULL;
char*	innobase_data_home_dir			= NULL;
char*	innobase_data_file_path 		= NULL;
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

/* The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL	32
ulong	innobase_active_counter	= 0;

ibool srv_compact_backup = FALSE;
ibool srv_rebuild_indexes = FALSE;

static char *xtrabackup_debug_sync = NULL;

static my_bool xtrabackup_compact = FALSE;
static my_bool xtrabackup_rebuild_indexes = FALSE;

static my_bool xtrabackup_incremental_force_scan = FALSE;

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
		(ut_malloc(sizeof(datafiles_iter_t)));
	it->mutex = OS_MUTEX_CREATE();

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

	os_mutex_enter(it->mutex);

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
	       (it->space->purpose != FIL_TABLESPACE ||
		UT_LIST_GET_LEN(it->space->chain) == 0))
		it->space = UT_LIST_GET_NEXT(space_list, it->space);
	if (it->space == NULL)
		goto end;

	it->node = UT_LIST_GET_FIRST(it->space->chain);

end:
	new_node = it->node;
	os_mutex_exit(it->mutex);

	return new_node;
}

void
datafiles_iter_free(datafiles_iter_t *it)
{
	os_mutex_free(it->mutex);
	ut_free(it);
}

/* ======== Date copying thread context ======== */

typedef struct {
	datafiles_iter_t 	*it;
	uint			num;
	uint			*count;
	os_ib_mutex_t		count_mutex;
	os_thread_id_t		id;
} data_thread_ctxt_t;

/* ======== for option and variables ======== */

enum options_xtrabackup
{
  OPT_XTRA_TARGET_DIR=256,
  OPT_XTRA_BACKUP,
  OPT_XTRA_STATS,
  OPT_XTRA_PREPARE,
  OPT_XTRA_EXPORT,
  OPT_XTRA_APPLY_LOG_ONLY,
  OPT_XTRA_PRINT_PARAM,
  OPT_XTRA_SUSPEND_AT_START,
  OPT_XTRA_SUSPEND_AT_END,
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
#if MYSQL_VERSION_ID >= 50500
  OPT_INNODB_USE_NATIVE_AIO,
#endif
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
  OPT_INNODB_PAGE_SIZE,
#endif
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
  OPT_INNODB_LOG_BLOCK_SIZE,
#endif
#ifdef XTRADB_BASED
  OPT_INNODB_FAST_CHECKSUM,
  OPT_INNODB_EXTRA_UNDOSLOTS,
  OPT_INNODB_DOUBLEWRITE_FILE,
#endif
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
#if MYSQL_VERSION_ID >= 50600
  OPT_INNODB_CHECKSUM_ALGORITHM,
  OPT_INNODB_UNDO_DIRECTORY,
  OPT_UNDO_TABLESPACES,
  OPT_INNODB_LOG_CHECKSUM_ALGORITHM,
#endif
  OPT_XTRA_INCREMENTAL_FORCE_SCAN,
  OPT_DEFAULTS_GROUP,
  OPT_OPEN_FILES_LIMIT,
  OPT_CLOSE_FILES
};

#if MYSQL_VERSION_ID >= 50600
/** Possible values for system variable "innodb_checksum_algorithm". */
static const char* innodb_checksum_algorithm_names[] = {
	"crc32",
	"strict_crc32",
	"innodb",
	"strict_innodb",
	"none",
	"strict_none",
	NullS
};

/** Used to define an enumerate type of the system variable
    innodb_checksum_algorithm. */
static TYPELIB innodb_checksum_algorithm_typelib = {
	array_elements(innodb_checksum_algorithm_names) - 1,
	"innodb_checksum_algorithm_typelib",
	innodb_checksum_algorithm_names,
	NULL
};
#endif

static struct my_option xb_long_options[] =
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
   0, GET_LL, REQUIRED_ARG, 100*1024*1024L, 1024*1024L, LONGLONG_MAX, 0,
   1024*1024L, 0},

  {"suspend-at-start", OPT_XTRA_SUSPEND_AT_START,
   "creates a file '" XB_FN_SUSPENDED_AT_START "' and waits until the user "
   "deletes that file after the background log copying thread is started "
   "during backup",
   (G_PTR*) &xtrabackup_suspend_at_start,
   (G_PTR*) &xtrabackup_suspend_at_start, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},

  {"suspend-at-end", OPT_XTRA_SUSPEND_AT_END, "creates a file '"
   XB_FN_SUSPENDED_AT_END "' and waits until the user deletes that file at "
   "the end of '--backup'",
   (G_PTR*) &xtrabackup_suspend_at_end, (G_PTR*) &xtrabackup_suspend_at_end,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"throttle", OPT_XTRA_THROTTLE, "limit count of IO operations (pairs of read&write) per second to IOS values (for '--backup')",
   (G_PTR*) &xtrabackup_throttle, (G_PTR*) &xtrabackup_throttle,
   0, GET_LONG, REQUIRED_ARG, 0, 0, LONG_MAX, 0, 1, 0},
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
#ifdef UNIV_LOG_ARCHIVE
 {"to-archived-lsn", OPT_XTRA_ARCHIVED_TO_LSN,
   "Don't apply archived logs with bigger log sequence number.",
   (G_PTR*) &xtrabackup_archived_to_lsn, (G_PTR*) &xtrabackup_archived_to_lsn, 0,
   GET_LL, REQUIRED_ARG, 0, 0, LONGLONG_MAX, 0, 0, 0},
#endif /* UNIV_LOG_ARCHIVE */
  {"tables", OPT_XTRA_TABLES, "filtering by regexp for table names.",
   (G_PTR*) &xtrabackup_tables, (G_PTR*) &xtrabackup_tables,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tables_file", OPT_XTRA_TABLES_FILE, "filtering by list of the exact database.table name in the file.",
   (G_PTR*) &xtrabackup_tables_file, (G_PTR*) &xtrabackup_tables_file,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"create-ib-logfile", OPT_XTRA_CREATE_IB_LOGFILE, "** not work for now** creates ib_logfile* also after '--prepare'. ### If you want create ib_logfile*, only re-execute this command in same options. ###",
   (G_PTR*) &xtrabackup_create_ib_logfile, (G_PTR*) &xtrabackup_create_ib_logfile,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

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
   "Number of threads to use for parallel datafiles transfer. Does not have "
   "any effect in the stream mode. The default value is 1.",
   (G_PTR*) &xtrabackup_parallel, (G_PTR*) &xtrabackup_parallel, 0, GET_INT,
   REQUIRED_ARG, 1, 1, INT_MAX, 0, 0, 0},

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
   0, GET_ULL, REQUIRED_ARG, (1 << 16), 1024, ULONGLONG_MAX, 0, 0, 0},

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
   0, GET_ULL, REQUIRED_ARG, (1 << 16), 1024, ULONGLONG_MAX, 0, 0, 0},

   {"log", OPT_LOG, "Ignored option for MySQL option compatibility",
   (G_PTR*) &log_ignored_opt, (G_PTR*) &log_ignored_opt, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

   {"innodb", OPT_INNODB, "Ignored option for MySQL option compatibility",
   (G_PTR*) &innobase_ignored_opt, (G_PTR*) &innobase_ignored_opt, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

  {"innodb_adaptive_hash_index", OPT_INNODB_ADAPTIVE_HASH_INDEX,
   "Enable InnoDB adaptive hash index (enabled by default).  "
   "Disable with --skip-innodb-adaptive-hash-index.",
   (G_PTR*) &innobase_adaptive_hash_index,
   (G_PTR*) &innobase_adaptive_hash_index,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"innodb_additional_mem_pool_size", OPT_INNODB_ADDITIONAL_MEM_POOL_SIZE,
   "Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.",
   (G_PTR*) &innobase_additional_mem_pool_size,
   (G_PTR*) &innobase_additional_mem_pool_size, 0, GET_LONG, REQUIRED_ARG,
   1*1024*1024L, 512*1024L, LONG_MAX, 0, 1024, 0},
  {"innodb_autoextend_increment", OPT_INNODB_AUTOEXTEND_INCREMENT,
   "Data file autoextend increment in megabytes",
   (G_PTR*) &srv_auto_extend_increment,
   (G_PTR*) &srv_auto_extend_increment,
   0, GET_ULONG, REQUIRED_ARG, 8L, 1L, 1000L, 0, 1L, 0},
  {"innodb_buffer_pool_size", OPT_INNODB_BUFFER_POOL_SIZE,
   "The size of the memory buffer InnoDB uses to cache data and indexes of its tables.",
   (G_PTR*) &innobase_buffer_pool_size, (G_PTR*) &innobase_buffer_pool_size, 0,
   GET_LL, REQUIRED_ARG, 8*1024*1024L, 1024*1024L, LONGLONG_MAX, 0,
   1024*1024L, 0},
  {"innodb_checksums", OPT_INNODB_CHECKSUMS, "Enable InnoDB checksums validation (enabled by default). \
Disable with --skip-innodb-checksums.", (G_PTR*) &innobase_use_checksums,
   (G_PTR*) &innobase_use_checksums, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
/*
  {"innodb_commit_concurrency", OPT_INNODB_COMMIT_CONCURRENCY,
   "Helps in performance tuning in heavily concurrent environments.",
   (G_PTR*) &srv_commit_concurrency, (G_PTR*) &srv_commit_concurrency,
   0, GET_ULONG, REQUIRED_ARG, 0, 0, 1000, 0, 1, 0},
*/
/*
  {"innodb_concurrency_tickets", OPT_INNODB_CONCURRENCY_TICKETS,
   "Number of times a thread is allowed to enter InnoDB within the same \
    SQL query after it has once got the ticket",
   (G_PTR*) &srv_n_free_tickets_to_enter,
   (G_PTR*) &srv_n_free_tickets_to_enter,
   0, GET_ULONG, REQUIRED_ARG, 500L, 1L, ULONG_MAX, 0, 1L, 0},
*/
  {"innodb_data_file_path", OPT_INNODB_DATA_FILE_PATH,
   "Path to individual files and their sizes.", (G_PTR*) &innobase_data_file_path,
   (G_PTR*) &innobase_data_file_path, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_data_home_dir", OPT_INNODB_DATA_HOME_DIR,
   "The common part for InnoDB table spaces.", (G_PTR*) &innobase_data_home_dir,
   (G_PTR*) &innobase_data_home_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},
  {"innodb_doublewrite", OPT_INNODB_DOUBLEWRITE, "Enable InnoDB doublewrite buffer (enabled by default). \
Disable with --skip-innodb-doublewrite.", (G_PTR*) &innobase_use_doublewrite,
   (G_PTR*) &innobase_use_doublewrite, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"innodb_io_capacity", OPT_INNODB_IO_CAPACITY,
   "Number of IOPs the server can do. Tunes the background IO rate",
   (G_PTR*) &srv_io_capacity, (G_PTR*) &srv_io_capacity,
   0, GET_ULONG, OPT_ARG, 200, 100, ~0UL, 0, 0, 0},
/*
  {"innodb_fast_shutdown", OPT_INNODB_FAST_SHUTDOWN,
   "Speeds up the shutdown process of the InnoDB storage engine. Possible "
   "values are 0, 1 (faster)"
   " or 2 (fastest - crash-like)"
   ".",
   (G_PTR*) &innobase_fast_shutdown,
   (G_PTR*) &innobase_fast_shutdown, 0, GET_ULONG, OPT_ARG, 1, 0,
   2, 0, 0, 0},
*/
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

  {"innodb_lock_wait_timeout", OPT_INNODB_LOCK_WAIT_TIMEOUT,
   "Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back.",
   (G_PTR*) &innobase_lock_wait_timeout, (G_PTR*) &innobase_lock_wait_timeout,
   0, GET_LONG, REQUIRED_ARG, 50, 1, 1024 * 1024 * 1024, 0, 1, 0},
/*
  {"innodb_locks_unsafe_for_binlog", OPT_INNODB_LOCKS_UNSAFE_FOR_BINLOG,
   "Force InnoDB not to use next-key locking. Instead use only row-level locking",
   (G_PTR*) &innobase_locks_unsafe_for_binlog,
   (G_PTR*) &innobase_locks_unsafe_for_binlog, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
*/
#ifdef UNIV_LOG_ARCHIVE
  {"innodb_log_arch_dir", OPT_INNODB_LOG_ARCH_DIR,
   "Where full logs should be archived.", (G_PTR*) &innobase_log_arch_dir,
   (G_PTR*) &innobase_log_arch_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* UNIV_LOG_ARCHIVE */
  {"innodb_log_buffer_size", OPT_INNODB_LOG_BUFFER_SIZE,
   "The size of the buffer which InnoDB uses to write log to the log files on disk.",
   (G_PTR*) &innobase_log_buffer_size, (G_PTR*) &innobase_log_buffer_size, 0,
   GET_LONG, REQUIRED_ARG, 1024*1024L, 256*1024L, LONG_MAX, 0, 1024, 0},
  {"innodb_log_file_size", OPT_INNODB_LOG_FILE_SIZE,
   "Size of each log file in a log group.",
   (G_PTR*) &innobase_log_file_size, (G_PTR*) &innobase_log_file_size, 0,
   GET_LL, REQUIRED_ARG, DEFAULT_LOG_FILE_SIZE, 1*1024*1024L, LONGLONG_MAX, 0,
   1024*1024L, 0},
  {"innodb_log_files_in_group", OPT_INNODB_LOG_FILES_IN_GROUP,
   "Number of log files in the log group. InnoDB writes to the files in a circular fashion. Value 3 is recommended here.",
   (G_PTR*) &innobase_log_files_in_group, (G_PTR*) &innobase_log_files_in_group,
   0, GET_LONG, REQUIRED_ARG, 2, 2, 100, 0, 1, 0},
  {"innodb_log_group_home_dir", OPT_INNODB_LOG_GROUP_HOME_DIR,
   "Path to InnoDB log files.", (G_PTR*) &INNODB_LOG_DIR,
   (G_PTR*) &INNODB_LOG_DIR, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"innodb_max_dirty_pages_pct", OPT_INNODB_MAX_DIRTY_PAGES_PCT,
   "Percentage of dirty pages allowed in bufferpool.", (G_PTR*) &srv_max_buf_pool_modified_pct,
   (G_PTR*) &srv_max_buf_pool_modified_pct, 0, GET_ULONG, REQUIRED_ARG, 90, 0, 100, 0, 0, 0},
/*
  {"innodb_max_purge_lag", OPT_INNODB_MAX_PURGE_LAG,
   "Desired maximum length of the purge queue (0 = no limit)",
   (G_PTR*) &srv_max_purge_lag,
   (G_PTR*) &srv_max_purge_lag, 0, GET_ULONG, REQUIRED_ARG, 0, 0, ULONG_MAX,
   0, 1L, 0},
*/
/*
  {"innodb_mirrored_log_groups", OPT_INNODB_MIRRORED_LOG_GROUPS,
   "Number of identical copies of log groups we keep for the database. Currently this should be set to 1.",
   (G_PTR*) &innobase_mirrored_log_groups,
   (G_PTR*) &innobase_mirrored_log_groups, 0, GET_LONG, REQUIRED_ARG, 1, 1, 10,
   0, 1, 0},
*/
  {"innodb_open_files", OPT_INNODB_OPEN_FILES,
   "How many files at the maximum InnoDB keeps open at the same time.",
   (G_PTR*) &innobase_open_files, (G_PTR*) &innobase_open_files, 0,
   GET_LONG, REQUIRED_ARG, 300L, 10L, LONG_MAX, 0, 1L, 0},
#if MYSQL_VERSION_ID >= 50500
  {"innodb_use_native_aio", OPT_INNODB_USE_NATIVE_AIO,
   "Use native AIO if supported on this platform.",
   (G_PTR*) &srv_use_native_aio,
   (G_PTR*) &srv_use_native_aio, 0, GET_BOOL, NO_ARG,
   FALSE, 0, 0, 0, 0, 0},
#endif
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
  {"innodb_page_size", OPT_INNODB_PAGE_SIZE,
   "The universal page size of the database.",
   (G_PTR*) &innobase_page_size, (G_PTR*) &innobase_page_size, 0,
   /* Use GET_LL to support numeric suffixes in 5.6 */
   GET_LL, REQUIRED_ARG,
   (1LL << 14), (1LL << 12), (1LL << UNIV_PAGE_SIZE_SHIFT_MAX), 0, 1L, 0},
#endif
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
  {"innodb_log_block_size", OPT_INNODB_LOG_BLOCK_SIZE,
  "The log block size of the transaction log file. "
   "Changing for created log file is not supported. Use on your own risk!",
   (G_PTR*) &innobase_log_block_size, (G_PTR*) &innobase_log_block_size, 0,
   GET_ULONG, REQUIRED_ARG, 512, 512, 1 << UNIV_PAGE_SIZE_SHIFT_MAX, 0, 1L, 0},
#endif
#ifdef XTRADB_BASED
  {"innodb_fast_checksum", OPT_INNODB_FAST_CHECKSUM,
   "Change the algorithm of checksum for the whole of datapage to 4-bytes word based.",
   (G_PTR*) &innobase_fast_checksum,
   (G_PTR*) &innobase_fast_checksum, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_extra_undoslots", OPT_INNODB_EXTRA_UNDOSLOTS,
   "Enable to use about 4000 undo slots instead of default 1024. Not recommended to use, "
   "Because it is not change back to disable, once it is used.",
   (G_PTR*) &innobase_extra_undoslots, (G_PTR*) &innobase_extra_undoslots,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_doublewrite_file", OPT_INNODB_DOUBLEWRITE_FILE,
   "Path to special datafile for doublewrite buffer. (default is "": not used)",
   (G_PTR*) &innobase_doublewrite_file, (G_PTR*) &innobase_doublewrite_file,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
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

  {"compact", OPT_XTRA_COMPACT,
   "Create a compact backup by skipping secondary index pages.",
   (G_PTR*) &xtrabackup_compact, (G_PTR*) &xtrabackup_compact,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"rebuild_indexes", OPT_XTRA_REBUILD_INDEXES,
   "Rebuild secondary indexes in InnoDB tables after applying the log. "
   "Only has effect with --prepare.",
   (G_PTR*) &xtrabackup_rebuild_indexes, (G_PTR*) &xtrabackup_rebuild_indexes,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"rebuild_threads", OPT_XTRA_REBUILD_INDEXES,
   "Use this number of threads to rebuild indexes in a compact backup. "
   "Only has effect with --prepare and --rebuild-indexes.",
   (G_PTR*) &xtrabackup_rebuild_threads, (G_PTR*) &xtrabackup_rebuild_threads,
   0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

#if MYSQL_VERSION_ID >= 50600
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
   (G_PTR*) &srv_undo_dir, (G_PTR*) &srv_undo_dir,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_undo_tablespaces", OPT_UNDO_TABLESPACES,
   "Number of undo tablespaces to use.",
   (G_PTR*)&srv_undo_tablespaces, (G_PTR*)&srv_undo_tablespaces,
   0, GET_ULONG, REQUIRED_ARG, 0, 0, 126, 0, 1, 0},
#endif

  {"incremental-force-scan", OPT_XTRA_INCREMENTAL_FORCE_SCAN,
   "Perform a full-scan incremental backup even in the presence of changed "
   "page bitmap data",
   (G_PTR*)&xtrabackup_incremental_force_scan,
   (G_PTR*)&xtrabackup_incremental_force_scan, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},

  {"defaults_group", OPT_DEFAULTS_GROUP, "defaults group in config file (default \"mysqld\").",
   (G_PTR*) &defaults_group, (G_PTR*) &defaults_group,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"open_files_limit", OPT_OPEN_FILES_LIMIT, "the maximum number of file "
   "descriptors to reserve with setrlimit().",
   (G_PTR*) &xb_open_files_limit, (G_PTR*) &xb_open_files_limit, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, UINT_MAX, 0, 1, 0},

  {"close_files", OPT_CLOSE_FILES, "do not keep files opened. Use at your own "
   "risk.", (G_PTR*) &xb_close_files, (G_PTR*) &xb_close_files, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

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

static const char *xb_load_default_groups[]= { "mysqld", "xtrabackup", 0, 0 };

static void print_version(void)
{
#ifdef XTRADB_BASED
  msg("%s version %s for Percona Server %s %s (%s) (revision id: %s)\n",
      my_progname, XTRABACKUP_VERSION, MYSQL_SERVER_VERSION, SYSTEM_TYPE,
      MACHINE_TYPE, XTRABACKUP_REVISION);
#else
  msg("%s version %s for MySQL server %s %s (%s) (revision id: %s)\n",
      my_progname, XTRABACKUP_VERSION, MYSQL_SERVER_VERSION, SYSTEM_TYPE,
      MACHINE_TYPE, XTRABACKUP_REVISION);
#endif
}

static void usage(void)
{
  puts("Open source backup tool for InnoDB and XtraDB\n\
\n\
Copyright (C) 2009-2013 Percona LLC and/or its affiliates.\n\
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
  print_defaults("my", xb_load_default_groups);
  my_print_help(xb_long_options);
  my_print_variables(xb_long_options);
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case 'h':
    strmake(mysql_real_data_home,argument, FN_REFLEN - 1);
    mysql_data_home= mysql_real_data_home;
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
Initializes log_block_size*/
static
ibool
xb_init_log_block_size(void)
{
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
	srv_log_block_size = 0;
	if (innobase_log_block_size != 512) {
		uint	n_shift = get_bit_shift(innobase_log_block_size);;

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
#endif
	return TRUE;
}

static my_bool
innodb_init_param(void)
{
	/* innobase_init */
	static char	current_dir[3];		/* Set if using current lib */
	my_bool		ret;
	char		*default_path;

	/* === some variables from mysqld === */
	memset((G_PTR) &mysql_tmpdir_list, 0, sizeof(mysql_tmpdir_list));

	if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
		exit(EXIT_FAILURE);

	/* dummy for initialize all_charsets[] */
	get_charset_name(0);

#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
	srv_page_size = 0;
	srv_page_size_shift = 0;

	if (innobase_page_size != (1LL << 14)) {
		int n_shift = get_bit_shift((ulint) innobase_page_size);

		if (n_shift >= 12 && n_shift <= UNIV_PAGE_SIZE_SHIFT_MAX) {
			srv_page_size_shift = n_shift;
			srv_page_size = 1 << n_shift;
			msg("InnoDB: The universal page size of the "
			    "database is set to %lu.\n", srv_page_size);
		} else {
			msg("InnoDB: Error: invalid value of "
			    "innobase_page_size: %lld", innobase_page_size);
			exit(EXIT_FAILURE);
		}
	} else {
		srv_page_size_shift = 14;
		srv_page_size = (1 << srv_page_size_shift);
	}

	if (!xb_init_log_block_size()) {
		goto error;
	}
#endif

#ifdef XTRADB_BASED
	srv_fast_checksum = (ibool) innobase_fast_checksum;
#endif

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

	/* First calculate the default path for innodb_data_home_dir etc.,
	in case the user has not given any value.

	Note that when using the embedded server, the datadirectory is not
	necessarily the current directory of this program. */

	  	/* It's better to use current lib, to keep paths short */
	  	current_dir[0] = FN_CURLIB;
	  	current_dir[1] = FN_LIBCHAR;
	  	current_dir[2] = 0;
	  	default_path = current_dir;

	ut_a(default_path);

#if (MYSQL_VERSION_ID < 50500)
//	if (specialflag & SPECIAL_NO_PRIOR) {
	        srv_set_thread_priorities = FALSE;
//	} else {
//	        srv_set_thread_priorities = TRUE;
//	        srv_query_thread_priority = QUERY_PRIOR;
//	}
#endif

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

	/* Set default InnoDB data file size to 10 MB and let it be
  	auto-extending. Thus users can use InnoDB in >= 4.0 without having
	to specify any startup options. */

	if (!innobase_data_file_path) {
  		innobase_data_file_path = (char*) "ibdata1:10M:autoextend";
	}
	msg("xtrabackup:   innodb_data_file_path = %s\n",
	    innobase_data_file_path);

	/* Since InnoDB edits the argument in the next call, we make another
	copy of it: */

	internal_innobase_data_file_path = strdup(innobase_data_file_path);

	ret = (my_bool) srv_parse_data_file_paths_and_sizes(
			internal_innobase_data_file_path);
	if (ret == FALSE) {
	  	msg("xtrabackup: syntax error in innodb_data_file_path\n");
mem_free_and_error:
	  	free(internal_innobase_data_file_path);
                goto error;
	}

	if (xtrabackup_prepare) {
		/* "--prepare" needs filenames only */
		ulint i;

		for (i=0; i < srv_n_data_files; i++) {
			char *p;

			p = srv_data_file_names[i];
			while ((p = strchr(p, SRV_PATH_SEPARATOR)) != NULL)
			{
				p++;
				srv_data_file_names[i] = p;
			}
		}
	}

#ifdef XTRADB_BASED
	srv_doublewrite_file = innobase_doublewrite_file;
#ifndef XTRADB55
	srv_extra_undoslots = (ibool) innobase_extra_undoslots;
#endif
#endif

	/* -------------- Log files ---------------------------*/

	/* The default dir for log files is the datadir of MySQL */

	if (!((xtrabackup_backup || xtrabackup_stats) && INNODB_LOG_DIR)) {
		INNODB_LOG_DIR = default_path;
	}
	if (xtrabackup_prepare && xtrabackup_incremental_dir) {
		INNODB_LOG_DIR = xtrabackup_incremental_dir;
	}
	msg("xtrabackup:   innodb_log_group_home_dir = %s\n", INNODB_LOG_DIR);

	ret = (my_bool) xb_parse_log_group_home_dirs();
	if (ret == FALSE) {

		goto mem_free_and_error;
	}

	srv_adaptive_flushing = FALSE;
	srv_use_sys_malloc = TRUE;
	srv_file_format = 1; /* Barracuda */
#if (MYSQL_VERSION_ID < 50500)
	srv_check_file_format_at_startup = UNIV_FORMAT_MIN; /* on */
#else
	srv_max_file_format_at_startup = UNIV_FORMAT_MIN; /* on */
#endif
	/* --------------------------------------------------*/

	srv_file_flush_method_str = innobase_unix_file_flush_method;

	srv_n_log_files = (ulint) innobase_log_files_in_group;
	srv_log_file_size = (ulint) innobase_log_file_size;
	msg("xtrabackup:   innodb_log_files_in_group = %ld\n",
	    srv_n_log_files);
	msg("xtrabackup:   innodb_log_file_size = %lld\n",
	    (long long) srv_log_file_size);

#ifdef UNIV_LOG_ARCHIVE
	srv_log_archive_on = (ulint) innobase_log_archive;
#endif /* UNIV_LOG_ARCHIVE */
	srv_log_buffer_size = (ulint) innobase_log_buffer_size;

        /* We set srv_pool_size here in units of 1 kB. InnoDB internally
        changes the value so that it becomes the number of database pages. */

	//srv_buf_pool_size = (ulint) innobase_buffer_pool_size;
	srv_buf_pool_size = (ulint) xtrabackup_use_memory;

	srv_mem_pool_size = (ulint) innobase_additional_mem_pool_size;

	srv_n_file_io_threads = (ulint) innobase_file_io_threads;
	srv_n_read_io_threads = (ulint) innobase_read_io_threads;
	srv_n_write_io_threads = (ulint) innobase_write_io_threads;

	srv_force_recovery = (ulint) innobase_force_recovery;

	srv_use_doublewrite_buf = (ibool) innobase_use_doublewrite;
#if MYSQL_VERSION_ID < 50600
	srv_use_checksums = (ibool) innobase_use_checksums;
#endif

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

	ut_a(DATA_MYSQL_LATIN1_SWEDISH_CHARSET_COLL ==
					my_charset_latin1.number);
	ut_a(DATA_MYSQL_BINARY_CHARSET_COLL == my_charset_bin.number);

	/* Store the latin1_swedish_ci character ordering table to InnoDB. For
	non-latin1_swedish_ci charsets we use the MySQL comparison functions,
	and consequently we do not need to know the ordering internally in
	InnoDB. */

	ut_a(0 == strcmp(my_charset_latin1.name, "latin1_swedish_ci"));
	srv_latin1_ordering = my_charset_latin1.sort_order;

	//innobase_commit_concurrency_init_default();

	/* Since we in this module access directly the fields of a trx
        struct, and due to different headers and flags it might happen that
	mutex_t has a different size in this module and in InnoDB
	modules, we check at run time that the size is the same in
	these compilation modules. */

#if MYSQL_VERSION_ID >= 50500
	/* On 5.5 srv_use_native_aio is TRUE by default. It is later reset
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
#endif /* MYSQL_VERSION_ID */

#if MYSQL_VERSION_ID >= 50600
	/* Assign the default value to srv_undo_dir if it's not specified, as
	my_getopt does not support default values for string options. We also
	ignore the option and override innodb_undo_directory on --prepare,
	because separate undo tablespaces are copied to the root backup
	directory. */

	if (!srv_undo_dir || !xtrabackup_backup) {
		srv_undo_dir = (char *) ".";
	}
#endif

#ifdef XTRADB_BASED
	/* Workaround for LP #1203308. */
	srv_track_changed_pages = FALSE;
#endif

#if MYSQL_VERSION_ID >= 50600
	innodb_log_checksum_func_update(srv_log_checksum_algorithm);
#endif
	return(FALSE);

error:
	msg("xtrabackup: innodb_init_param(): Error occured.\n");
	return(TRUE);
}

static my_bool
innodb_init(void)
{
	int	err;

#if MYSQL_VERSION_ID < 50500
	/* InnoDB relies on buf_LRU_old_ratio to be always initialized (see
	debug assertions in buf_LRU_old_adjust_len(). Normally it is initialized
	from ha_innodb.cc. That code is not used in XtraBackup, which led to
	assertion failures in 5.1 debug builds (see LP bug #924492).

	5.5 initializes that variable to a hard-coded value of 37% (and then
	adjusting it to the actual server variable value). That's why the
	assertion failures never occurred on 5.5. Let's do the same for 5.1. */

	buf_LRU_old_ratio_update(100 * 3 / 8, FALSE);
#endif

	err = innobase_start_or_create_for_mysql();

	if (err != DB_SUCCESS) {
	  	free(internal_innobase_data_file_path);
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
	/* Use UINT64PF instead of LSN_PF here, as we have to maintain the file
	format. */
	if (fscanf(fp, "from_lsn = " UINT64PF "\n", &metadata_from_lsn)
			!= 1) {
		r = FALSE;
		goto end;
	}
	if (fscanf(fp, "to_lsn = " UINT64PF "\n", &metadata_to_lsn)
			!= 1) {
		r = FALSE;
		goto end;
	}
	if (fscanf(fp, "last_lsn = " UINT64PF "\n", &metadata_last_lsn)
			!= 1) {
		metadata_last_lsn = 0;
	}
	/* Optional field */
	if (fscanf(fp, "compact = %d\n", &t) == 1) {
		xtrabackup_compact = (t == 1);
	} else {
		xtrabackup_compact = 0;
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
	/* Use UINT64PF instead of LSN_PF here, as we have to maintain the file
	format. */
	snprintf(buf, buf_len,
		 "backup_type = %s\n"
		 "from_lsn = " UINT64PF "\n"
		 "to_lsn = " UINT64PF "\n"
		 "last_lsn = " UINT64PF "\n"
		 "compact = %d\n",
		 metadata_type,
		 metadata_from_lsn,
		 metadata_to_lsn,
		 metadata_last_lsn,
		 xtrabackup_compact == TRUE);
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
		ds_close(stream);
		return(FALSE);
	}

	ds_close(stream);

	return(TRUE);
}

/***********************************************************************
Write backup meta info to a specified file.
@return TRUE on success, FALSE on failure. */
static
my_bool
xtrabackup_write_metadata(const char *filepath)
{
	char		buf[1024];
	size_t		len;
	FILE		*fp;

	xtrabackup_print_metadata(buf, sizeof(buf));

	len = strlen(buf);

	fp = fopen(filepath, "w");
	if(!fp) {
		msg("xtrabackup: Error: cannot open %s\n", filepath);
		return(FALSE);
	}
	if (fwrite(buf, len, 1, fp) < 1) {
		fclose(fp);
		return(FALSE);
	}

	fclose(fp);

	return(TRUE);
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
	char		buf[64];
	my_bool		ret;
	size_t		len;
	MY_STAT		mystat;

	snprintf(buf, sizeof(buf),
		 "page_size = %lu\n"
		 "zip_size = %lu\n"
		 "space_id = %lu\n",
		 info->page_size, info->zip_size, info->space_id);
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

	ds_close(f);

	return(ret);
}

/* ================= backup ================= */
void
xtrabackup_io_throttling(void)
{
	if (xtrabackup_throttle && (io_ticket--) < 0) {
		os_event_reset(wait_throttle);
		os_event_wait(wait_throttle);
	}
}

/************************************************************************
Checks if a given table name matches any of specifications in the --tables or
--tables-file options.

@return TRUE on match. */
static my_bool
check_if_table_matches_filters(const char *name)
{
	int 			regres;
	int 			i;
	xtrabackup_tables_t*	table;

	if (xtrabackup_tables) {

		regres = REG_NOMATCH;

		for (i = 0; i < tables_regex_num; i++) {
			regres = xb_regexec(&tables_regex[i], name, 1,
					    tables_regmatch, 0);
			if (regres != REG_NOMATCH) {

				return(TRUE);
			}
		}
	}

	if (xtrabackup_tables_file) {

		XB_HASH_SEARCH(name_hash, tables_hash, ut_fold_string(name),
			       table, (void) 0,
			       !strcmp(table->name, name));
		if (table) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/************************************************************************
Checks if a table specified as a name in the form "database/name" (InnoDB 5.6)
or "./database/name.ibd" (InnoDB 5.5-) should be skipped from backup based on
the --tables or --tables-file options.

@return TRUE if the table should be skipped. */
static
my_bool
check_if_skip_table(
/******************/
	const char*	name)	/*!< in: path to the table */
{
	char buf[FN_REFLEN];
	const char *dbname, *tbname;
	const char *ptr;
	char *eptr;

	if (xtrabackup_tables == NULL && xtrabackup_tables_file == NULL) {
		return(FALSE);
	}

	dbname = NULL;
	tbname = name;
	while ((ptr = strchr(tbname, SRV_PATH_SEPARATOR)) != NULL) {
		dbname = tbname;
		tbname = ptr + 1;
	}

	if (dbname == NULL) {
		return(FALSE);
	}

	strncpy(buf, dbname, FN_REFLEN);
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
	if (check_if_table_matches_filters(buf)) {

		return(FALSE);
	}
	if ((eptr = strstr(buf, "#P#")) != NULL) {

		*eptr = 0;

		if (check_if_table_matches_filters(buf)) {

			return(FALSE);
		}
	}

	return(TRUE);
}

/***********************************************************************
Reads the space flags from a given data file and returns the compressed
page size, or 0 if the space is not compressed. */
ulint
xb_get_zip_size(os_file_t file)
{
	byte	*buf;
	byte	*page;
	ulint	 zip_size = ULINT_UNDEFINED;
	ibool	 success;
	ulint	 space;

	buf = static_cast<byte *>(ut_malloc(2 * UNIV_PAGE_SIZE_MAX));
	page = static_cast<byte *>(ut_align(buf, UNIV_PAGE_SIZE_MAX));

	success = xb_os_file_read(file, page, 0, UNIV_PAGE_SIZE_MAX);
	if (!success) {
		goto end;
	}

	space = mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
	zip_size = (space == 0 ) ? 0 :
		dict_tf_get_zip_size(fsp_header_get_flags(page));
end:
	ut_free(buf);

	return(zip_size);
}

/**********************************************************************//**
Closes a file. */
static
void
xb_fil_node_close_file(
/*===================*/
	fil_node_t*	node)	/*!< in: file node */
{
	ibool	ret;

	mutex_enter(&fil_system->mutex);

	ut_ad(node);
	ut_a(node->n_pending == 0);
	ut_a(node->n_pending_flushes == 0);
#if MYSQL_VERSION_ID >= 50600
	ut_a(!node->being_extended);
#endif

	if (!node->open) {

		mutex_exit(&fil_system->mutex);

		return;
	}

	ret = os_file_close(node->handle);
	ut_a(ret);

	node->open = FALSE;

	ut_a(fil_system->n_open > 0);
	fil_system->n_open--;
#if MYSQL_VERSION_ID >= 50600
	fil_n_file_opened--;
#endif

	if (node->space->purpose == FIL_TABLESPACE &&
	    !trx_sys_sys_space(node->space->id)) {

		ut_a(UT_LIST_GET_LEN(fil_system->LRU) > 0);

		/* The node is in the LRU list, remove it */
		UT_LIST_REMOVE(LRU, fil_system->LRU, node);
	}

	mutex_exit(&fil_system->mutex);
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

	is_system = trx_sys_sys_space(node->space->id);

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

	strncpy(dst_name, cursor.rel_path, sizeof(dst_name));

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
		msg("[%02u] %s %s\n", thread_n, action, node_path);
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
			action = "Copying";
		}
		msg("[%02u] %s %s to %s\n", thread_n, action,
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
	msg("[%02u]        ...done\n", thread_n);
	xb_fil_cur_close(&cursor);
	ds_close(dstfile);
	if (write_filter && write_filter->deinit) {
		write_filter->deinit(&write_filt_ctxt);
	}
	return(FALSE);

error:
	xb_fil_cur_close(&cursor);
	if (dstfile != NULL) {
		ds_close(dstfile);
	}
	if (write_filter && write_filter->deinit) {
		write_filter->deinit(&write_filt_ctxt);;
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

static my_bool
xtrabackup_copy_logfile(lsn_t from_lsn, my_bool is_last)
{
	/* definition from recv_recovery_from_checkpoint_start() */
	log_group_t*	group;
	lsn_t		group_scanned_lsn;
	lsn_t		contiguous_lsn;

	ut_a(dst_log_file != NULL);

	/* read from checkpoint_lsn_start to current */
	contiguous_lsn = ut_dulint_align_down(from_lsn,
						OS_FILE_LOG_BLOCK_SIZE);

	/* TODO: We must check the contiguous_lsn still exists in log file.. */

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		ibool	finished;
		lsn_t	start_lsn;
		lsn_t	end_lsn;

		/* reference recv_group_scan_log_recs() */
	finished = FALSE;

	start_lsn = contiguous_lsn;

	while (!finished) {
		end_lsn = ut_dulint_add(start_lsn, RECV_SCAN_SIZE);

		xtrabackup_io_throttling();

		mutex_enter(&log_sys->mutex);

		xb_log_group_read_log_seg(LOG_RECOVER, log_sys->buf,
					  group, start_lsn, end_lsn);


		/* reference recv_scan_log_recs() */
		{
	byte*	log_block;
	lsn_t	scanned_lsn;
	ulint	data_len;

	ulint	scanned_checkpoint_no = 0;

	finished = FALSE;

	log_block = log_sys->buf;
	scanned_lsn = start_lsn;

	while (log_block < log_sys->buf + RECV_SCAN_SIZE && !finished) {
		ulint	no = log_block_get_hdr_no(log_block);
		ulint	scanned_no = log_block_convert_lsn_to_no(scanned_lsn);
		ibool	checksum_is_ok =
			log_block_checksum_is_ok_or_old_format(log_block);

		if (no != scanned_no && checksum_is_ok) {
			ulint blocks_in_group;

			blocks_in_group = log_block_convert_lsn_to_no(
				log_group_get_capacity(group)) - 1;

			if (no < scanned_no ||
			    /* Log block numbers wrap around at 0x3FFFFFFF */
			    ((scanned_no | 0x4000000UL) - no) %
			    blocks_in_group == 0) {

				/* old log block, do nothing */
				finished = TRUE;

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

			goto error;
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
			finished = TRUE;
			break;
		}

		if (log_block_get_flush_bit(log_block)) {
			/* This block was a start of a log flush operation:
			we know that the previous flush operation must have
			been completed for all log groups before this block
			can have been flushed to any of the groups. Therefore,
			we know that log data is contiguous up to scanned_lsn
			in all non-corrupt log groups. */

			if (ut_dulint_cmp(scanned_lsn, contiguous_lsn) > 0) {
				contiguous_lsn = scanned_lsn;
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

			finished = TRUE;
			break;
		}

		scanned_lsn = ut_dulint_add(scanned_lsn, data_len);
		scanned_checkpoint_no = log_block_get_checkpoint_no(log_block);

		if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
			/* Log data for this group ends here */

			finished = TRUE;
		} else {
			log_block += OS_FILE_LOG_BLOCK_SIZE;
		}
	} /* while (log_block < log_sys->buf + RECV_SCAN_SIZE && !finished) */

	group_scanned_lsn = scanned_lsn;



		}

		/* ===== write log to 'xtrabackup_logfile' ====== */
		{
		ulint 	write_size;

		if (!finished) {
			write_size = RECV_SCAN_SIZE;
		} else {
			write_size = (ulint) ut_dulint_minus(
					ut_dulint_align_up(group_scanned_lsn, OS_FILE_LOG_BLOCK_SIZE),
					start_lsn);
			if (!is_last &&
			    group_scanned_lsn % OS_FILE_LOG_BLOCK_SIZE
			    )
				write_size -= OS_FILE_LOG_BLOCK_SIZE;
		}


		if (ds_write(dst_log_file, log_sys->buf, write_size)) {
			msg("xtrabackup: Error: write to logfile failed\n");
			goto error;
		}


		}

		mutex_exit(&log_sys->mutex);

		start_lsn = end_lsn;
	}



		group->scanned_lsn = group_scanned_lsn;

		msg(">> log scanned up to (" LSN_PF ")\n",
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
		xb_event_wait_time(log_copying_stop,
				xtrabackup_log_copy_interval * 1000ULL);
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
	os_thread_exit(NULL);

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

	while (log_copying) {
		os_thread_sleep(1000000); /*1 sec*/

		//for DEBUG
		//if (io_ticket == xtrabackup_throttle) {
		//	msg("There seem to be no IO...?\n");
		//}

		io_ticket = xtrabackup_throttle;
		os_event_set(wait_throttle);
	}

	/* stop io throttle */
	xtrabackup_throttle = 0;
	os_event_set(wait_throttle);

	os_thread_exit(NULL);

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

	os_thread_exit(NULL);

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

	while ((node = datafiles_iter_next(ctxt->it)) != NULL) {

		/* copy the datafile */
		if(xtrabackup_copy_datafile(node, num)) {
			msg("[%02u] xtrabackup: Error: "
			    "failed to copy datafile.\n", num);
			exit(EXIT_FAILURE);
		}
	}

	os_mutex_enter(ctxt->count_mutex);
	(*ctxt->count)--;
	os_mutex_exit(ctxt->count_mutex);

	my_thread_end();
	os_thread_exit(NULL);
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
		ds_data = ds_meta = ds_create(xtrabackup_target_dir,
					      DS_TYPE_STDOUT);
	} else {
		/* Local filesystem */
		ds_data = ds_meta = ds_create(xtrabackup_target_dir,
					      DS_TYPE_LOCAL);
	}

	/* Track it for destruction */
	xtrabackup_add_datasink(ds_data);

	/* Encryption always done just before final output */
	if (xtrabackup_encrypt) {
		ds_ctxt_t	*ds;

		ds = ds_create(xtrabackup_target_dir, DS_TYPE_ENCRYPT);
		xtrabackup_add_datasink(ds);

		ds_set_pipe(ds, ds_data);
		ds_data = ds_meta = ds;
	}

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

		if (xtrabackup_stream_fmt != XB_STREAM_FMT_XBSTREAM ||
		    xtrabackup_suspend_at_end ||
		    xtrabackup_suspend_at_start) {

			/* 'xbstream' allow parallel streams, but we
			still can't stream directly to stdout when
			xtrabackup is invoked from innobackupex
			(i.e. with --suspend_at_and), because
			innobackupex and xtrabackup streams would
			interfere. Use temporary files instead. */
			ds_meta = ds_create(xtrabackup_target_dir, DS_TYPE_TMPFILE);
			xtrabackup_add_datasink(ds_meta);
			ds_set_pipe(ds_meta, ds);
		} else {
			ds_meta = ds_data;
		}
	}

	/* Compression for ds_data */
	if (xtrabackup_compress) {
		ds_ctxt_t	*ds;

		/* Use a 1 MB buffer for compressed output stream */
		ds = ds_create(xtrabackup_target_dir, DS_TYPE_BUFFER);
		ds_buffer_set_size(ds, 1024 * 1024);
		xtrabackup_add_datasink(ds);
		ds_set_pipe(ds, ds_data);
		ds_data = ds;

		ds = ds_create(xtrabackup_target_dir, DS_TYPE_COMPRESS);
		xtrabackup_add_datasink(ds);
		ds_set_pipe(ds, ds_data);
		ds_data = ds;
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
}

#define SRV_N_PENDING_IOS_PER_THREAD 	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100

/************************************************************************
@return TRUE if table should be opened. */
static
ibool
xb_check_if_open_tablespace(
	const char*	db,
	const char*	table)
{
	char buf[FN_REFLEN];

	snprintf(buf, sizeof(buf), "%s%s/%s", xb_dict_prefix, db, table);

	return !check_if_skip_table(buf);
}

/************************************************************************
Initializes the I/O and tablespace cache subsystems. */
static
void
xb_fil_io_init(void)
/*================*/
{
#if MYSQL_VERSION_ID >= 50600
	srv_n_file_io_threads = srv_n_read_io_threads;
#else
	srv_n_file_io_threads = 2 + srv_n_read_io_threads +
		srv_n_write_io_threads;
#endif

	os_aio_init(8 * SRV_N_PENDING_IOS_PER_THREAD,
		    srv_n_read_io_threads,
		    srv_n_write_io_threads,
		    SRV_MAX_N_PENDING_SYNC_IOS);

	fil_init(srv_file_per_table ? 50000 : 5000, LONG_MAX);

	fsp_init();
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
	ibool	create_new_db;
#ifdef XTRADB_BASED
	ibool	create_new_doublewrite_file;
#endif
	ulint	err;
	ulint   sum_of_new_sizes;

	for (i = 0; i < srv_n_file_io_threads; i++) {
		thread_nr[i] = i;

		os_thread_create(io_handler_thread, thread_nr + i,
				 thread_ids + i);
    	}

	os_thread_sleep(200000); /*0.2 sec*/

	err = open_or_create_data_files(&create_new_db,
#ifdef XTRADB_BASED
					&create_new_doublewrite_file,
#endif
					&min_flushed_lsn, &max_flushed_lsn,
					&sum_of_new_sizes);
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

	/* create_new_db must not be TRUE.. */
	if (create_new_db) {
		msg("xtrabackup: could not find data files at the "
		    "specified datadir\n");
		return(DB_ERROR);
	}

#if MYSQL_VERSION_ID >= 50600
	/* Add separate undo tablespaces to fil_system */

	err = srv_undo_tablespaces_init(FALSE,
					TRUE,
					srv_undo_tablespaces,
					&srv_undo_tablespaces_open);
	if (err != DB_SUCCESS) {
		return(err);
	}
#endif

	/* It is important to call fil_load_single_table_tablespace() after
	srv_undo_tablespaces_init(), because fil_is_user_tablespace_id() *
	relies on srv_undo_tablespaces_open to be properly initialized */

	err = fil_load_single_table_tablespaces(xb_check_if_open_tablespace);
	if (err != DB_SUCCESS) {
		return(err);
	}

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

	/* Shutdown the aio threads. This has been copied from
	innobase_shutdown_for_mysql(). */

	srv_shutdown_state = SRV_SHUTDOWN_EXIT_THREADS;

	for (i = 0; i < 1000; i++) {
		os_aio_wake_all_threads_at_shutdown();

		os_mutex_enter(os_sync_mutex);

		if (os_thread_count == 0) {

			os_mutex_exit(os_sync_mutex);

			os_thread_sleep(10000);

			break;
		}

		os_mutex_exit(os_sync_mutex);

		os_thread_sleep(10000);
	}

	if (i == 1000) {
		msg("xtrabackup: Warning: %lu threads created by InnoDB"
		    " had not exited at shutdown!\n",
		    (ulong) os_thread_count);
	}

	os_aio_free();

	fil_close_all_files();
	fil_system = NULL;

	/* Reset srv_file_io_threads to its default value to avoid confusing
	warning on --prepare in innobase_start_or_create_for_mysql()*/
	srv_n_file_io_threads = 4;

	srv_shutdown_state = SRV_SHUTDOWN_NONE;
}

/***********************************************************************
Prepare a sync file path. */
static void
xb_make_sync_file_name(
/*===================*/
	const char*	name,	/*!<in: sync file name */
	char*		path)	/*!<in/out: path to the sync file */
{
	snprintf(path, FN_REFLEN, "%s/%s", xtrabackup_target_dir, name);
	srv_normalize_path_for_win(path);
}

/***********************************************************************
Create an empty file with a given path and close it.
@return TRUE on succees, FALSE on error. */
static ibool
xb_create_sync_file(
/*================*/
	const char*	path)	/*!<in: path to the sync file */
{
	File	suspend_file;
	pid_t	pid;
	char	buffer[64];

	pid = getpid();
	snprintf(buffer, sizeof(buffer), "%u", (uint) pid);

	msg("xtrabackup: Creating suspend file '%s' with pid '%u'\n",
	    path, (uint) pid);

	suspend_file = my_create(path, 0, O_WRONLY | O_EXCL | O_NOFOLLOW,
				 MYF(MY_WME));

	if (suspend_file >= 0) {
		ibool rc;

		rc = my_write(suspend_file, (uchar *) buffer, strlen(buffer),
			      MYF(MY_WME | MY_NABP)) == 0;

		my_close(suspend_file, MYF(MY_WME));
		return(rc);
	}

	msg("xtrabackup: Error: failed to create file '%s'\n",
	    path);
	return(FALSE);
}

static
void
xb_filters_init()
{
	if (xtrabackup_tables) {
		/* init regexp */
		char *p, *next;
		int i;
		char errbuf[100];

		tables_regex_num = 1;

		p = xtrabackup_tables;
		while ((p = strchr(p, ',')) != NULL) {
			p++;
			tables_regex_num++;
		}

		tables_regex = static_cast<xb_regex_t *>
			(ut_malloc(sizeof(xb_regex_t) * tables_regex_num));

		p = xtrabackup_tables;
		for (i=0; i < tables_regex_num; i++) {
			int	rc;

			next = strchr(p, ',');
			ut_a(next || i == tables_regex_num - 1);

			next++;
			if (i != tables_regex_num - 1)
				*(next - 1) = '\0';

			rc = xb_regcomp(&tables_regex[i], p,
					REG_EXTENDED);

			if (rc) {

				xb_regerror(rc, &tables_regex[i], errbuf,
					    sizeof(errbuf));
				msg("xtrabackup: regcomp(%s) failed: %s\n",
				    p, errbuf);

				exit(EXIT_FAILURE);
			}

			if (i != tables_regex_num - 1)
				*(next - 1) = ',';
			p = next;
		}
	}

	if (xtrabackup_tables_file) {
		char name_buf[NAME_LEN*2+2];
		FILE *fp;

		name_buf[NAME_LEN*2+1] = '\0';

		/* init tables_hash */
		tables_hash = hash_create(1000);

		/* read and store the filenames */
		fp = fopen(xtrabackup_tables_file,"r");
		if (!fp) {
			msg("xtrabackup: cannot open %s\n",
			    xtrabackup_tables_file);
			exit(EXIT_FAILURE);
		}
		for (;;) {
			xtrabackup_tables_t*	table;
			char*	p = name_buf;

			if ( fgets(name_buf, NAME_LEN*2+1, fp) == 0 ) {
				break;
			}

			p = strchr(name_buf, '\n');
			if (p)
			{
				*p = '\0';
			}

			table = static_cast<xtrabackup_tables_t *>
				(ut_malloc(sizeof(xtrabackup_tables_t)
					+ strlen(name_buf) + 1));
			memset(table, '\0', sizeof(xtrabackup_tables_t) + strlen(name_buf) + 1);
			table->name = ((char*)table) + sizeof(xtrabackup_tables_t);
			strcpy(table->name, name_buf);

			HASH_INSERT(xtrabackup_tables_t, name_hash, tables_hash,
					ut_fold_string(table->name), table);
		}
	}
}

static
void
xb_tables_hash_free(hash_table_t* hash)
{
	ulint	i;

	/* free the hash elements */
	for (i = 0; i < hash_get_n_cells(hash); i++) {
		xtrabackup_tables_t*	table;

		table = static_cast<xtrabackup_tables_t *>
			(HASH_GET_FIRST(hash, i));

		while (table) {
			xtrabackup_tables_t*	prev_table = table;

			table = static_cast<xtrabackup_tables_t *>
				(HASH_GET_NEXT(name_hash, prev_table));

			HASH_DELETE(xtrabackup_tables_t, name_hash, hash,
				ut_fold_string(prev_table->name), prev_table);
			ut_free(prev_table);
		}
	}

	/* free hash */
	hash_table_free(hash);
}

/************************************************************************
Destroy table filters for partial backup. */
static
void
xb_filters_free()
{
	if (xtrabackup_tables) {
		/* free regexp */
		int i;

		for (i = 0; i < tables_regex_num; i++) {
			xb_regfree(&tables_regex[i]);
		}
		ut_free(tables_regex);
	}

	if (xtrabackup_tables_file) {
		xb_tables_hash_free(tables_hash);
	}
}

/***********************************************************************
Create a specified sync file and wait until it's removed.  */
static void
xtrabackup_suspend(
/*===============*/
	const char*	sync_fn)	/*!<in: sync file name */
{
	char		suspend_path[FN_REFLEN];
	ibool		success = TRUE;
	ibool		exists = TRUE;
	os_file_type_t	type;

	xb_make_sync_file_name(sync_fn, suspend_path);

	if (!xb_create_sync_file(suspend_path)) {
		return;
	}

	while (exists && success) {
		os_thread_sleep(200000); /*0.2 sec*/
		success = os_file_status(suspend_path, &exists, &type);
		/* success == FALSE if file exists, but stat() failed.
		os_file_status() prints an error message in this case */
		ut_a(success);
	}
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

static void
xtrabackup_backup_func(void)
{
	MY_STAT			 stat_info;
	lsn_t			 latest_cp;
	uint			 i;
	uint			 count;
	os_ib_mutex_t		 count_mutex;
	data_thread_ctxt_t 	*data_threads;

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

	xb_set_innodb_read_only();

	srv_backup_mode = TRUE;
	srv_close_files = xb_close_files;

	if (srv_close_files)
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
#if (MYSQL_VERSION_ID < 50100)
		srv_unix_file_flush_method = SRV_UNIX_FDATASYNC;
#else /* MYSQL_VERSION_ID < 51000 */
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;
#endif
#if (MYSQL_VERSION_ID < 50100)
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "fdatasync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_FDATASYNC;
#else /* MYSQL_VERSION_ID < 51000 */
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "fsync")) {
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;
#endif

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DSYNC")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DIRECT")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DIRECT;
		msg("xtrabackup: using O_DIRECT\n");
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "littlesync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_LITTLESYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "nosync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_NOSYNC;
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID > 50600
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "ALL_O_DIRECT")) {
		srv_unix_file_flush_method = SRV_UNIX_ALL_O_DIRECT;
		msg("xtrabackup: using ALL_O_DIRECT\n");
#endif
#if MYSQL_VERSION_ID > 50600
	} else if (0 == ut_strcmp(srv_file_flush_method_str,
				  "O_DIRECT_NO_FSYNC")) {
		srv_unix_file_flush_method = SRV_UNIX_O_DIRECT_NO_FSYNC;
		msg("xtrabackup: using O_DIRECT_NO_FSYNC\n");
#endif
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

	os_sync_mutex = NULL;
	srv_general_init();
	ut_crc32_init();

	xb_filters_init();

	{
	ibool	log_file_created;
	ibool	log_created	= FALSE;
	ibool	log_opened	= FALSE;
	ulint	err;
	ulint	i;

	xb_fil_io_init();

	log_init();

	lock_sys_create(srv_lock_table_size);

	for (i = 0; i < srv_n_log_files; i++) {
		err = open_or_create_log_file(FALSE, &log_file_created,
							     log_opened, 0, i);
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
		&& (my_mkdir(xtrabackup_extra_lsndir,0777,MYF(0)) < 0)){
		msg("xtrabackup: Error: cannot mkdir %d: %s\n",
		    my_errno, xtrabackup_extra_lsndir);
		exit(EXIT_FAILURE);
	}

	/* create target dir if not exist */
	if (!my_stat(xtrabackup_target_dir,&stat_info,MYF(0))
		&& (my_mkdir(xtrabackup_target_dir,0777,MYF(0)) < 0)){
		msg("xtrabackup: Error: cannot mkdir %d: %s\n",
		    my_errno, xtrabackup_target_dir);
		exit(EXIT_FAILURE);
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

	/* start back ground thread to copy newer log */
	os_thread_id_t log_copying_thread_id;
	datafiles_iter_t *it;

	log_hdr_buf_ = static_cast<byte *>
		(ut_malloc(LOG_FILE_HDR_SIZE + UNIV_PAGE_SIZE_MAX));
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

	log_group_read_checkpoint_info(max_cp_group, max_cp_field);
	buf = log_sys->checkpoint_buf;

	checkpoint_lsn_start = MACH_READ_64(buf + LOG_CHECKPOINT_LSN);
	checkpoint_no_start = MACH_READ_64(buf + LOG_CHECKPOINT_NO);

	mutex_exit(&log_sys->mutex);

reread_log_header:
	fil_io(OS_FILE_READ | OS_FILE_LOG, TRUE, max_cp_group->space_id,
				0,
				0, 0, LOG_FILE_HDR_SIZE,
				log_hdr_buf, max_cp_group);

	/* check consistency of log file header to copy */
	mutex_enter(&log_sys->mutex);

	err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

        if (err != DB_SUCCESS) {

		ut_free(log_hdr_buf_);
                exit(EXIT_FAILURE);
        }

        log_group_read_checkpoint_info(max_cp_group, max_cp_field);
        buf = log_sys->checkpoint_buf;

	if(ut_dulint_cmp(checkpoint_no_start,
			MACH_READ_64(buf + LOG_CHECKPOINT_NO)) != 0) {
		checkpoint_lsn_start = MACH_READ_64(buf + LOG_CHECKPOINT_LSN);
		checkpoint_no_start = MACH_READ_64(buf + LOG_CHECKPOINT_NO);
		mutex_exit(&log_sys->mutex);
		goto reread_log_header;
	}

	mutex_exit(&log_sys->mutex);

	xtrabackup_init_datasinks();

	/* open the log file */
	memset(&stat_info, 0, sizeof(MY_STAT));
	dst_log_file = ds_open(ds_meta, XB_LOG_FILENAME, &stat_info);
	if (dst_log_file == NULL) {
		msg("xtrabackup: error: failed to open the target stream for "
		    "'%s'.\n", XB_LOG_FILENAME);
		ut_free(log_hdr_buf_);
		exit(EXIT_FAILURE);
	}

	/* label it */
	strcpy((char*) log_hdr_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
		"xtrabkup ");
	ut_sprintf_timestamp(
		(char*) log_hdr_buf + (LOG_FILE_WAS_CREATED_BY_HOT_BACKUP
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
		wait_throttle = xb_os_event_create(NULL);

		os_thread_create(io_watching_thread, NULL, &io_watching_thread_id);
	}


	/* copy log file by current position */
	if(xtrabackup_copy_logfile(checkpoint_lsn_start, FALSE))
		exit(EXIT_FAILURE);


	log_copying_stop = xb_os_event_create(NULL);
	os_thread_create(log_copying_thread, NULL, &log_copying_thread_id);

	/* Populate fil_system with tablespaces to copy */
	err = xb_load_tablespaces();
	if (err != DB_SUCCESS) {
		msg("xtrabackup: error: xb_load_tablespaces() failed with"
		    "error code %lu\n", err);
		exit(EXIT_FAILURE);
	}

	/* Suspend at start, for the FLUSH CHANGED_PAGE_BITMAPS call */
	if (xtrabackup_suspend_at_start) {
		xtrabackup_suspend(XB_FN_SUSPENDED_AT_START);
	}

	if (xtrabackup_incremental) {
		if (!xtrabackup_incremental_force_scan) {
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

	it = datafiles_iter_new(f_system);
	if (it == NULL) {
		msg("xtrabackup: Error: datafiles_iter_new() failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Create data copying threads */
	data_threads = (data_thread_ctxt_t *)
		ut_malloc(sizeof(data_thread_ctxt_t) * xtrabackup_parallel);
	count = xtrabackup_parallel;
	count_mutex = OS_MUTEX_CREATE();

	for (i = 0; i < (uint) xtrabackup_parallel; i++) {
		data_threads[i].it = it;
		data_threads[i].num = i+1;
		data_threads[i].count = &count;
		data_threads[i].count_mutex = count_mutex;
		os_thread_create(data_copy_thread_func, data_threads + i,
				 &data_threads[i].id);
	}

	/* Wait for threads to exit */
	while (1) {
		os_thread_sleep(1000000);
		os_mutex_enter(count_mutex);
		if (count == 0) {
			os_mutex_exit(count_mutex);
			break;
		}
		os_mutex_exit(count_mutex);
	}

	os_mutex_free(count_mutex);
	ut_free(data_threads);
	datafiles_iter_free(it);

	if (changed_page_bitmap) {
		xb_page_bitmap_deinit(changed_page_bitmap);
	}
	}

	/* suspend-at-end */
	if (xtrabackup_suspend_at_end) {
		xtrabackup_suspend(XB_FN_SUSPENDED_AT_END);
	}

	/* read the latest checkpoint lsn */
	latest_cp = ut_dulint_zero;
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

		log_group_read_checkpoint_info(max_cp_group, max_cp_field);

		latest_cp = MACH_READ_64(log_sys->checkpoint_buf + LOG_CHECKPOINT_LSN);

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

	os_event_free(log_copying_stop);

	/* Signal innobackupex that log copying has stopped and it may now
	unlock tables, so we can possibly stream xtrabackup_logfile later
	without holding the lock. */
	if (xtrabackup_suspend_at_end) {
		char	log_copied_sync_file[FN_REFLEN];
		xb_make_sync_file_name(XB_FN_LOG_COPIED, log_copied_sync_file);
		if (!xb_create_sync_file(log_copied_sync_file)) {
			exit(EXIT_FAILURE);
		}
	}

	if(!xtrabackup_incremental) {
		strcpy(metadata_type, "full-backuped");
		metadata_from_lsn = ut_dulint_zero;
	} else {
		strcpy(metadata_type, "incremental");
		metadata_from_lsn = incremental_lsn;
	}
	metadata_to_lsn = latest_cp;
	metadata_last_lsn = log_copy_scanned_lsn;

	if (!xtrabackup_stream_metadata(ds_meta)) {
		msg("xtrabackup: error: "
		    "xtrabackup_stream_metadata() failed.\n");
	}
	if (xtrabackup_extra_lsndir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_extra_lsndir,
			XTRABACKUP_METADATA_FILENAME);
		if (!xtrabackup_write_metadata(filename))
			msg("xtrabackup: error: "
			    "xtrabackup_write_metadata() failed.\n");

	}

	xtrabackup_destroy_datasinks();

	if (wait_throttle)
		os_event_free(wait_throttle);

	msg("xtrabackup: Transaction log of lsn (" LSN_PF ") to (" LSN_PF
	    ") was copied.\n", checkpoint_lsn_start, log_copy_scanned_lsn);
	xb_filters_free();

	xb_data_files_close();
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
	ulint	page_size;
	buf_block_t*	block;
	ulint	zip_size;

	n_pages = sum_data = n_recs = 0;
	n_pages_extern = sum_data_extern = 0;


	if (level == 0)
		fprintf(stdout, "        leaf pages: ");
	else
		fprintf(stdout, "     level %lu pages: ", level);

	mtr_start(&mtr);

	mtr_x_lock(&(index->lock), &mtr);
	block = xb_btr_root_block_get(index, RW_X_LATCH, &mtr);
	page = buf_block_get_frame(block);

	space = page_get_space_id(page);
	zip_size = fil_space_get_zip_size(space);

	while (level != btr_page_get_level(page, &mtr)) {

		ut_a(space == buf_block_get_space(block));
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
#if (MYSQL_VERSION_ID < 50100)
	mtr_x_lock(&(index->tree->lock), &mtr);
#else /* MYSQL_VERSION_ID < 51000 */
	mtr_x_lock(&(index->lock), &mtr);
#endif

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

						local_block = btr_block_get(space_id, zip_size, page_no, RW_S_LATCH, index, &local_mtr);
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
		block = btr_block_get(space, zip_size, right_page_no,
				      RW_X_LATCH, index, &mtr);
		page = buf_block_get_frame(block);
		goto loop;
	}
	mem_heap_free(heap);

	if (zip_size) {
		page_size = zip_size;
	} else {
		page_size = UNIV_PAGE_SIZE;
	}

	if (level == 0)
		fprintf(stdout, "recs=%llu, ", n_recs);

	fprintf(stdout, "pages=%llu, data=%llu bytes, data/pages=%lld%%",
		n_pages, sum_data,
		((sum_data * 100)/ page_size)/n_pages);


	if (level == 0 && n_pages_extern) {
		putc('\n', stdout);
		/* also scan blob pages*/
		fprintf(stdout, "    external pages: ");

		fprintf(stdout, "pages=%llu, data=%llu bytes, data/pages=%lld%%",
			n_pages_extern, sum_data_extern,
			((sum_data_extern * 100)/ page_size)/n_pages_extern);
	}

	putc('\n', stdout);

	if (level > 0) {
		xtrabackup_stats_level(index, level - 1);
	}

	return(TRUE);
}

static void
xtrabackup_stats_func(void)
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

	xb_set_innodb_read_only();
	/* set read only */
#if MYSQL_VERSION_ID < 50600
	srv_fake_write = TRUE;
#endif
#if (MYSQL_VERSION_ID >= 50500) && (MYSQL_VERSION_ID < 50600)
	/* AIO is incompatible with srv_read_only/srv_fake_write */
	srv_use_native_aio = FALSE;
#endif

	/* initialize components */
	if(innodb_init_param())
		exit(EXIT_FAILURE);

	/* Check if the log files have been created, otherwise innodb_init()
	will crash when called with srv_read_only == TRUE */
	for (n = 0; n < srv_n_log_files; n++) {
		char		logname[FN_REFLEN];
		ibool		exists;
		os_file_type_t	type;

		snprintf(logname, sizeof(logname), "%s%c%s%lu",
			INNODB_LOG_DIR, SRV_PATH_SEPARATOR,
			"ib_logfile", (ulong) n);
		srv_normalize_path_for_win(logname);

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

	xb_adjust_fatal_semaphore_wait_threshold(72000); /* 20 hours */

	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	sys_tables = xb_dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);

	xb_btr_pcur_open_at_index_side(TRUE, sys_index, BTR_SEARCH_LEAF, &pcur,
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
		xb_adjust_fatal_semaphore_wait_threshold(-72000);

		goto end;
	}

	field = rec_get_nth_field_old(rec, 0, &len);

#if (MYSQL_VERSION_ID < 50100)
	if (!rec_get_deleted_flag(rec, sys_tables->comp))
#else /* MYSQL_VERSION_ID < 51000 */
	if (!rec_get_deleted_flag(rec, 0))
#endif
	{

		/* We found one */

                char*	table_name = mem_strdupl((char*) field, len);

		btr_pcur_store_position(&pcur, &mtr);

		mtr_commit(&mtr);

		table = xb_dict_table_get_low(table_name);
		mem_free(table_name);

		if (table && check_if_skip_table(table->name))
			goto skip;


		if (table == NULL) {
			fputs("InnoDB: Failed to load table ", stderr);
#if (MYSQL_VERSION_ID < 50100)
			ut_print_namel(stderr, NULL, (char*) field, len);
#else /* MYSQL_VERSION_ID < 51000 */
			ut_print_namel(stderr, NULL, TRUE, (char*) field, len);
#endif
			putc('\n', stderr);
		} else {
			dict_index_t*	index;

			/* The table definition was corrupt if there
			is no index */

			if (dict_table_get_first_index(table)) {
				xb_dict_stats_update_transient(table);
			}

			//dict_table_print_low(table);

			index = UT_LIST_GET_FIRST(table->indexes);
			while (index != NULL) {
{
	ib_int64_t	n_vals;

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
		table->name, index->name,
		(ulong) index->space,
#if (MYSQL_VERSION_ID < 50100)
		(ulong) index->tree->page,
#else /* MYSQL_VERSION_ID < 51000 */
		(ulong) index->page,
#endif
		(ulong) fil_space_get_zip_size(index->space),
		(ulong) n_vals,
		(ulong) index->stat_n_leaf_pages,
		(ulong) index->stat_index_size);

	{
		mtr_t	local_mtr;
		page_t*	root;
		ulint	page_level;

		mtr_start(&local_mtr);

#if (MYSQL_VERSION_ID < 50100)
		mtr_x_lock(&(index->tree->lock), &local_mtr);
		root = btr_root_get(index->tree, &local_mtr);
#else /* MYSQL_VERSION_ID < 51000 */
		mtr_x_lock(&(index->lock), &local_mtr);
		root = btr_root_get(index, &local_mtr);
#endif
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

	xb_filters_free();

	/* shutdown InnoDB */
	if(innodb_end())
		exit(EXIT_FAILURE);
}

/* ================= prepare ================= */

static my_bool
xtrabackup_init_temp_log(void)
{
	os_file_t	src_file = XB_FILE_UNDEFINED;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	ibool	success;

	ulint	field;
	byte	log_buf[UNIV_PAGE_SIZE_MAX * 128]; /* 2 MB */

	ib_int64_t	file_size;

	lsn_t	max_no;
	lsn_t	max_lsn;
	lsn_t	checkpoint_no;

	ulint	fold;

	bool	checkpoint_found;

	max_no = ut_dulint_zero;

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

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);
retry:
	src_file = xb_file_create_no_error_handling(src_path, OS_FILE_OPEN,
						    OS_FILE_READ_WRITE,
						    &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		msg("xtrabackup: Warning: cannot open %s. will try to find.\n",
		    src_path);

		/* check if ib_logfile0 may be xtrabackup_logfile */
		src_file = xb_file_create_no_error_handling(dst_path,
							    OS_FILE_OPEN,
							    OS_FILE_READ_WRITE,
							    &success);
		if (!success) {
			os_file_get_last_error(TRUE);
			msg("  xtrabackup: Fatal error: cannot find %s.\n",
			    src_path);

			goto error;
		}

		success = xb_os_file_read(src_file, log_buf, 0,
					  LOG_FILE_HDR_SIZE);
		if (!success) {
			goto error;
		}

		if ( ut_memcmp(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
				(byte*)"xtrabkup", (sizeof "xtrabkup") - 1) == 0) {
			msg("  xtrabackup: 'ib_logfile0' seems to be "
			    "'xtrabackup_logfile'. will retry.\n");

			os_file_close(src_file);
			src_file = XB_FILE_UNDEFINED;

			/* rename and try again */
			success = xb_file_rename(dst_path, src_path);
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
	success = xb_os_file_read(src_file, log_buf, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	if ( ut_memcmp(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
			(byte*)"xtrabkup", (sizeof "xtrabkup") - 1) != 0 ) {
		msg("xtrabackup: notice: xtrabackup_logfile was already used "
		    "to '--prepare'.\n");
		goto skip_modify;
	} else {
		/* clear it later */
		//memset(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
		//		' ', 4);
	}

	checkpoint_found = false;

	/* read last checkpoint lsn */
	for (field = LOG_CHECKPOINT_1; field <= LOG_CHECKPOINT_2;
			field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {
		if (!xb_recv_check_cp_is_consistent(const_cast<const byte *>
						    (log_buf + field)))
			goto not_consistent;

		checkpoint_no = MACH_READ_64(log_buf + field + LOG_CHECKPOINT_NO);

		if (ut_dulint_cmp(checkpoint_no, max_no) >= 0) {
			max_no = checkpoint_no;
			max_lsn = MACH_READ_64(log_buf + field + LOG_CHECKPOINT_LSN);
			checkpoint_found = true;
		}
not_consistent:
		;
	}

	if (!checkpoint_found) {
		msg("xtrabackup: No valid checkpoint found.\n");
		goto error;
	}


	/* It seems to be needed to overwrite the both checkpoint area. */
	MACH_WRITE_64(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN, max_lsn);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1
			+ LOG_CHECKPOINT_OFFSET_LOW32,
			LOG_FILE_HDR_SIZE + (ulint) ut_dulint_minus(max_lsn,
			ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE)));
#if MYSQL_VERSION_ID >= 50600
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1
			+ LOG_CHECKPOINT_OFFSET_HIGH32, 0);
#endif
#ifdef XTRADB_BASED
	MACH_WRITE_64(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_ARCHIVED_LSN,
			(ib_uint64_t)(LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
					ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE))));
#endif
	fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_1, LOG_CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN,
		LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_CHECKSUM_2, fold);

	MACH_WRITE_64(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN, max_lsn);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_2
			+ LOG_CHECKPOINT_OFFSET_LOW32,
			LOG_FILE_HDR_SIZE + (ulint) ut_dulint_minus(max_lsn,
			ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE)));
#if MYSQL_VERSION_ID >= 50600
	mach_write_to_4(log_buf + LOG_CHECKPOINT_2
			+ LOG_CHECKPOINT_OFFSET_HIGH32, 0);
#endif
#ifdef XTRADB_BASED
	MACH_WRITE_64(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_ARCHIVED_LSN,
			(ib_uint64_t)(LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
					ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE))));
#endif
        fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_2, LOG_CHECKPOINT_CHECKSUM_1);
        mach_write_to_4(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_CHECKSUM_1, fold);

        fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN,
                LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
        mach_write_to_4(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_CHECKSUM_2, fold);


	success = xb_os_file_write(src_path, src_file, log_buf, 0,
				   LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	/* expand file size (9/8) and align to UNIV_PAGE_SIZE_MAX */

	if (file_size % UNIV_PAGE_SIZE_MAX) {
		memset(log_buf, 0, UNIV_PAGE_SIZE_MAX);
		success = xb_os_file_write(src_path, src_file, log_buf,
					    file_size,
					    UNIV_PAGE_SIZE_MAX
					    - (ulint) (file_size
						       % UNIV_PAGE_SIZE_MAX));
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
			success = xb_os_file_write(src_path, src_file, log_buf,
						   file_size,
						   UNIV_PAGE_SIZE_MAX * 128);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE_MAX * 128;
		}

		if (expand) {
			success = xb_os_file_write(src_path, src_file, log_buf,
						   file_size,
						   expand * UNIV_PAGE_SIZE_MAX);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE_MAX * expand;
		}
	}

	/* make larger than 2MB */
	if (file_size < 2*1024*1024L) {
		memset(log_buf, 0, UNIV_PAGE_SIZE_MAX);
		while (file_size < 2*1024*1024L) {
			success = xb_os_file_write(src_path, src_file, log_buf,
						   file_size,
						   UNIV_PAGE_SIZE_MAX);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE_MAX;
		}
		file_size = os_file_get_size(src_file);
	}

	msg("xtrabackup: xtrabackup_logfile detected: size=" INT64PF ", "
	    "start_lsn=(" LSN_PF ")\n", file_size, max_lsn);

	os_file_close(src_file);
	src_file = XB_FILE_UNDEFINED;

	/* fake InnoDB */
	INNODB_LOG_DIR = NULL;
	innobase_log_file_size      = file_size;
	innobase_log_files_in_group = 1;

	srv_thread_concurrency = 0;

	/* rename 'xtrabackup_logfile' to 'ib_logfile0' */
	success = xb_file_rename(src_path, dst_path);
	if (!success) {
		goto error;
	}
	xtrabackup_logfile_is_renamed = TRUE;

	return(FALSE);

skip_modify:
	os_file_close(src_file);
	src_file = XB_FILE_UNDEFINED;
	return(FALSE);

error:
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

/***********************************************************************
Searches for matching tablespace file for given .delta file and space_id
in given directory. When matching tablespace found, renames it to match the
name of .delta file. If there was a tablespace with matching name and
mismatching ID, renames it to xtrabackup_tmp_#ID.ibd. If there was no
matching file, creates a new tablespace.
@return file handle of matched or created file */
static
os_file_t
xb_delta_open_matching_space(
	const char*	dbname,		/* in: path to destination database dir */
	const char*	name,		/* in: name of delta file (without .delta) */
	ulint		space_id,	/* in: space id of delta file */
	ulint		zip_size,	/* in: zip_size of tablespace */
	char*		real_name,	/* out: full path of destination file */
	size_t		real_name_len,	/* out: buffer size for real_name */
	ibool* 		success)	/* out: indicates error. TRUE = success */
{
	char			dest_dir[FN_REFLEN];
	char			dest_space_name[FN_REFLEN];
	ibool			ok;
	fil_space_t*		fil_space;
	os_file_t		file	= 0;
	ulint			tablespace_flags;
	xtrabackup_tables_t*	table;

	ut_a(dbname != NULL ||
		trx_sys_sys_space(space_id) ||
		space_id == ULINT_UNDEFINED);

	*success = FALSE;

	if (dbname) {
		snprintf(dest_dir, FN_REFLEN, "%s/%s",
			xtrabackup_target_dir, dbname);
		srv_normalize_path_for_win(dest_dir);

		snprintf(dest_space_name, FN_REFLEN, "%s%s/%s",
			 xb_dict_prefix, dbname, name);
	} else {
		snprintf(dest_dir, FN_REFLEN, "%s", xtrabackup_target_dir);
		srv_normalize_path_for_win(dest_dir);

		snprintf(dest_space_name, FN_REFLEN, "%s%s", xb_dict_prefix,
			 name);
	}

	snprintf(real_name, real_name_len,
		 "%s/%s",
		 xtrabackup_target_dir, dest_space_name);
	srv_normalize_path_for_win(real_name);
	dest_space_name[strlen(dest_space_name) - XB_DICT_SUFFIX_LEN] = '\0';

	/* Create the database directory if it doesn't exist yet */
	if (!os_file_create_directory(dest_dir, FALSE)) {
		msg("xtrabackup: error: cannot create dir %s\n", dest_dir);
		return file;
	}

	if (trx_sys_sys_space(space_id)) {
		goto found;
	}

	/* remember space name for further reference */
	table = static_cast<xtrabackup_tables_t *>
		(ut_malloc(sizeof(xtrabackup_tables_t) +
			strlen(dest_space_name) + 1));

	table->name = ((char*)table) + sizeof(xtrabackup_tables_t);
	strcpy(table->name, dest_space_name);
	HASH_INSERT(xtrabackup_tables_t, name_hash, inc_dir_tables_hash,
			ut_fold_string(table->name), table);

	mutex_enter(&fil_system->mutex);
	fil_space = xb_space_get_by_name(dest_space_name);
	mutex_exit(&fil_system->mutex);

	if (fil_space != NULL) {
		if (fil_space->id == space_id || space_id == ULINT_UNDEFINED) {
			/* we found matching space */
			goto found;
		} else {

			char	tmpname[FN_REFLEN];

			snprintf(tmpname, FN_REFLEN, "%s%s/xtrabackup_tmp_#%lu",
				 xb_dict_prefix, dbname, fil_space->id);

			msg("xtrabackup: Renaming %s to %s.ibd\n",
				fil_space->name, tmpname);

			if (!xb_fil_rename_tablespace(NULL, fil_space->id,
						      tmpname, NULL))
			{
				msg("xtrabackup: Cannot rename %s to %s\n",
					fil_space->name, tmpname);
				goto exit;
			}
		}
	}

	if (space_id == ULINT_UNDEFINED)
	{
		msg("xtrabackup: Error: Cannot handle DDL operation on tablespace "
		    "%s\n", dest_space_name);
		exit(EXIT_FAILURE);
	}
	mutex_enter(&fil_system->mutex);
	fil_space = xb_space_get_by_id(space_id);
	mutex_exit(&fil_system->mutex);
	if (fil_space != NULL) {
		char	tmpname[FN_REFLEN];

		strncpy(tmpname, dest_space_name, FN_REFLEN);
#if MYSQL_VERSION_ID < 50600
		/* Need to truncate .ibd before renaming.  For MySQL 5.6 it's
		already truncated.  */
		tmpname[strlen(tmpname) - 4] = 0;
#endif

		msg("xtrabackup: Renaming %s to %s\n",
		    fil_space->name, dest_space_name);

		if (!xb_fil_rename_tablespace(NULL, fil_space->id, tmpname,
					      NULL))
		{
			msg("xtrabackup: Cannot rename %s to %s\n",
				fil_space->name, dest_space_name);
			goto exit;
		}

		goto found;
	}

	/* No matching space found. create the new one.  */

	if (!xb_fil_space_create(dest_space_name, space_id, 0,
				 FIL_TABLESPACE)) {
		msg("xtrabackup: Cannot create tablespace %s\n",
			dest_space_name);
		goto exit;
	}

	/* Calculate correct tablespace flags for compressed tablespaces.  */
	if (!zip_size || zip_size == ULINT_UNDEFINED) {
		tablespace_flags = 0;
	}
	else {
		tablespace_flags
			= (get_bit_shift(zip_size >> PAGE_ZIP_MIN_SIZE_SHIFT
					 << 1)
			   << DICT_TF_ZSSIZE_SHIFT)
			| DICT_TF_COMPACT
			| (DICT_TF_FORMAT_ZIP << DICT_TF_FORMAT_SHIFT);
		ut_a(dict_tf_get_zip_size(tablespace_flags)
		     == zip_size);
	}
	*success = xb_space_create_file(real_name, space_id, tablespace_flags,
					&file);
	goto exit;

found:
	/* open the file and return it's handle */

	file = xb_file_create_no_error_handling(real_name, OS_FILE_OPEN,
					    OS_FILE_READ_WRITE,
					    &ok);

	if (ok) {
		*success = TRUE;
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
	os_file_t	src_file = XB_FILE_UNDEFINED;
	os_file_t	dst_file = XB_FILE_UNDEFINED;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	char	meta_path[FN_REFLEN];
	char	space_name[FN_REFLEN];
	ibool	success;

	ibool	last_buffer = FALSE;
	ulint	page_in_buffer;
	ulint	incremental_buffers = 0;

	xb_delta_info_t info;
	ulint		page_size;
	ulint		page_size_shift;
	byte*		incremental_buffer_base = NULL;
	byte*		incremental_buffer;

	size_t		offset;

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

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);
	srv_normalize_path_for_win(meta_path);

	if (!xb_read_delta_metadata(meta_path, &info)) {
		goto error;
	}

	page_size = info.page_size;
	page_size_shift = get_bit_shift(page_size);
	msg("xtrabackup: page size for %s is %lu bytes\n",
	    src_path, page_size);
	if (page_size_shift < 10 ||
	    page_size_shift > UNIV_PAGE_SIZE_SHIFT_MAX) {
		msg("xtrabackup: error: invalid value of page_size "
		    "(%lu bytes) read from %s\n", page_size, meta_path);
		goto error;
	}

	src_file = xb_file_create_no_error_handling(src_path, OS_FILE_OPEN,
						    OS_FILE_READ_WRITE,
						    &success);
	if (!success) {
		os_file_get_last_error(TRUE);
		msg("xtrabackup: error: cannot open %s\n", src_path);
		goto error;
	}

	posix_fadvise(src_file, 0, 0, POSIX_FADV_SEQUENTIAL);

	xb_file_set_nocache(src_file, src_path, "OPEN");

	dst_file = xb_delta_open_matching_space(
			dbname, space_name, info.space_id, info.zip_size,
			dst_path, sizeof(dst_path), &success);
	if (!success) {
		msg("xtrabackup: error: cannot open %s\n", dst_path);
		goto error;
	}

	posix_fadvise(dst_file, 0, 0, POSIX_FADV_DONTNEED);

	xb_file_set_nocache(dst_file, dst_path, "OPEN");

	/* allocate buffer for incremental backup (4096 pages) */
	incremental_buffer_base = static_cast<byte *>
		(ut_malloc((UNIV_PAGE_SIZE_MAX / 4 + 1) *
			   UNIV_PAGE_SIZE_MAX));
	incremental_buffer = static_cast<byte *>
		(ut_align(incremental_buffer_base,
			  UNIV_PAGE_SIZE_MAX));

	msg("Applying %s to %s...\n", src_path, dst_path);

	while (!last_buffer) {
		ulint cluster_header;

		/* read to buffer */
		/* first block of block cluster */
		offset = ((incremental_buffers * (page_size / 4))
			 << page_size_shift);
		success = xb_os_file_read(src_file, incremental_buffer,
					  offset, page_size);
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
			if (mach_read_from_4(incremental_buffer + page_in_buffer * 4)
			    == 0xFFFFFFFFUL)
				break;
		}

		ut_a(last_buffer || page_in_buffer == page_size / 4);

		/* read whole of the cluster */
		success = xb_os_file_read(src_file, incremental_buffer,
					  offset, page_in_buffer * page_size);
		if (!success) {
			goto error;
		}

		posix_fadvise(src_file, offset, page_in_buffer * page_size,
			      POSIX_FADV_DONTNEED);

		for (page_in_buffer = 1; page_in_buffer < page_size / 4;
		     page_in_buffer++) {
			ulint offset_on_page;

			offset_on_page = mach_read_from_4(incremental_buffer + page_in_buffer * 4);

			if (offset_on_page == 0xFFFFFFFFUL)
				break;

			/* apply blocks in the cluster */
//			if (ut_dulint_cmp(incremental_lsn,
//				MACH_READ_64(incremental_buffer
//						 + page_in_buffer * page_size
//						 + FIL_PAGE_LSN)) >= 0)
//				continue;

			success = xb_os_file_write(dst_path, dst_file,
					incremental_buffer +
						page_in_buffer * page_size,
					(offset_on_page << page_size_shift),
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
	xtrabackup_tables_t*	table;

	snprintf(name, FN_REFLEN, "%s%s/%s",
				xb_dict_prefix, db_name, file_name);
	name[strlen(name) - XB_DICT_SUFFIX_LEN] = '\0';

	XB_HASH_SEARCH(name_hash, inc_dir_tables_hash, ut_fold_string(name),
		       table, (void) 0,
		       !strcmp(table->name, name));

	if (!table) {
		snprintf(name, FN_REFLEN, "%s/%s/%s", data_home_dir,
						      db_name, file_name);
		return xb_file_delete(name);
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
	dbdir = os_file_opendir(path, FALSE);

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
	dir = os_file_opendir(path, FALSE);

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

		sprintf(dbpath, "%s/%s", path,
								dbinfo.name);
		srv_normalize_path_for_win(dbpath);

		dbdir = os_file_opendir(dbpath, FALSE);

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
	os_file_t	src_file = XB_FILE_UNDEFINED;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	ibool	success;
	byte	log_buf[UNIV_PAGE_SIZE_MAX];

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

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);

	success = xb_file_rename(dst_path, src_path);
	if (!success) {
		goto error;
	}
	xtrabackup_logfile_is_renamed = FALSE;

	if (!clear_flag)
		return(FALSE);

	/* clear LOG_FILE_WAS_CREATED_BY_HOT_BACKUP field */
	src_file = xb_file_create_no_error_handling(src_path, OS_FILE_OPEN,
						    OS_FILE_READ_WRITE,
						    &success);
	if (!success) {
		goto error;
	}

	success = xb_os_file_read(src_file, log_buf, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	memset(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP, ' ', 4);

	success = xb_os_file_write(src_path, src_file, log_buf, 0,
				   LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	os_file_close(src_file);
	src_file = XB_FILE_UNDEFINED;

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
#if (MYSQL_VERSION_ID < 50600)
	ulint			minlen;
	ulint			maxlen;
#endif

	col = table->cols;

	for (ulint i = 0; i < table->n_cols; ++i, ++col) {
		byte*		ptr = row;

		mach_write_to_4(ptr, col->prtype);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->mtype);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->len);
		ptr += sizeof(ib_uint32_t);

#if (MYSQL_VERSION_ID >= 50600)
		mach_write_to_4(ptr, col->mbminmaxlen);
#else
		innobase_get_cset_width(dtype_get_charset_coll(col->prtype),
					&minlen, &maxlen);
		mach_write_to_4(ptr, DATA_MBMINMAXLEN(minlen, maxlen));
#endif
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->ind);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->ord_part);
		ptr += sizeof(ib_uint32_t);

#if (MYSQL_VERSION_ID >= 50600)
		mach_write_to_4(ptr, col->max_prefix);
#else
		mach_write_to_4(ptr, 0);
#endif

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
	ut_a(table->name != 0);
	len = strlen(table->name) + 1;

	/* Write the table name. */
	mach_write_to_4(value, len);

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)
	    || fwrite(table->name, 1,  len, file) != len) {

		msg("xtrabackup: Error: writing table name.");

		return(false);
	}

	byte		row[sizeof(ib_uint32_t) * 3];

	/* Write the next autoinc value. */
#ifdef INNODB_VERSION_SHORT
	MACH_WRITE_64(row, table->autoinc);
#else
	MACH_WRITE_64(row,
			ut_dulint_create((table->autoinc >> 32) & 0xFFFFFFFFUL,
					 table->autoinc & 0xFFFFFFFFUL));
#endif

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
		msg("xtrabackup: Error: cannot close %s\n", node->name);

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

#ifdef UNIV_LOG_ARCHIVE
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
		if (fileinfo.size > (ib_int64_t)xtrabackup_arch_file_size) {
			xtrabackup_arch_file_size = fileinfo.size;
		}
	}

	return xtrabackup_arch_first_file_lsn != 0;
}
#endif /* UNIV_LOG_ARCHIVE */

static void
xtrabackup_prepare_func(void)
{
	ulint	err;
	datafiles_iter_t	*it;
	fil_node_t		*node;
	fil_space_t		*space;
	char			 metadata_path[FN_REFLEN];

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

	if (!xtrabackup_read_metadata(metadata_path))
		msg("xtrabackup: error: xtrabackup_read_metadata()\n");

	if (!innobase_log_arch_dir)
	{
		if (!strcmp(metadata_type, "full-backuped")) {
			msg("xtrabackup: This target seems to be not prepared "
			    "yet.\n");
		} else if (!strcmp(metadata_type, "full-prepared")) {
			msg("xtrabackup: This target seems to be already "
			    "prepared.\n");
			goto skip_check;
		} else {
			msg("xtrabackup: This target seems not to have correct "
			    "metadata...\n");
		}

		if (xtrabackup_incremental) {
			msg("xtrabackup: error: applying incremental backup "
			    "needs target prepared.\n");
			exit(EXIT_FAILURE);
		}
skip_check:
		if (xtrabackup_incremental
		    && ut_dulint_cmp(metadata_to_lsn, incremental_lsn) != 0) {
			msg("xtrabackup: error: This incremental backup seems "
			    "not to be proper for the target.\n"
			    "xtrabackup:  Check 'to_lsn' of the target and "
			    "'from_lsn' of the incremental.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* Create logfiles for recovery from 'xtrabackup_logfile', before start InnoDB */
	srv_max_n_threads = 1000;
	os_sync_mutex = NULL;
	ut_mem_init();
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
	/* temporally dummy value to avoid crash */
	srv_page_size_shift = 14;
	srv_page_size = (1 << srv_page_size_shift);
#endif
	os_sync_init();
	sync_init();
	os_io_init_simple();
	mem_init(srv_mem_pool_size);
	ut_crc32_init();

	if(!innobase_log_arch_dir && xtrabackup_init_temp_log())
		goto error;

	if(innodb_init_param()) {
		goto error;
	}

	/* Expand compacted datafiles */

	if (xtrabackup_compact) {
		srv_compact_backup = TRUE;

		if (!xb_expand_datafiles()) {
			goto error;
		}

		/* Reset the 'compact' flag in xtrabackup_checkpoints so we
		don't expand on subsequent invocations. */
		xtrabackup_compact = FALSE;
		if (!xtrabackup_write_metadata(metadata_path)) {
			msg("xtrabackup: error: xtrabackup_write_metadata() "
			    "failed\n");
			goto error;
		}
	}

	xb_normalize_init_values();

	if (xtrabackup_incremental || innobase_log_arch_dir) {
		err = xb_data_files_init();
		if (err != DB_SUCCESS) {
			msg("xtrabackup: error: xb_data_files_init() failed "
			    "with error code %lu\n", err);
			goto error;
		}
	}
	if (xtrabackup_incremental) {
		inc_dir_tables_hash = hash_create(1000);

		if(!xtrabackup_apply_deltas()) {
			xb_data_files_close();
			xb_tables_hash_free(inc_dir_tables_hash);
			goto error;
		}
	}
	if (xtrabackup_incremental || innobase_log_arch_dir) {
		xb_data_files_close();
	}
	if (xtrabackup_incremental) {
		/* Cleanup datadir from tablespaces deleted between full and
		incremental backups */

		xb_process_datadir("./", ".ibd", rm_if_not_found, NULL);

		xb_tables_hash_free(inc_dir_tables_hash);
	}
	sync_close();
	sync_initialized = FALSE;
	os_sync_free();
	mem_close();
	os_sync_mutex = NULL;
	ut_free_all_mem();

	/* Reset the configuration as it might have been changed by
	xb_data_files_init(). */
	if(innodb_init_param()) {
		goto error;
	}

	srv_apply_log_only = (ibool) xtrabackup_apply_log_only;
	srv_rebuild_indexes = (ibool) xtrabackup_rebuild_indexes;

	/* increase IO threads */
	if(srv_n_file_io_threads < 10) {
		srv_n_read_io_threads = 4;
		srv_n_write_io_threads = 4;
	}

#ifdef UNIV_LOG_ARCHIVE
	if (innobase_log_arch_dir) {
		srv_arch_dir = innobase_log_arch_dir;
		srv_archive_recovery = TRUE;
		if (xtrabackup_archived_to_lsn) {
			if (xtrabackup_archived_to_lsn < metadata_last_lsn) {
				msg("xtrabackup: warning: logs applying lsn "
				    "limit " UINT64PF " is "
				    "less than metadata last-lsn " UINT64PF
				    " and will be set to metadata last-lsn value\n",
				    xtrabackup_archived_to_lsn,
				    metadata_last_lsn);
				xtrabackup_archived_to_lsn = metadata_last_lsn;
			}
			if (xtrabackup_archived_to_lsn < min_flushed_lsn) {
				msg("xtrabackup: error: logs applying "
				    "lsn limit " UINT64PF " is less than "
				    "min_flushed_lsn " UINT64PF
				    ", there is nothing to do\n",
				    xtrabackup_archived_to_lsn,
				    min_flushed_lsn);
				goto error;
			}
		}
		srv_archive_recovery_limit_lsn= xtrabackup_archived_to_lsn;
		/*
		  Unfinished transactions are not rolled back during log applying
		  as they can be finished at the firther files applyings.
		*/
		srv_apply_log_only = TRUE;

		if (!xtrabackup_arch_search_files(min_flushed_lsn)) {
			goto error;
		}

		/*
		  Check if last log file last lsn is big enough to overlap
		  last scanned lsn read from metadata.
		*/
		if (xtrabackup_arch_last_file_lsn +
		    xtrabackup_arch_file_size -
		    LOG_FILE_HDR_SIZE < metadata_last_lsn) {
			msg("xtrabackup: error: there are no enough archived logs "
			    "to apply\n");
			goto error;
		}
	}
#endif /* UNIV_LOG_ARCHIVE */

	msg("xtrabackup: Starting InnoDB instance for recovery.\n"
	    "xtrabackup: Using %lld bytes for buffer pool "
	    "(set by --use-memory parameter)\n", xtrabackup_use_memory);

	if(innodb_init())
		goto error;

	it = datafiles_iter_new(fil_system);
	if (it == NULL) {
		msg("xtrabackup: Error: datafiles_iter_new() failed.\n");
		exit(EXIT_FAILURE);
	}

	while ((node = datafiles_iter_next(it)) != NULL) {
		byte		*header;
		ulint		 size;
		ulint		 actual_size;
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

		block = buf_page_get(space->id,
				     dict_tf_get_zip_size(flags),
				     0, RW_S_LATCH, &mtr);
		header = FSP_HEADER_OFFSET + buf_block_get_frame(block);

		size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES,
				      &mtr);

		mtr_commit(&mtr);

		fil_extend_space_to_desired_size(&actual_size, space->id, size);
	}

	datafiles_iter_free(it);

	if (xtrabackup_export) {
		msg("xtrabackup: export option is specified.\n");
		os_file_t	info_file = XB_FILE_UNDEFINED;
		char		info_file_path[FN_REFLEN];
		ibool		success;
		char		table_name[FN_REFLEN];

		byte*		page;
		byte*		buf = NULL;

		buf = static_cast<byte *>(ut_malloc(UNIV_PAGE_SIZE * 2));
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
			if (trx_sys_sys_space(space->id)) {
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
			while ((next = strchr(p, SRV_PATH_SEPARATOR)) != NULL)
			{
				prev = p;
				p = next + 1;
			}
			info_file_path[len - 4] = 0;
			strncpy(table_name, prev, FN_REFLEN);

			info_file_path[len - 4] = '.';

			mutex_enter(&(dict_sys->mutex));

			table = xb_dict_table_get_low(table_name);
			if (!table) {
				msg("xtrabackup: error: "
				    "cannot find dictionary "
				    "record of table %s\n",
				    table_name);
				goto next_node;
			}
			index = dict_table_get_first_index(table);
			n_index = UT_LIST_GET_LEN(table->indexes);
			if (n_index > 31) {
				msg("xtrabackup: error: "
				    "sorry, cannot export over "
				    "31 indexes for now.\n");
				goto next_node;
			}

			/* Write MySQL 5.6 .cfg file */
			if (!xb_export_cfg_write(node, table)) {
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
				    index->name,
#if (MYSQL_VERSION_ID < 50500)
				    index->id.low,
#else
				    (ulint)(index->id &
					    0xFFFFFFFFUL),
#endif
				    (ulint) index->page);
				index = dict_table_get_next_index(index);
				n_index++;
			}

			srv_normalize_path_for_win(info_file_path);
			info_file = xb_file_create(
				info_file_path,
				OS_FILE_OVERWRITE,
				OS_FILE_NORMAL, OS_DATA_FILE,
				&success);
			if (!success) {
				os_file_get_last_error(TRUE);
				goto next_node;
			}
			success = xb_os_file_write(info_file_path,
						   info_file, page,
						   0, UNIV_PAGE_SIZE);
			if (!success) {
				os_file_get_last_error(TRUE);
				goto next_node;
			}
			success = xb_file_flush(info_file);
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
	}

	/* print binlog position (again?) */
	msg("\n[notice (again)]\n"
	    "  If you use binary log and don't use any hack of group commit,\n"
	    "  the binary log position seems to be:\n");
	trx_sys_print_mysql_binlog_offset();
	msg("\n");

	/* output to xtrabackup_binlog_pos_innodb */
	if (*trx_sys_mysql_bin_log_name != '\0') {
		FILE *fp;

		fp = fopen("xtrabackup_binlog_pos_innodb", "w");
		if (fp) {
			/* Use UINT64PF instead of LSN_PF here, as we have to
			maintain the file format. */
			fprintf(fp, "%s\t" UINT64PF "\n",
				trx_sys_mysql_bin_log_name,
				trx_sys_mysql_bin_log_pos);
			fclose(fp);
		} else {
			msg("xtrabackup: failed to open "
			    "'xtrabackup_binlog_pos_innodb'\n");
		}
	}
#ifdef UNIV_LOG_ARCHIVE
	if (innobase_log_arch_dir)
		srv_start_lsn = log_sys->lsn = recv_sys->recovered_lsn;
#endif /* UNIV_LOG_ARCHIVE */
	/* Check whether the log is applied enough or not. */
	if ((xtrabackup_incremental
	     && ut_dulint_cmp(srv_start_lsn, incremental_last_lsn) < 0)
	    ||(!xtrabackup_incremental
	       && ut_dulint_cmp(srv_start_lsn, metadata_last_lsn) < 0)) {
		msg(
"xtrabackup: ########################################################\n"
"xtrabackup: # !!WARNING!!                                          #\n"
"xtrabackup: # The transaction log file is corrupted.               #\n"
"xtrabackup: # The log was not applied to the intended LSN!         #\n"
"xtrabackup: ########################################################\n"
		    );
		if (xtrabackup_incremental) {
			msg("xtrabackup: The intended lsn is " LSN_PF "\n",
			    incremental_last_lsn);
		} else {
			msg("xtrabackup: The intended lsn is " LSN_PF "\n",
			    metadata_last_lsn);
		}
	}

	if(innodb_end())
		goto error;

	sync_initialized = FALSE;
	os_sync_mutex = NULL;

	/* re-init necessary components */
	ut_mem_init();
	os_sync_init();
	sync_init();
	os_io_init_simple();

	if(xtrabackup_close_temp_log(TRUE))
		exit(EXIT_FAILURE);

	/* output to metadata file */
	{
		char	filename[FN_REFLEN];

		strcpy(metadata_type, "full-prepared");

		if(xtrabackup_incremental
		   && ut_dulint_cmp(metadata_to_lsn, incremental_to_lsn) < 0)
		{
			metadata_to_lsn = incremental_to_lsn;
			metadata_last_lsn = incremental_last_lsn;
		}

		sprintf(filename, "%s/%s", xtrabackup_target_dir, XTRABACKUP_METADATA_FILENAME);
		if (!xtrabackup_write_metadata(filename))
			msg("xtrabackup: error: xtrabackup_write_metadata"
			    "(xtrabackup_target_dir)\n");

		if(xtrabackup_extra_lsndir) {
			sprintf(filename, "%s/%s", xtrabackup_extra_lsndir, XTRABACKUP_METADATA_FILENAME);
			if (!xtrabackup_write_metadata(filename))
				msg("xtrabackup: error: "
				    "xtrabackup_write_metadata"
				    "(xtrabackup_extra_lsndir)\n");
		}
	}

	if(!xtrabackup_create_ib_logfile)
		return;

	/* TODO: make more smart */

	msg("\n[notice]\n"
	    "We cannot call InnoDB second time during the process lifetime.\n");
	msg("Please re-execte to create ib_logfile*. Sorry.\n");

	return;

error:
	xtrabackup_close_temp_log(FALSE);

	exit(EXIT_FAILURE);
}

/* ================= main =================== */

int main(int argc, char **argv)
{
	int ho_error;

#ifdef __linux__
	/* Ensure xtrabackup process is killed when the parent one
	(innobackupex) is terminated with an unhandled signal */

	if (prctl(PR_SET_PDEATHSIG, SIGKILL)) {
		msg("prctl() failed with errno = %d\n", errno);
		exit(EXIT_FAILURE);
	}
#endif
	MY_INIT(argv[0]);
	xb_regex_init();

#if MYSQL_VERSION_ID >= 50600
	system_charset_info= &my_charset_utf8_general_ci;
	key_map_full.set_all();
#endif

	/* scan options for group to load defaults from */
	{
		int	i;
		char*	optend;
		for (i=1; i < argc; i++) {
			optend = strcend(argv[i], '=');
			if (strncmp(argv[i], "--defaults-group", optend - argv[i]) == 0) {
				xb_load_default_groups[2] = defaults_group = optend + 1;
			}
		}
	}
	load_defaults("my", xb_load_default_groups, &argc, &argv);

	/* ignore unsupported options */
	{
	int i,j,argc_new,find;
	char *optend, *prev_found = NULL;
	argc_new = argc;

	j=1;
	for (i=1 ; i < argc ; i++) {
		uint count;
		struct my_option *opt= (struct my_option *) xb_long_options;
		optend= strcend((argv)[i], '=');
		if (!strncmp(argv[i], "--defaults-file", optend - argv[i]))
		{
			msg("xtrabackup: Error: --defaults-file "
			    "must be specified first on the command "
			    "line\n");
			exit(EXIT_FAILURE);
		}
		for (count= 0; opt->name; opt++) {
			if (!getopt_compare_strings(opt->name, (argv)[i] + 2,
				(uint)(optend - (argv)[i] - 2))) /* match found */
			{
				if (!opt->name[(uint)(optend - (argv)[i] - 2)]) {
					find = 1;
					goto next_opt;
				}
				if (!count) {
					count= 1;
					prev_found= (char *) opt->name;
				}
				else if (strcmp(prev_found, opt->name)) {
					count++;
				}
			}
		}
		find = count;
next_opt:
		if(!find){
			argc_new--;
		} else {
			(argv)[j]=(argv)[i];
			j++;
		}
	}
	argc = argc_new;
	argv[argc] = NULL;
	}

	if ((ho_error=handle_options(&argc, &argv, xb_long_options, get_one_option)))
		exit(ho_error);

	if (argc != 0) {
		msg("xtrabackup: Error: unknown argument: '%s'\n",
		    argv[0]);
		exit(EXIT_FAILURE);
	}

	if ((!xtrabackup_print_param) && (!xtrabackup_prepare) && (strcmp(mysql_data_home, "./") == 0)) {
		if (!xtrabackup_print_param)
			usage();
		msg("\nxtrabackup: Error: Please set parameter 'datadir'\n");
		exit(EXIT_FAILURE);
	}

	/* Ensure target dir is not relative to datadir */
	my_load_path(xtrabackup_real_target_dir, xtrabackup_target_dir, NULL);
	xtrabackup_target_dir= xtrabackup_real_target_dir;

#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
	/* temporary setting of enough size */
	srv_page_size_shift = UNIV_PAGE_SIZE_SHIFT_MAX;
	srv_page_size = UNIV_PAGE_SIZE_MAX;
#endif
#ifdef XTRADB_BASED
	srv_log_block_size = 512;
#endif
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

		sprintf(filename, "%s/%s", xtrabackup_incremental_basedir, XTRABACKUP_METADATA_FILENAME);

		if (!xtrabackup_read_metadata(filename)) {
			msg("xtrabackup: error: failed to read metadata from "
			    "%s\n", filename);
			exit(EXIT_FAILURE);
		}

		incremental_lsn = metadata_to_lsn;
		xtrabackup_incremental = xtrabackup_incremental_basedir; //dummy
	} else if (xtrabackup_prepare && xtrabackup_incremental_dir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_incremental_dir, XTRABACKUP_METADATA_FILENAME);

		if (!xtrabackup_read_metadata(filename)) {
			msg("xtrabackup: error: failed to read metadata from "
			    "%s\n", filename);
			exit(EXIT_FAILURE);
		}

		incremental_lsn = metadata_from_lsn;
		incremental_to_lsn = metadata_to_lsn;
		incremental_last_lsn = metadata_last_lsn;
		xtrabackup_incremental = xtrabackup_incremental_dir; //dummy

	} else {
		xtrabackup_incremental = NULL;
	}

	/* --print-param */
	if (xtrabackup_print_param) {
		/* === some variables from mysqld === */
		memset((G_PTR) &mysql_tmpdir_list, 0,
		       sizeof(mysql_tmpdir_list));

		if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
			exit(EXIT_FAILURE);

		printf("# This MySQL options file was generated by XtraBackup.\n");
		printf("[%s]\n", defaults_group);
		printf("datadir = \"%s\"\n", mysql_data_home);
		printf("tmpdir = \"%s\"\n", mysql_tmpdir_list.list[0]);
		printf("innodb_data_home_dir = \"%s\"\n",
			innobase_data_home_dir ? innobase_data_home_dir : mysql_data_home);
		printf("innodb_data_file_path = \"%s\"\n",
			innobase_data_file_path ? innobase_data_file_path : "ibdata1:10M:autoextend");
		printf("innodb_log_group_home_dir = \"%s\"\n",
			INNODB_LOG_DIR ? INNODB_LOG_DIR : mysql_data_home);
		printf("innodb_log_files_in_group = %ld\n", innobase_log_files_in_group);
		printf("innodb_log_file_size = %lld\n", innobase_log_file_size);
		printf("innodb_flush_method = \"%s\"\n",
		       (innobase_unix_file_flush_method != NULL) ?
		       innobase_unix_file_flush_method : "");
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
		printf("innodb_page_size = %lld\n", innobase_page_size);
#endif
#if defined(XTRADB_BASED) || MYSQL_VERSION_ID >= 50600
		printf("innodb_fast_checksum = %d\n", innobase_fast_checksum);
		printf("innodb_log_block_size = %lu\n", innobase_log_block_size);
		if (innobase_doublewrite_file != NULL) {
			printf("innodb_doublewrite_file = %s\n", innobase_doublewrite_file);
		}
#endif
#if MYSQL_VERSION_ID >= 50600
		if (srv_undo_dir) {

			printf("innodb_undo_directory = \"%s\"\n",
			       srv_undo_dir);
		}
		printf("innodb_undo_tablespaces = %lu\n", srv_undo_tablespaces);
		printf("innodb_checksum_algorithm = %s\n",
		       innodb_checksum_algorithm_names[srv_checksum_algorithm]
		       );
		printf("innodb_log_checksum_algorithm = %s\n",
		       innodb_checksum_algorithm_names[srv_log_checksum_algorithm]
		       );
#endif
		printf("innodb_buffer_pool_filename = \"%s\"\n",
			innobase_buffer_pool_filename ?
				innobase_buffer_pool_filename :
				"ib_buffer_pool");
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

	/* cannot execute both for now */
	{
		int num = 0;

		if (xtrabackup_backup) num++;
		if (xtrabackup_stats) num++;
		if (xtrabackup_prepare) num++;
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

	/* --backup */
	if (xtrabackup_backup)
		xtrabackup_backup_func();

	/* --stats */
	if (xtrabackup_stats)
		xtrabackup_stats_func();

	/* --prepare */
	if (xtrabackup_prepare)
		xtrabackup_prepare_func();

	xb_regex_end();

	exit(EXIT_SUCCESS);
}
