/******************************************************
XtraBackup: The another hot backup tool for InnoDB
(c) 2009 Percona Inc.
Created 3/3/2009 Yasufumi Kinoshita
*******************************************************/

#define XTRABACKUP_VERSION "alpha-0.3"

//#define XTRABACKUP_TARGET_IS_PLUGIN

#include <my_base.h>
#include <my_getopt.h>
#include <mysql_version.h>
#include <mysql_com.h>

#if (MYSQL_VERSION_ID < 50100)
#define GPTR gptr
#else /* MYSQL_VERSION_ID < 51000 */
#define GPTR uchar*
#endif

#include <univ.i>
#include <os0file.h>
#include <os0thread.h>
#include <srv0start.h>
#include <srv0srv.h>
#include <trx0roll.h>
#include <trx0trx.h>
#include <trx0sys.h>
#include <mtr0mtr.h>
#include <row0ins.h>
#include <row0mysql.h>
#include <row0sel.h>
#include <row0upd.h>
#include <log0log.h>
#include <lock0lock.h>
#include <dict0crea.h>
#include <btr0cur.h>
#include <btr0btr.h>
#include <fsp0fsp.h>
#include <sync0sync.h>
#include <fil0fil.h>
#include <trx0xa.h>

/* ==start === definition at fil0fil.c === */
// ##################################################################
// NOTE: We should check the following definitions fit to the source.
// ##################################################################

#ifndef INNODB_VERSION_SHORT
//5.0 5.1
/* File node of a tablespace or the log data space */
struct fil_node_struct {
        fil_space_t*    space;  /* backpointer to the space where this node
                                belongs */
        char*           name;   /* path to the file */
        ibool           open;   /* TRUE if file open */
        os_file_t       handle; /* OS handle to the file, if file open */
        ibool           is_raw_disk;/* TRUE if the 'file' is actually a raw
                                device or a raw disk partition */
        ulint           size;   /* size of the file in database pages, 0 if
                                not known yet; the possible last incomplete
                                megabyte may be ignored if space == 0 */
        ulint           n_pending;
                                /* count of pending i/o's on this file;
                                closing of the file is not allowed if
                                this is > 0 */
        ulint           n_pending_flushes;
                                /* count of pending flushes on this file;
                                closing of the file is not allowed if
                                this is > 0 */
        ib_longlong     modification_counter;/* when we write to the file we
                                increment this by one */
        ib_longlong     flush_counter;/* up to what modification_counter value
                                we have flushed the modifications to disk */
        UT_LIST_NODE_T(fil_node_t) chain;
                                /* link field for the file chain */
        UT_LIST_NODE_T(fil_node_t) LRU;
                                /* link field for the LRU list */
        ulint           magic_n;
};

struct fil_space_struct {
        char*           name;   /* space name = the path to the first file in
                                it */
        ulint           id;     /* space id */
        ib_longlong     tablespace_version;
                                /* in DISCARD/IMPORT this timestamp is used to
                                check if we should ignore an insert buffer
                                merge request for a page because it actually
                                was for the previous incarnation of the
                                space */
        ibool           mark;   /* this is set to TRUE at database startup if
                                the space corresponds to a table in the InnoDB
                                data dictionary; so we can print a warning of
                                orphaned tablespaces */
        ibool           stop_ios;/* TRUE if we want to rename the .ibd file of
                                tablespace and want to stop temporarily
                                posting of new i/o requests on the file */
        ibool           stop_ibuf_merges;
                                /* we set this TRUE when we start deleting a
                                single-table tablespace */
        ibool           is_being_deleted;
                                /* this is set to TRUE when we start
                                deleting a single-table tablespace and its
                                file; when this flag is set no further i/o
                                or flush requests can be placed on this space,
                                though there may be such requests still being
                                processed on this space */
        ulint           purpose;/* FIL_TABLESPACE, FIL_LOG, or FIL_ARCH_LOG */
        UT_LIST_BASE_NODE_T(fil_node_t) chain;
                                /* base node for the file chain */
        ulint           size;   /* space size in pages; 0 if a single-table
                                tablespace whose size we do not know yet;
                                last incomplete megabytes in data files may be
                                ignored if space == 0 */
        ulint           n_reserved_extents;
                                /* number of reserved free extents for
                                ongoing operations like B-tree page split */
        ulint           n_pending_flushes; /* this is > 0 when flushing
                                the tablespace to disk; dropping of the
                                tablespace is forbidden if this is > 0 */
        ulint           n_pending_ibuf_merges;/* this is > 0 when merging
                                insert buffer entries to a page so that we
                                may need to access the ibuf bitmap page in the
                                tablespade: dropping of the tablespace is
                                forbidden if this is > 0 */
        hash_node_t     hash;   /* hash chain node */
        hash_node_t     name_hash;/* hash chain the name_hash table */
        rw_lock_t       latch;  /* latch protecting the file space storage
                                allocation */
        UT_LIST_NODE_T(fil_space_t) unflushed_spaces;
                                /* list of spaces with at least one unflushed
                                file we have written to */
        ibool           is_in_unflushed_spaces; /* TRUE if this space is
                                currently in the list above */
        UT_LIST_NODE_T(fil_space_t) space_list;
                                /* list of all spaces */
        ibuf_data_t*    ibuf_data;
                                /* insert buffer data */
        ulint           magic_n;
};
typedef struct fil_system_struct        fil_system_t;
struct fil_system_struct {
        mutex_t         mutex;          /* The mutex protecting the cache */
        hash_table_t*   spaces;         /* The hash table of spaces in the
                                        system; they are hashed on the space
                                        id */
        hash_table_t*   name_hash;      /* hash table based on the space
                                        name */
        UT_LIST_BASE_NODE_T(fil_node_t) LRU;
                                        /* base node for the LRU list of the
                                        most recently used open files with no
                                        pending i/o's; if we start an i/o on
                                        the file, we first remove it from this
                                        list, and return it to the start of
                                        the list when the i/o ends;
                                        log files and the system tablespace are
                                        not put to this list: they are opened
                                        after the startup, and kept open until
                                        shutdown */
        UT_LIST_BASE_NODE_T(fil_space_t) unflushed_spaces;
                                        /* base node for the list of those
                                        tablespaces whose files contain
                                        unflushed writes; those spaces have
                                        at least one file node where
                                        modification_counter > flush_counter */
        ulint           n_open;         /* number of files currently open */
        ulint           max_n_open;     /* n_open is not allowed to exceed
                                        this */
        ib_longlong     modification_counter;/* when we write to a file we
                                        increment this by one */
        ulint           max_assigned_id;/* maximum space id in the existing
                                        tables, or assigned during the time
                                        mysqld has been up; at an InnoDB
                                        startup we scan the data dictionary
                                        and set here the maximum of the
                                        space id's of the tables there */
        ib_longlong     tablespace_version;
                                        /* a counter which is incremented for
                                        every space object memory creation;
                                        every space mem object gets a
                                        'timestamp' from this; in DISCARD/
                                        IMPORT this is used to check if we
                                        should ignore an insert buffer merge
                                        request */
        UT_LIST_BASE_NODE_T(fil_space_t) space_list;
                                        /* list of all file spaces */
};
#else
//Plugin ?
#endif /* INNODB_VERSION_SHORT */

extern fil_system_t*   fil_system;

/* ==end=== definition  at fil0fil.c === */


my_bool innodb_inited= 0;

/* === xtrabackup specific options === */
char xtrabackup_real_target_dir[FN_REFLEN] = "./xtrabackup_backupfiles/";
char *xtrabackup_target_dir= xtrabackup_real_target_dir;
my_bool xtrabackup_backup = FALSE;
my_bool xtrabackup_prepare = FALSE;
my_bool xtrabackup_print_param = FALSE;

my_bool xtrabackup_suspend_at_end = FALSE;
longlong xtrabackup_use_memory = 100*1024*1024L;
my_bool xtrabackup_create_ib_logfile = FALSE;

long xtrabackup_throttle = 0; /* 0:unlimited */
lint io_ticket;
os_event_t wait_throttle = NULL;

my_bool xtrabackup_stream = FALSE;

static ulint		n[SRV_MAX_N_IO_THREADS + 5];
static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 5];

dulint checkpoint_lsn_start;
dulint checkpoint_no_start;
dulint log_copy_scanned_lsn;
ib_longlong log_copy_offset = 0;
ibool log_copying = TRUE;
ibool log_copying_running = FALSE;
ibool log_copying_succeed = FALSE;

ibool xtrabackup_logfile_is_renamed = FALSE;

/* === sharing with thread === */
os_file_t       dst_log = -1;
char            dst_log_path[FN_REFLEN];

/* === some variables from mysqld === */
char mysql_real_data_home[FN_REFLEN] = "./";
char *mysql_data_home= mysql_real_data_home;
static char mysql_data_home_buff[2];

char *opt_mysql_tmpdir = NULL;
MY_TMPDIR mysql_tmpdir_list;

/* === static parameters in ha_innodb.cc */

#define HA_INNOBASE_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_INNOBASE_RANGE_COUNT	  100

ulong 	innobase_large_page_size = 0;

/* The default values for the following, type long or longlong, start-up
parameters are declared in mysqld.cc: */

long innobase_additional_mem_pool_size = 1*1024*1024L;
long innobase_buffer_pool_awe_mem_mb = 0;
long innobase_file_io_threads = 4;
long innobase_force_recovery = 0;
long innobase_lock_wait_timeout = 50;
long innobase_log_buffer_size = 1024*1024L;
long innobase_log_files_in_group = 2;
long innobase_log_files_in_group_backup;
long innobase_mirrored_log_groups = 1;
long innobase_open_files = 300L;

longlong innobase_buffer_pool_size = 8*1024*1024L;
longlong innobase_log_file_size = 5*1024*1024L;
longlong innobase_log_file_size_backup;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

char*	innobase_data_home_dir			= NULL;
char*	innobase_data_file_path 		= NULL;
char*	innobase_log_group_home_dir		= NULL;
char*	innobase_log_group_home_dir_backup	= NULL;
char*	innobase_log_arch_dir			= NULL;/* unused */
/* The following has a misleading name: starting from 4.0.5, this also
affects Windows: */
char*	innobase_unix_file_flush_method		= NULL;

/* Below we have boolean-valued start-up parameters, and their default
values */

ulong	innobase_fast_shutdown			= 0;
my_bool innobase_log_archive			= FALSE;/* unused */
my_bool innobase_use_doublewrite    = TRUE;
my_bool innobase_use_checksums      = TRUE;
my_bool innobase_use_large_pages    = FALSE;
my_bool	innobase_use_native_aio			= FALSE;
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


/* ======== for option and variables ======== */

enum options_xtrabackup
{
  OPT_XTRA_TARGET_DIR=256,
  OPT_XTRA_BACKUP,
  OPT_XTRA_PREPARE,
  OPT_XTRA_PRINT_PARAM,
  OPT_XTRA_SUSPEND_AT_END,
  OPT_XTRA_USE_MEMORY,
  OPT_XTRA_THROTTLE,
  OPT_XTRA_STREAM,
  OPT_XTRA_CREATE_IB_LOGFILE,
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
  OPT_INNODB_FORCE_RECOVERY,
  OPT_INNODB_LOCK_WAIT_TIMEOUT,
  OPT_INNODB_LOG_BUFFER_SIZE,
  OPT_INNODB_LOG_FILE_SIZE,
  OPT_INNODB_LOG_FILES_IN_GROUP,
  OPT_INNODB_MIRRORED_LOG_GROUPS,
  OPT_INNODB_OPEN_FILES,
  OPT_INNODB_SYNC_SPIN_LOOPS,
  OPT_INNODB_THREAD_CONCURRENCY,
  OPT_INNODB_THREAD_SLEEP_DELAY
};

static struct my_option my_long_options[] =
{
  {"target-dir", OPT_XTRA_TARGET_DIR, "destination directory", (GPTR*) &xtrabackup_target_dir,
   (GPTR*) &xtrabackup_target_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"backup", OPT_XTRA_BACKUP, "take backup to target-dir",
   (GPTR*) &xtrabackup_backup, (GPTR*) &xtrabackup_backup,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"prepare", OPT_XTRA_PREPARE, "prepare a backup for starting mysql server on the backup.",
   (GPTR*) &xtrabackup_prepare, (GPTR*) &xtrabackup_prepare,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"print-param", OPT_XTRA_PRINT_PARAM, "print parameter of mysqld needed for copyback.",
   (GPTR*) &xtrabackup_print_param, (GPTR*) &xtrabackup_print_param,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"use-memory", OPT_XTRA_USE_MEMORY, "The value is used instead of buffer_pool_size",
   (GPTR*) &xtrabackup_use_memory, (GPTR*) &xtrabackup_use_memory,
   0, GET_LL, REQUIRED_ARG, 100*1024*1024L, 1024*1024L, LONGLONG_MAX, 0,
   1024*1024L, 0},
  {"suspend-at-end", OPT_XTRA_SUSPEND_AT_END, "creates a file 'xtrabackup_suspended' and waits until the user deletes that file at the end of '--backup'",
   (GPTR*) &xtrabackup_suspend_at_end, (GPTR*) &xtrabackup_suspend_at_end,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"throttle", OPT_XTRA_THROTTLE, "limit count of IO operations (pairs of read&write) per second to IOS values (for '--backup')",
   (GPTR*) &xtrabackup_throttle, (GPTR*) &xtrabackup_throttle,
   0, GET_LONG, REQUIRED_ARG, 0, 0, LONG_MAX, 0, 1, 0},
  {"log-stream", OPT_XTRA_STREAM, "outputs the contents of 'xtrabackup_logfile' to stdout only until the file 'xtrabackup_suspended' deleted (for '--backup').",
   (GPTR*) &xtrabackup_stream, (GPTR*) &xtrabackup_stream,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"create-ib-logfile", OPT_XTRA_CREATE_IB_LOGFILE, "** not work for now** creates ib_logfile* also after '--prepare'. ### If you want create ib_logfile*, only re-execute this command in same options. ###",
   (GPTR*) &xtrabackup_create_ib_logfile, (GPTR*) &xtrabackup_create_ib_logfile,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

  {"datadir", 'h', "Path to the database root.", (GPTR*) &mysql_data_home,
   (GPTR*) &mysql_data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't',
   "Path for temporary files. Several paths may be specified, separated by a "
#if defined(__WIN__) || defined(OS2) || defined(__NETWARE__)
   "semicolon (;)"
#else
   "colon (:)"
#endif
   ", in this case they are used in a round-robin fashion.",
   (GPTR*) &opt_mysql_tmpdir,
   (GPTR*) &opt_mysql_tmpdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {"innodb_adaptive_hash_index", OPT_INNODB_ADAPTIVE_HASH_INDEX,
   "Enable InnoDB adaptive hash index (enabled by default).  "
   "Disable with --skip-innodb-adaptive-hash-index.",
   (GPTR*) &innobase_adaptive_hash_index,
   (GPTR*) &innobase_adaptive_hash_index,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"innodb_additional_mem_pool_size", OPT_INNODB_ADDITIONAL_MEM_POOL_SIZE,
   "Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.",
   (GPTR*) &innobase_additional_mem_pool_size,
   (GPTR*) &innobase_additional_mem_pool_size, 0, GET_LONG, REQUIRED_ARG,
   1*1024*1024L, 512*1024L, LONG_MAX, 0, 1024, 0},
  {"innodb_autoextend_increment", OPT_INNODB_AUTOEXTEND_INCREMENT,
   "Data file autoextend increment in megabytes",
   (GPTR*) &srv_auto_extend_increment,
   (GPTR*) &srv_auto_extend_increment,
   0, GET_ULONG, REQUIRED_ARG, 8L, 1L, 1000L, 0, 1L, 0},
  {"innodb_buffer_pool_size", OPT_INNODB_BUFFER_POOL_SIZE,
   "The size of the memory buffer InnoDB uses to cache data and indexes of its tables.",
   (GPTR*) &innobase_buffer_pool_size, (GPTR*) &innobase_buffer_pool_size, 0,
   GET_LL, REQUIRED_ARG, 8*1024*1024L, 1024*1024L, LONGLONG_MAX, 0,
   1024*1024L, 0},
  {"innodb_checksums", OPT_INNODB_CHECKSUMS, "Enable InnoDB checksums validation (enabled by default). \
Disable with --skip-innodb-checksums.", (GPTR*) &innobase_use_checksums,
   (GPTR*) &innobase_use_checksums, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
/*
  {"innodb_commit_concurrency", OPT_INNODB_COMMIT_CONCURRENCY,
   "Helps in performance tuning in heavily concurrent environments.",
   (GPTR*) &srv_commit_concurrency, (GPTR*) &srv_commit_concurrency,
   0, GET_ULONG, REQUIRED_ARG, 0, 0, 1000, 0, 1, 0},
*/
/*
  {"innodb_concurrency_tickets", OPT_INNODB_CONCURRENCY_TICKETS,
   "Number of times a thread is allowed to enter InnoDB within the same \
    SQL query after it has once got the ticket",
   (GPTR*) &srv_n_free_tickets_to_enter,
   (GPTR*) &srv_n_free_tickets_to_enter,
   0, GET_ULONG, REQUIRED_ARG, 500L, 1L, ULONG_MAX, 0, 1L, 0},
*/
  {"innodb_data_file_path", OPT_INNODB_DATA_FILE_PATH,
   "Path to individual files and their sizes.", (GPTR*) &innobase_data_file_path,
   (GPTR*) &innobase_data_file_path, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_data_home_dir", OPT_INNODB_DATA_HOME_DIR,
   "The common part for InnoDB table spaces.", (GPTR*) &innobase_data_home_dir,
   (GPTR*) &innobase_data_home_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},
  {"innodb_doublewrite", OPT_INNODB_DOUBLEWRITE, "Enable InnoDB doublewrite buffer (enabled by default). \
Disable with --skip-innodb-doublewrite.", (GPTR*) &innobase_use_doublewrite,
   (GPTR*) &innobase_use_doublewrite, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
/*
  {"innodb_fast_shutdown", OPT_INNODB_FAST_SHUTDOWN,
   "Speeds up the shutdown process of the InnoDB storage engine. Possible "
   "values are 0, 1 (faster)"
   " or 2 (fastest - crash-like)"
   ".",
   (GPTR*) &innobase_fast_shutdown,
   (GPTR*) &innobase_fast_shutdown, 0, GET_ULONG, OPT_ARG, 1, 0,
   2, 0, 0, 0},
*/
  {"innodb_file_io_threads", OPT_INNODB_FILE_IO_THREADS,
   "Number of file I/O threads in InnoDB.", (GPTR*) &innobase_file_io_threads,
   (GPTR*) &innobase_file_io_threads, 0, GET_LONG, REQUIRED_ARG, 4, 4, 64, 0,
   1, 0},
  {"innodb_file_per_table", OPT_INNODB_FILE_PER_TABLE,
   "Stores each InnoDB table to an .ibd file in the database dir.",
   (GPTR*) &innobase_file_per_table,
   (GPTR*) &innobase_file_per_table, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_flush_log_at_trx_commit", OPT_INNODB_FLUSH_LOG_AT_TRX_COMMIT,
   "Set to 0 (write and flush once per second), 1 (write and flush at each commit) or 2 (write at commit, flush once per second).",
   (GPTR*) &srv_flush_log_at_trx_commit,
   (GPTR*) &srv_flush_log_at_trx_commit,
   0, GET_ULONG, OPT_ARG,  1, 0, 2, 0, 0, 0},
  {"innodb_flush_method", OPT_INNODB_FLUSH_METHOD,
   "With which method to flush data.", (GPTR*) &innobase_unix_file_flush_method,
   (GPTR*) &innobase_unix_file_flush_method, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},

/* ####### Should we use this option? ####### */
  {"innodb_force_recovery", OPT_INNODB_FORCE_RECOVERY,
   "Helps to save your data in case the disk image of the database becomes corrupt.",
   (GPTR*) &innobase_force_recovery, (GPTR*) &innobase_force_recovery, 0,
   GET_LONG, REQUIRED_ARG, 0, 0, 6, 0, 1, 0},

  {"innodb_lock_wait_timeout", OPT_INNODB_LOCK_WAIT_TIMEOUT,
   "Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back.",
   (GPTR*) &innobase_lock_wait_timeout, (GPTR*) &innobase_lock_wait_timeout,
   0, GET_LONG, REQUIRED_ARG, 50, 1, 1024 * 1024 * 1024, 0, 1, 0},
/*
  {"innodb_locks_unsafe_for_binlog", OPT_INNODB_LOCKS_UNSAFE_FOR_BINLOG,
   "Force InnoDB not to use next-key locking. Instead use only row-level locking",
   (GPTR*) &innobase_locks_unsafe_for_binlog,
   (GPTR*) &innobase_locks_unsafe_for_binlog, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
*/
/*
  {"innodb_log_arch_dir", OPT_INNODB_LOG_ARCH_DIR,
   "Where full logs should be archived.", (GPTR*) &innobase_log_arch_dir,
   (GPTR*) &innobase_log_arch_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
*/
  {"innodb_log_buffer_size", OPT_INNODB_LOG_BUFFER_SIZE,
   "The size of the buffer which InnoDB uses to write log to the log files on disk.",
   (GPTR*) &innobase_log_buffer_size, (GPTR*) &innobase_log_buffer_size, 0,
   GET_LONG, REQUIRED_ARG, 1024*1024L, 256*1024L, LONG_MAX, 0, 1024, 0},
  {"innodb_log_file_size", OPT_INNODB_LOG_FILE_SIZE,
   "Size of each log file in a log group.",
   (GPTR*) &innobase_log_file_size, (GPTR*) &innobase_log_file_size, 0,
   GET_LL, REQUIRED_ARG, 5*1024*1024L, 1*1024*1024L, LONGLONG_MAX, 0,
   1024*1024L, 0},
  {"innodb_log_files_in_group", OPT_INNODB_LOG_FILES_IN_GROUP,
   "Number of log files in the log group. InnoDB writes to the files in a circular fashion. Value 3 is recommended here.",
   (GPTR*) &innobase_log_files_in_group, (GPTR*) &innobase_log_files_in_group,
   0, GET_LONG, REQUIRED_ARG, 2, 2, 100, 0, 1, 0},
  {"innodb_log_group_home_dir", OPT_INNODB_LOG_GROUP_HOME_DIR,
   "Path to InnoDB log files.", (GPTR*) &innobase_log_group_home_dir,
   (GPTR*) &innobase_log_group_home_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"innodb_max_dirty_pages_pct", OPT_INNODB_MAX_DIRTY_PAGES_PCT,
   "Percentage of dirty pages allowed in bufferpool.", (GPTR*) &srv_max_buf_pool_modified_pct,
   (GPTR*) &srv_max_buf_pool_modified_pct, 0, GET_ULONG, REQUIRED_ARG, 90, 0, 100, 0, 0, 0},
/*
  {"innodb_max_purge_lag", OPT_INNODB_MAX_PURGE_LAG,
   "Desired maximum length of the purge queue (0 = no limit)",
   (GPTR*) &srv_max_purge_lag,
   (GPTR*) &srv_max_purge_lag, 0, GET_ULONG, REQUIRED_ARG, 0, 0, ULONG_MAX,
   0, 1L, 0},
*/
/*
  {"innodb_mirrored_log_groups", OPT_INNODB_MIRRORED_LOG_GROUPS,
   "Number of identical copies of log groups we keep for the database. Currently this should be set to 1.",
   (GPTR*) &innobase_mirrored_log_groups,
   (GPTR*) &innobase_mirrored_log_groups, 0, GET_LONG, REQUIRED_ARG, 1, 1, 10,
   0, 1, 0},
*/
  {"innodb_open_files", OPT_INNODB_OPEN_FILES,
   "How many files at the maximum InnoDB keeps open at the same time.",
   (GPTR*) &innobase_open_files, (GPTR*) &innobase_open_files, 0,
   GET_LONG, REQUIRED_ARG, 300L, 10L, LONG_MAX, 0, 1L, 0},
/*
  {"innodb_rollback_on_timeout", OPT_INNODB_ROLLBACK_ON_TIMEOUT,
   "Roll back the complete transaction on lock wait timeout, for 4.x compatibility (disabled by default)",
   (GPTR*) &innobase_rollback_on_timeout, (GPTR*) &innobase_rollback_on_timeout,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
*/
/*
  {"innodb_status_file", OPT_INNODB_STATUS_FILE,
   "Enable SHOW INNODB STATUS output in the innodb_status.<pid> file",
   (GPTR*) &innobase_create_status_file, (GPTR*) &innobase_create_status_file,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
*/
/*
  {"innodb_sync_spin_loops", OPT_INNODB_SYNC_SPIN_LOOPS,
   "Count of spin-loop rounds in InnoDB mutexes",
   (GPTR*) &srv_n_spin_wait_rounds,
   (GPTR*) &srv_n_spin_wait_rounds,
   0, GET_ULONG, REQUIRED_ARG, 20L, 0L, ULONG_MAX, 0, 1L, 0},
*/
/*
  {"innodb_thread_concurrency", OPT_INNODB_THREAD_CONCURRENCY,
   "Helps in performance tuning in heavily concurrent environments. "
   "Sets the maximum number of threads allowed inside InnoDB. Value 0"
   " will disable the thread throttling.",
   (GPTR*) &srv_thread_concurrency, (GPTR*) &srv_thread_concurrency,
   0, GET_ULONG, REQUIRED_ARG, 8, 0, 1000, 0, 1, 0},
*/
/*
  {"innodb_thread_sleep_delay", OPT_INNODB_THREAD_SLEEP_DELAY,
   "Time of innodb thread sleeping before joining InnoDB queue (usec). Value 0"
    " disable a sleep",
   (GPTR*) &srv_thread_sleep_delay,
   (GPTR*) &srv_thread_sleep_delay,
   0, GET_ULONG, REQUIRED_ARG, 10000L, 0L, ULONG_MAX, 0, 1L, 0},
*/

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static const char *load_default_groups[]= { "mysqld","xtrabackup",0 };

static void print_version(void)
{
  printf("%s  Ver %s for %s %s (%s)\n" ,my_progname,
	  XTRABACKUP_VERSION, MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage(void)
{
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");

  printf("Usage: [%s --backup | %s --prepare] [OPTIONS]\n",my_progname,my_progname);
  print_defaults("my",load_default_groups);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case 'h':
    strmake(mysql_real_data_home,argument, sizeof(mysql_real_data_home)-1);
    mysql_data_home= mysql_real_data_home;
    break;
  case OPT_XTRA_TARGET_DIR:
    strmake(xtrabackup_real_target_dir,argument, sizeof(xtrabackup_real_target_dir)-1);
    xtrabackup_target_dir= xtrabackup_real_target_dir;
    break;
  case '?':
    usage();
    exit(0);
    break;
  default:
    break;
  }
  return 0;
}

/* ================ Dummys =================== */

ibool
thd_is_replication_slave_thread(
	void*	thd)
{
	fprintf(stderr, "thd_is_replication_slave_thread() is called\n");
}

ibool
thd_has_edited_nontrans_tables(
	void*	thd)
{
	fprintf(stderr, "thd_has_edited_nontrans_tables() is called\n");
}

ibool
thd_is_select(
	const void*	thd)
{
	fprintf(stderr, "thd_is_select() is called\n");
}

void
innobase_mysql_prepare_print_arbitrary_thd(void)
{
	/* do nothing */
}

void
innobase_mysql_end_print_arbitrary_thd(void)
{
	/* do nothing */
}

void
innobase_mysql_print_thd(
	FILE*   f,		
	void*   input_thd,
	uint	max_query_len)
{
	fprintf(stderr, "innobase_mysql_print_thd() is called\n");
}

void
innobase_get_cset_width(
	ulint	cset,
	ulint*	mbminlen,
	ulint*	mbmaxlen)
{
	CHARSET_INFO*	cs;
	ut_ad(cset < 256);
	ut_ad(mbminlen);
	ut_ad(mbmaxlen);

	cs = all_charsets[cset];
	if (cs) {
		*mbminlen = cs->mbminlen;
		*mbmaxlen = cs->mbmaxlen;
	} else {
		ut_a(cset == 0);
		*mbminlen = *mbmaxlen = 0;
	}
}

void
innobase_convert_from_table_id(
	char*	to,
	const char*	from,
	ulint	len)
{
	fprintf(stderr, "innobase_convert_from_table_id() is called\n");
}

void
innobase_convert_from_id(
	char*	to,
	const char*	from,
	ulint	len)
{
	fprintf(stderr, "innobase_convert_from_id() is called\n");
}

int
innobase_strcasecmp(
	const char*	a,
	const char*	b)
{
	return(my_strcasecmp(&my_charset_utf8_general_ci, a, b));
}

void
innobase_casedn_str(
	char*	a)
{
	my_casedn_str(&my_charset_utf8_general_ci, a);
}

struct charset_info_st*
innobase_get_charset(
	void*   mysql_thd)
{
	fprintf(stderr, "innobase_get_charset() is called\n");
}

int
innobase_mysql_tmpfile(void)
{
	char	filename[FN_REFLEN];
	int	fd2 = -1;
	File	fd = create_temp_file(filename, my_tmpdir(&mysql_tmpdir_list), "ib",
#ifdef __WIN__
				O_BINARY | O_TRUNC | O_SEQUENTIAL |
				O_TEMPORARY | O_SHORT_LIVED |
#endif /* __WIN__ */
				O_CREAT | O_EXCL | O_RDWR,
				MYF(MY_WME));
	if (fd >= 0) {
#ifndef __WIN__
		/* On Windows, open files cannot be removed, but files can be
		created with the O_TEMPORARY flag to the same effect
		("delete on close"). */
		unlink(filename);
#endif /* !__WIN__ */
		/* Copy the file descriptor, so that the additional resources
		allocated by create_temp_file() can be freed by invoking
		my_close().

		Because the file descriptor returned by this function
		will be passed to fdopen(), it will be closed by invoking
		fclose(), which in turn will invoke close() instead of
		my_close(). */
		fd2 = dup(fd);
		if (fd2 < 0) {
			fprintf(stderr, "Got error %d on dup\n",fd2);
                }
		my_close(fd, MYF(MY_WME));
	}
	return(fd2);
}

void
innobase_invalidate_query_cache(
	trx_t*	trx,
	char*	full_name,
	ulint	full_name_len)
{
	/* do nothing */
}

int
mysql_get_identifier_quote_char(
	trx_t*		trx,
	const char*	name,
	ulint		namelen)
{
	return '"';
}

void
innobase_print_identifier(
	FILE*	f,
	trx_t*	trx,
	ibool	table_id,
	const char*	name,
	ulint	namelen)
{
        const char*     s       = name;
        int             q;

        q = '"';

        const char*     e = s + namelen;
        putc(q, f);
        while (s < e) {
                int     c = *s++;
                if (c == q) {
                        putc(c, f);
                }
                putc(c, f);
        }
        putc(q, f);
}

ibool
trx_is_interrupted(
	trx_t*	trx)
{
	/* There are no mysql_thd */
	return(FALSE);
}

int
innobase_mysql_cmp(
	int		mysql_type,
	uint		charset_number,
	unsigned char*	a,
	unsigned int	a_length,
	unsigned char*	b,
	unsigned int	b_length)
{
	CHARSET_INFO*		charset;
	enum enum_field_types	mysql_tp;
	int                     ret;

	DBUG_ASSERT(a_length != UNIV_SQL_NULL);
	DBUG_ASSERT(b_length != UNIV_SQL_NULL);

	mysql_tp = (enum enum_field_types) mysql_type;

	switch (mysql_tp) {

        case MYSQL_TYPE_BIT:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
	case FIELD_TYPE_TINY_BLOB:
	case FIELD_TYPE_MEDIUM_BLOB:
	case FIELD_TYPE_BLOB:
	case FIELD_TYPE_LONG_BLOB:
        case MYSQL_TYPE_VARCHAR:
		/* Use the charset number to pick the right charset struct for
		the comparison. Since the MySQL function get_charset may be
		slow before Bar removes the mutex operation there, we first
		look at 2 common charsets directly. */

		if (charset_number == default_charset_info->number) {
			charset = default_charset_info;
		} else if (charset_number == my_charset_latin1.number) {
			charset = &my_charset_latin1;
		} else {
			charset = get_charset(charset_number, MYF(MY_WME));

			if (charset == NULL) {
			  fprintf(stderr, "InnoDB needs charset %lu for doing "
					  "a comparison, but MySQL cannot "
					  "find that charset.\n",
					  (ulong) charset_number);
				ut_a(0);
			}
		}

                /* Starting from 4.1.3, we use strnncollsp() in comparisons of
                non-latin1_swedish_ci strings. NOTE that the collation order
                changes then: 'b\0\0...' is ordered BEFORE 'b  ...'. Users
                having indexes on such data need to rebuild their tables! */

                ret = charset->coll->strnncollsp(charset,
                                  a, a_length,
                                                 b, b_length, 0);
		if (ret < 0) {
		        return(-1);
		} else if (ret > 0) {
		        return(1);
		} else {
		        return(0);
	        }
	default:
		assert(0);
	}

	return(0);
}

ulint
innobase_get_at_most_n_mbchars(
	ulint charset_id,
	ulint prefix_len,
	ulint data_len,
	const char* str)
{
	ulint char_length;	/* character length in bytes */
	ulint n_chars;		/* number of characters in prefix */
	CHARSET_INFO* charset;	/* charset used in the field */

	charset = get_charset((uint) charset_id, MYF(MY_WME));

	ut_ad(charset);
	ut_ad(charset->mbmaxlen);

	/* Calculate how many characters at most the prefix index contains */

	n_chars = prefix_len / charset->mbmaxlen;

	/* If the charset is multi-byte, then we must find the length of the
	first at most n chars in the string. If the string contains less
	characters than n, then we return the length to the end of the last
	character. */

	if (charset->mbmaxlen > 1) {
		/* my_charpos() returns the byte length of the first n_chars
		characters, or a value bigger than the length of str, if
		there were not enough full characters in str.

		Why does the code below work:
		Suppose that we are looking for n UTF-8 characters.

		1) If the string is long enough, then the prefix contains at
		least n complete UTF-8 characters + maybe some extra
		characters + an incomplete UTF-8 character. No problem in
		this case. The function returns the pointer to the
		end of the nth character.

		2) If the string is not long enough, then the string contains
		the complete value of a column, that is, only complete UTF-8
		characters, and we can store in the column prefix index the
		whole string. */

		char_length = my_charpos(charset, str,
						str + data_len, (int) n_chars);
		if (char_length > data_len) {
			char_length = data_len;
		}
	} else {
		if (data_len < prefix_len) {
			char_length = data_len;
		} else {
			char_length = prefix_len;
		}
	}

	return(char_length);
}

ibool
innobase_query_is_update(void)
{
	fprintf(stderr, "innobase_query_is_update() is called\n");
	return(0);
}

/* control innodb */

my_bool
innodb_init_param(void)
{
	/* === some variables from mysqld === */
	bzero((GPTR) &mysql_tmpdir_list, sizeof(mysql_tmpdir_list));

	if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
		exit(1);

	/* dummy for initialize all_charsets[] */
	get_charset_name(0);


	/* innobase_init */

	static char	current_dir[3];		/* Set if using current lib */
	int		err;
	my_bool		ret;
	char 	        *default_path;

	/* Check that values don't overflow on 32-bit systems. */
	if (sizeof(ulint) == 4) {
		if (xtrabackup_use_memory > UINT_MAX32) {
			fprintf(stderr,
				"use-memory can't be over 4GB"
				" on 32-bit systems\n");
		}

		if (innobase_buffer_pool_size > UINT_MAX32) {
			fprintf(stderr,
				"innobase_buffer_pool_size can't be over 4GB"
				" on 32-bit systems\n");

			goto error;
		}

		if (innobase_log_file_size > UINT_MAX32) {
			fprintf(stderr,
				"innobase_log_file_size can't be over 4GB"
				" on 32-bit systemsi\n");

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

//	if (specialflag & SPECIAL_NO_PRIOR) {
	        srv_set_thread_priorities = FALSE;
//	} else {
//	        srv_set_thread_priorities = TRUE;
//	        srv_query_thread_priority = QUERY_PRIOR;
//	}

	/* Set InnoDB initialization parameters according to the values
	read from MySQL .cnf file */

	/*--------------- Data files -------------------------*/

	/* The default dir for data files is the datadir of MySQL */

	srv_data_home = (innobase_data_home_dir ? innobase_data_home_dir :
			 default_path);

	/* Set default InnoDB data file size to 10 MB and let it be
  	auto-extending. Thus users can use InnoDB in >= 4.0 without having
	to specify any startup options. */

	if (!innobase_data_file_path) {
  		innobase_data_file_path = (char*) "ibdata1:10M:autoextend";
	}

	/* Since InnoDB edits the argument in the next call, we make another
	copy of it: */

	internal_innobase_data_file_path = strdup(innobase_data_file_path);

	ret = (my_bool) srv_parse_data_file_paths_and_sizes(
				internal_innobase_data_file_path,
				&srv_data_file_names,
				&srv_data_file_sizes,
				&srv_data_file_is_raw_partition,
				&srv_n_data_files,
				&srv_auto_extend_last_data_file,
				&srv_last_file_size_max);
	if (ret == FALSE) {
	  	fprintf(stderr,
			"InnoDB: syntax error in innodb_data_file_path\n");
	  	free(internal_innobase_data_file_path);
                goto error;
	}

	/* -------------- Log files ---------------------------*/

	/* The default dir for log files is the datadir of MySQL */

	if (!innobase_log_group_home_dir) {
	  	innobase_log_group_home_dir = default_path;
	}

#ifdef UNIV_LOG_ARCHIVE
	/* Since innodb_log_arch_dir has no relevance under MySQL,
	starting from 4.0.6 we always set it the same as
	innodb_log_group_home_dir: */

	innobase_log_arch_dir = innobase_log_group_home_dir;

	srv_arch_dir = innobase_log_arch_dir;
#endif /* UNIG_LOG_ARCHIVE */

	ret = (my_bool)
		srv_parse_log_group_home_dirs(innobase_log_group_home_dir,
						&srv_log_group_home_dirs);

	if (ret == FALSE || innobase_mirrored_log_groups != 1) {
	  fprintf(stderr, "syntax error in innodb_log_group_home_dir, or a "
			  "wrong number of mirrored log groups\n");

	  	free(internal_innobase_data_file_path);
                goto error;
	}

	/* --------------------------------------------------*/

	srv_file_flush_method_str = innobase_unix_file_flush_method;

	srv_n_log_groups = (ulint) innobase_mirrored_log_groups;
	srv_n_log_files = (ulint) innobase_log_files_in_group;
	srv_log_file_size = (ulint) innobase_log_file_size;

#ifdef UNIV_LOG_ARCHIVE
	srv_log_archive_on = (ulint) innobase_log_archive;
#endif /* UNIV_LOG_ARCHIVE */
	srv_log_buffer_size = (ulint) innobase_log_buffer_size;

        /* We set srv_pool_size here in units of 1 kB. InnoDB internally
        changes the value so that it becomes the number of database pages. */

        if (innobase_buffer_pool_awe_mem_mb == 0) {
                /* Careful here: we first convert the signed long int to ulint
                and only after that divide */

                //srv_pool_size = ((ulint) innobase_buffer_pool_size) / 1024;
		srv_pool_size = ((ulint) xtrabackup_use_memory) / 1024;
        } else {
                srv_use_awe = TRUE;
                srv_pool_size = (ulint)
                                (1024 * innobase_buffer_pool_awe_mem_mb);
                srv_awe_window_size = (ulint) innobase_buffer_pool_size;

                /* Note that what the user specified as
                innodb_buffer_pool_size is actually the AWE memory window
                size in this case, and the real buffer pool size is
                determined by .._awe_mem_mb. */
        }

	srv_mem_pool_size = (ulint) innobase_additional_mem_pool_size;

	srv_n_file_io_threads = (ulint) innobase_file_io_threads;

	srv_lock_wait_timeout = (ulint) innobase_lock_wait_timeout;
	srv_force_recovery = (ulint) innobase_force_recovery;

	srv_use_doublewrite_buf = (ibool) innobase_use_doublewrite;
	srv_use_checksums = (ibool) innobase_use_checksums;

	srv_use_adaptive_hash_indexes = (ibool) innobase_adaptive_hash_index;

	os_use_large_pages = (ibool) innobase_use_large_pages;
	os_large_page_size = (ulint) innobase_large_page_size;

	row_rollback_on_timeout = (ibool) innobase_rollback_on_timeout;

	srv_file_per_table = (ibool) innobase_file_per_table;
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

	ut_a(0 == strcmp((char*)my_charset_latin1.name,
						(char*)"latin1_swedish_ci"));
	memcpy(srv_latin1_ordering, my_charset_latin1.sort_order, 256);

	/* Since we in this module access directly the fields of a trx
        struct, and due to different headers and flags it might happen that
	mutex_t has a different size in this module and in InnoDB
	modules, we check at run time that the size is the same in
	these compilation modules. */

	srv_sizeof_trx_t_in_ha_innodb_cc = sizeof(trx_t);

	return(FALSE);

error:
	fprintf(stderr, "innodb_init_param(): Error occured.\n");
	return(TRUE);
}

my_bool
innodb_init(void)
{
	int	err;

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
	fprintf(stderr, "innodb_init(): Error occured.\n");
	return(TRUE);
}

my_bool
innodb_end(void)
{
	srv_fast_shutdown = (ulint) innobase_fast_shutdown;
	innodb_inited = 0;
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
	fprintf(stderr, "innodb_end(): Error occured.\n");
	return(TRUE);
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


/* TODO: We may tune the behavior (e.g. by fil_aio)*/
#define COPY_CHUNK 64

my_bool
xtrabackup_copy_datafile(fil_node_t* node)
{
	os_file_t	src_file = -1;
	os_file_t	dst_file = -1;
	char    dst_path[FN_REFLEN];
	ibool		success;
	byte*		page;
	byte*		buf2 = NULL;
	dulint		flush_lsn;
	ulint		space_id;
	ib_longlong	file_size;
	ib_longlong	offset;
	ibool		src_exist = TRUE;

	sprintf(dst_path, "%s%s", xtrabackup_target_dir, strstr(node->name, "/"));

	/* open src_file*/
	if (!node->open) {
		src_file = os_file_create_simple_no_error_handling(
						node->name, OS_FILE_OPEN,
						OS_FILE_READ_ONLY, &success);
		if (!success) {
			/* The following call prints an error message */
			os_file_get_last_error(TRUE);

			ut_print_timestamp(stderr);

			fprintf(stderr,
"  InnoDB: error: cannot open %s\n."
"InnoDB: Have you deleted .ibd files under a running mysqld server?\n",
				node->name);
			src_exist = FALSE;
		}
	} else {
		src_file = node->handle;
	}

	/* open dst_file */
	dst_file = os_file_create(dst_path, OS_FILE_OVERWRITE,
					OS_FILE_AIO, OS_DATA_FILE, &success);
                if (!success) {
                        /* The following call prints an error message */
                        os_file_get_last_error(TRUE);

                        ut_print_timestamp(stderr);

                        fprintf(stderr,
"  InnoDB: error: cannot open %s\n.",
                                dst_path);
                        goto error;
                }

	if(!src_exist)
		goto error;

	/* copy : TODO: tune later */
	printf("Copying %s \n     to %s\n", node->name, dst_path);

	buf2 = ut_malloc((COPY_CHUNK + 1) * UNIV_PAGE_SIZE);
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	success = os_file_read(src_file, page, 0, 0, UNIV_PAGE_SIZE);
	if (!success) {
		goto error;
	}
	flush_lsn = mach_read_from_8(page + FIL_PAGE_FILE_FLUSH_LSN);
		/* check current flush lsn newer than checkpoint@start */
//	if (ut_dulint_cmp(backup_start_checkpoint, flush_lsn) >= 0) {
//		goto error;
//	}
	space_id = fsp_header_get_space_id(page);

	file_size = os_file_get_size_as_iblonglong(src_file);

	for (offset = 0; offset < file_size; offset += COPY_CHUNK * UNIV_PAGE_SIZE) {
		ulint chunk;
copy_loop:
		if (file_size - offset > COPY_CHUNK * UNIV_PAGE_SIZE) {
			chunk = COPY_CHUNK * UNIV_PAGE_SIZE;
		} else {
			chunk = (ulint)(file_size - offset);
		}

		xtrabackup_io_throttling();

		success = os_file_read(src_file, page,
				(ulint)(offset & 0xFFFFFFFFUL),
				(ulint)(offset >> 32), chunk);
		if (!success) {
			goto error;
		}

		success = os_file_write(dst_path, dst_file, page,
			(ulint)(offset & 0xFFFFFFFFUL),
			(ulint)(offset >> 32), chunk);
		if (!success) {
			goto error;
		}

	}

	success = os_file_flush(dst_file);
	if (!success) {
		goto error;
	}


	/* check size again */
	/* TODO: but is it needed ?? */
	if (file_size < os_file_get_size_as_iblonglong(src_file)) {
		offset -= COPY_CHUNK * UNIV_PAGE_SIZE;
		file_size = os_file_get_size_as_iblonglong(src_file);
		goto copy_loop;
	}

	/* TODO: How should we treat double_write_buffer here? */
	/* (currently, don't care about. Because,
	    the blocks is newer than the last checkpoint anyway.) */

	/* close */
	printf("        ...done\n");
	os_file_close(src_file);
	os_file_close(dst_file);
	ut_free(buf2);
	return(FALSE);
error:
	if (!src_file == -1)
		os_file_close(src_file);
	if (!dst_file == -1)
		os_file_close(dst_file);
	if (buf2)
		ut_free(buf2);
	fprintf(stderr, "Error: xtrabackup_copy_datafile() failed.\n");
	return(TRUE); /*ERROR*/
}

my_bool
xtrabackup_copy_logfile(dulint from_lsn, my_bool is_last)
{
	char	path[FN_REFLEN];
	char	*ptr1, *ptr2;

        fil_system_t*   system = fil_system;
        fil_space_t*    space;
	fil_space_t*	last_src;
        fil_node_t*     node;

	/* definition from recv_recovery_from_checkpoint_start() */
	log_group_t*	group;
	log_group_t*	max_cp_group;
	log_group_t*	up_to_date_group;
	ulint		max_cp_field;
	dulint		checkpoint_lsn;
	dulint		checkpoint_no;
	dulint		old_scanned_lsn;
	dulint		group_scanned_lsn;
	dulint		contiguous_lsn;
	dulint		archived_lsn;
	ulint		capacity;
	byte*		buf;
	byte		log_hdr_buf[LOG_FILE_HDR_SIZE];
	ulint		err;

	ibool		success;

	if (!xtrabackup_stream)
		ut_a(dst_log != -1);

	/* read from checkpoint_lsn_start to current */
	contiguous_lsn = ut_dulint_align_down(from_lsn,
						OS_FILE_LOG_BLOCK_SIZE);

	/* TODO: We must check the contiguous_lsn still exists in log file.. */

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		ibool	finished;
		dulint	start_lsn;
		dulint	end_lsn;


		old_scanned_lsn = from_lsn;

		/* reference recv_group_scan_log_recs() */
	finished = FALSE;

	start_lsn = contiguous_lsn;
		
	while (!finished) {			
		end_lsn = ut_dulint_add(start_lsn, RECV_SCAN_SIZE);

		xtrabackup_io_throttling();

		log_group_read_log_seg(LOG_RECOVER, log_sys->buf,
						group, start_lsn, end_lsn);

		//printf("log read from (%lu %lu) to (%lu %lu)\n",
		//	start_lsn.high, start_lsn.low, end_lsn.high, end_lsn.low);

		/* reference recv_scan_log_recs() */
		{
	byte*	log_block;
	ulint	no;
	dulint	scanned_lsn;
	ulint	data_len;
	ibool	more_data;

	ulint	scanned_checkpoint_no = 0;

	finished = FALSE;
	
	log_block = log_sys->buf;
	scanned_lsn = start_lsn;
	more_data = FALSE;

	while (log_block < log_sys->buf + RECV_SCAN_SIZE && !finished) {

		no = log_block_get_hdr_no(log_block);

		if (no != log_block_convert_lsn_to_no(scanned_lsn)
		    || !log_block_checksum_is_ok_or_old_format(log_block)) {

			if (no == log_block_convert_lsn_to_no(scanned_lsn)
			    && !log_block_checksum_is_ok_or_old_format(
								log_block)) {
				fprintf(stderr,
"InnoDB: Log block no %lu at lsn %lu %lu has\n"
"InnoDB: ok header, but checksum field contains %lu, should be %lu\n",
				(ulong) no,
				(ulong) ut_dulint_get_high(scanned_lsn),
				(ulong) ut_dulint_get_low(scanned_lsn),
				(ulong) log_block_get_checksum(log_block),
				(ulong) log_block_calc_checksum(log_block));
			}

			/* Garbage or an incompletely written log block */

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
		ulint write_size;

		if (!finished) {
			write_size = RECV_SCAN_SIZE;
		} else {
			write_size = ut_dulint_minus(
					ut_dulint_align_up(group_scanned_lsn, OS_FILE_LOG_BLOCK_SIZE),
					start_lsn);
		}

		//printf("Wrinting offset= %lld, size= %lu\n", log_copy_offset, write_size);

		if (!xtrabackup_stream) {
			success = os_file_write(dst_log_path, dst_log, log_sys->buf,
					(ulint)(log_copy_offset & 0xFFFFFFFFUL),
					(ulint)(log_copy_offset >> 32), write_size);
		} else {
			ulint ret;
			ulint stdout_write_size = write_size;
			if (finished && !is_last)
				stdout_write_size -= OS_FILE_LOG_BLOCK_SIZE;
			if (stdout_write_size) {
				ret = write(fileno(stdout), log_sys->buf, stdout_write_size);
				if (ret == stdout_write_size) {
					success = TRUE;
				} else {
					fprintf(stderr, "write: %lu > %lu\n", stdout_write_size, ret);
					success = FALSE;
				}
			} else {
				success = TRUE; /* do nothing */
			}
		}

		log_copy_offset += write_size;

		if (finished) {
			/* if continue, it will start from align_down(group_scanned_lsn) */
			log_copy_offset -= OS_FILE_LOG_BLOCK_SIZE;
		}

		if(!success) {
			if (!xtrabackup_stream) {
				fprintf(stderr, "Error: os_file_write to %s\n", dst_log_path);
			} else {
				fprintf(stderr, "Error: write to stdout\n");
			}
			goto error;
		}


		}





		start_lsn = end_lsn;
	}



		group->scanned_lsn = group_scanned_lsn;
		
		if (ut_dulint_cmp(old_scanned_lsn, group_scanned_lsn) < 0) {
			/* We found a more up-to-date group */

			up_to_date_group = group;
		}

		fprintf(stderr, ">> log scanned up to (%lu %lu)\n",group->scanned_lsn.high,group->scanned_lsn.low);

		group = UT_LIST_GET_NEXT(log_groups, group);

		/* update global variable*/
		log_copy_scanned_lsn = group_scanned_lsn;

		/* innodb_mirrored_log_groups must be 1, no other groups */
		ut_a(group == NULL);
	}


	if (!xtrabackup_stream) {
		success = os_file_flush(dst_log);
	} else {
		fflush(stdout);
		success = TRUE;
	}

	if(!success) {
		goto error;
	}

	return(FALSE);

error:
	if (!xtrabackup_stream)
		os_file_close(dst_log);
	fprintf(stderr, "Error: xtrabackup_copy_logfile() failed.\n");
	return(TRUE);
}

/* copying logfile in background */
#define SLEEPING_PERIOD 5

static
#ifndef __WIN__
void*
#else
ulint
#endif
log_copying_thread(
	void*	arg)
{
	ulint	counter = 0;

	if (!xtrabackup_stream)
		ut_a(dst_log != -1);

	log_copying_running = TRUE;

	while(log_copying) {
		usleep(200000); /*0.2 sec*/

		counter++;
		if(counter >= SLEEPING_PERIOD * 5) {
			if(xtrabackup_copy_logfile(log_copy_scanned_lsn, FALSE))
				goto end;
			counter = 0;
		}
	}

	/* last copying */
	if(xtrabackup_copy_logfile(log_copy_scanned_lsn, TRUE))
		goto end;

	log_copying_succeed = TRUE;
end:
	log_copying_running = FALSE;
	os_thread_exit(NULL);
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
	/* currently, for --backup only */
	ut_a(xtrabackup_backup);

	while (log_copying) {
		usleep(1000000); /*1 sec*/

		//for DEBUG
		//if (io_ticket == xtrabackup_throttle) {
		//	fprintf(stderr, "There seem to be no IO...?\n");
		//}

		io_ticket = xtrabackup_throttle;
		os_event_set(wait_throttle);
	}

	/* stop io throttle */
	xtrabackup_throttle = 0;
	os_event_set(wait_throttle);

	os_thread_exit(NULL);
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
	ulint	i;
	
	segment = *((ulint*)arg);

#ifdef UNIV_DEBUG_THREAD_CREATION
	fprintf(stderr, "Io handler thread %lu starts, id %lu\n", segment,
			os_thread_pf(os_thread_get_curr_id()));
#endif
	for (i = 0;; i++) {
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

/* CAUTION(?): Don't rename file_per_table during backup */
void
xtrabackup_backup_func(void)
{
	MY_STAT stat_info;

	/* cd to datadir */

	if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
	{
		fprintf(stderr, "cannot my_setwd %s\n", mysql_real_data_home);
		exit(1);
	}
	mysql_data_home= mysql_data_home_buff;
	mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
	mysql_data_home[1]=0;

	/* initialize components */
        if(innodb_init_param())
                exit(1);

        if (srv_pool_size >= 1000 * 1024) {
                                  /* Here we still have srv_pool_size counted
                                  in kilobytes (in 4.0 this was in bytes)
				  srv_boot() converts the value to
                                  pages; if buffer pool is less than 1000 MB,
                                  assume fewer threads. */
                srv_max_n_threads = 50000;

        } else if (srv_pool_size >= 8 * 1024) {

                srv_max_n_threads = 10000;
        } else {
		srv_max_n_threads = 1000;       /* saves several MB of memory,
                                                especially in 64-bit
                                                computers */
        }

	{
	ulint	n;
	ulint	i;

	n = srv_n_data_files;
	
	for (i = 0; i < n; i++) {
		srv_data_file_sizes[i] = srv_data_file_sizes[i]
					* ((1024 * 1024) / UNIV_PAGE_SIZE);
	}		

	srv_last_file_size_max = srv_last_file_size_max
					* ((1024 * 1024) / UNIV_PAGE_SIZE);
		
	srv_log_file_size = srv_log_file_size / UNIV_PAGE_SIZE;

	srv_log_buffer_size = srv_log_buffer_size / UNIV_PAGE_SIZE;

	srv_pool_size = srv_pool_size / (UNIV_PAGE_SIZE / 1024);

	srv_awe_window_size = srv_awe_window_size / UNIV_PAGE_SIZE;
	
	if (srv_use_awe) {
	        /* If we are using AWE we must save memory in the 32-bit
		address space of the process, and cannot bind the lock
		table size to the real buffer pool size. */

	        srv_lock_table_size = 20 * srv_awe_window_size;
	} else {
	        srv_lock_table_size = 5 * srv_pool_size;
	}
	}

	os_sync_mutex = NULL;
	srv_general_init();

	{
	buf_pool_t*	ret;
	ibool	create_new_db;
	ibool	log_file_created;
	ibool	log_created	= FALSE;
	ibool	log_opened	= FALSE;
	dulint	min_flushed_lsn;
	dulint	max_flushed_lsn;
	ulint   sum_of_new_sizes;
	ulint	sum_of_data_file_sizes;
	ulint	tablespace_size_in_header;
	ulint	err;
	ulint	i;
	ibool	srv_file_per_table_original_value  = srv_file_per_table;
	mtr_t   mtr;




#define SRV_N_PENDING_IOS_PER_THREAD 	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100
                os_aio_init(8 * SRV_N_PENDING_IOS_PER_THREAD
                            * srv_n_file_io_threads,
                            srv_n_file_io_threads,
                            SRV_MAX_N_PENDING_SYNC_IOS);

	fil_init(srv_max_n_open_files);

	fsp_init();
	log_init();

	lock_sys_create(srv_lock_table_size);

	for (i = 0; i < srv_n_file_io_threads; i++) {
		n[i] = i;

		os_thread_create(io_handler_thread, n + i, thread_ids + i);
    	}

	err = open_or_create_data_files(&create_new_db,
					&min_flushed_lsn, &max_flushed_lsn,
					&sum_of_new_sizes);
	if (err != DB_SUCCESS) {
	        fprintf(stderr,
"InnoDB: Could not open or create data files.\n"
"InnoDB: If you tried to add new data files, and it failed here,\n"
"InnoDB: you should now edit innodb_data_file_path in my.cnf back\n"
"InnoDB: to what it was, and remove the new ibdata files InnoDB created\n"
"InnoDB: in this failed attempt. InnoDB only wrote those files full of\n"
"InnoDB: zeros, but did not yet use them in any way. But be careful: do not\n"
"InnoDB: remove old data files which contain your precious data!\n");

		//return((int) err);
		exit(1);
	}

	/* create_new_db must not be TRUE.. */
	if (create_new_db) {
		fprintf(stderr, "InnoDB: Something wrong with source files...\n");
		exit(1);
	}

	for (i = 0; i < srv_n_log_files; i++) {
		err = open_or_create_log_file(create_new_db, &log_file_created,
							     log_opened, 0, i);
		if (err != DB_SUCCESS) {

			//return((int) err);
			exit(1);
		}

		if (log_file_created) {
			log_created = TRUE;
		} else {
			log_opened = TRUE;
		}
		if ((log_opened && create_new_db)
			    		|| (log_opened && log_created)) {
			fprintf(stderr, 
	"InnoDB: Error: all log files must be created at the same time.\n"
	"InnoDB: All log files must be created also in database creation.\n"
	"InnoDB: If you want bigger or smaller log files, shut down the\n"
	"InnoDB: database and make sure there were no errors in shutdown.\n"
	"InnoDB: Then delete the existing log files. Edit the .cnf file\n"
	"InnoDB: and start the database again.\n");

			//return(DB_ERROR);
			exit(1);
		}
	}

	/* log_file_created must not be TRUE, if online */
	if (log_file_created) {
		fprintf(stderr, "InnoDB: Something wrong with source files...\n");
		exit(1);
	}

	fil_load_single_table_tablespaces();

	}

	if (!xtrabackup_stream) {

	/* create target dir if not exist */
	if (!my_stat(xtrabackup_target_dir,&stat_info,MYF(0))
		&& (my_mkdir(xtrabackup_target_dir,0777,MYF(0)) < 0)){
		fprintf(stderr,"Error: cannot mkdir %d: %s\n",my_errno,xtrabackup_target_dir);
		exit(1);
	}

	} else {
		fprintf(stderr,"Stream mode.\n");
		/* stdout can treat binary at Linux */
		//setmode(fileno(stdout), O_BINARY);
	}

        {
	char	path[FN_REFLEN];
	char	*ptr1, *ptr2;

        fil_system_t*   system = fil_system;
        fil_space_t*    space;
	fil_space_t*	last_src;
        fil_node_t*     node;

	/* definition from recv_recovery_from_checkpoint_start() */
	log_group_t*	group;
	log_group_t*	max_cp_group;
	log_group_t*	up_to_date_group;
	ulint		max_cp_field;
	dulint		checkpoint_lsn;
	dulint		checkpoint_no;
	dulint		old_scanned_lsn;
	dulint		group_scanned_lsn;
	dulint		contiguous_lsn;
	dulint		archived_lsn;
	ulint		capacity;
	byte*		buf;
	byte		log_hdr_buf_[LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE];
	byte*		log_hdr_buf;
	ulint		err;

	ibool		success;

	log_hdr_buf = ut_align(log_hdr_buf_, OS_FILE_LOG_BLOCK_SIZE);

	/* log space */
	//space = UT_LIST_GET_NEXT(space_list, UT_LIST_GET_FIRST(system->space_list));
	//printf("space: name=%s, id=%d, purpose=%d, size=%d\n",
	//	space->name, space->id, space->purpose, space->size);

	/* get current checkpoint_lsn */
	/* Look for the latest checkpoint from any of the log groups */
	
	err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

	if (err != DB_SUCCESS) {

		exit(1);
	}
		
	log_group_read_checkpoint_info(max_cp_group, max_cp_field);
	buf = log_sys->checkpoint_buf;

	checkpoint_lsn_start = mach_read_from_8(buf + LOG_CHECKPOINT_LSN);
	checkpoint_no_start = mach_read_from_8(buf + LOG_CHECKPOINT_NO);

reread_log_header:
	fil_io(OS_FILE_READ | OS_FILE_LOG, TRUE, max_cp_group->space_id,
				0, 0, LOG_FILE_HDR_SIZE,
				log_hdr_buf, max_cp_group);

	/* check consistency of log file header to copy */
        err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

        if (err != DB_SUCCESS) {

                exit(1);
        }

        log_group_read_checkpoint_info(max_cp_group, max_cp_field);
        buf = log_sys->checkpoint_buf;

	if(ut_dulint_cmp(checkpoint_no_start,
			mach_read_from_8(buf + LOG_CHECKPOINT_NO)) != 0) {
		checkpoint_lsn_start = mach_read_from_8(buf + LOG_CHECKPOINT_LSN);
		checkpoint_no_start = mach_read_from_8(buf + LOG_CHECKPOINT_NO);
		goto reread_log_header;
	}

	if (!xtrabackup_stream) {

		/* open 'xtrabackup_logfile' */
		sprintf(dst_log_path, "%s%s", xtrabackup_target_dir, "/xtrabackup_logfile");
		dst_log = os_file_create(dst_log_path, OS_FILE_OVERWRITE,
						OS_FILE_AIO, OS_LOG_FILE, &success);

                if (!success) {
                        /* The following call prints an error message */
                        os_file_get_last_error(TRUE);

                        ut_print_timestamp(stderr);

                        fprintf(stderr,
"  InnoDB: error: cannot open %s\n.",
                                dst_log_path);
                        exit(1);
                }

	}

	/* label it */
	strcpy((char*) log_hdr_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
		"xtrabkup ");
	ut_sprintf_timestamp(
		(char*) log_hdr_buf + (LOG_FILE_WAS_CREATED_BY_HOT_BACKUP
				+ (sizeof "xtrabkup ") - 1));

	if (!xtrabackup_stream) {
		success = os_file_write(dst_log_path, dst_log, log_hdr_buf,
				0, 0, LOG_FILE_HDR_SIZE);
	} else {
		/* Stream */
		if (write(fileno(stdout), log_hdr_buf, LOG_FILE_HDR_SIZE)
				== LOG_FILE_HDR_SIZE) {
			success = TRUE;
		} else {
			success = FALSE;
		}
	}

	log_copy_offset += LOG_FILE_HDR_SIZE;
	if (!success) {
		if (!dst_log == -1)
			os_file_close(dst_log);
		exit(1);
	}

	/* start flag */
	log_copying = TRUE;

	/* start io throttle */
	if(xtrabackup_throttle) {
		os_thread_id_t io_watching_thread_id;

		io_ticket = xtrabackup_throttle;
		wait_throttle = os_event_create(NULL);

		os_thread_create(io_watching_thread, NULL, &io_watching_thread_id);
	}


	/* copy log file by current position */
	if(xtrabackup_copy_logfile(checkpoint_lsn_start, FALSE))
		exit(1);


	/* start back ground thread to copy newer log */
	os_thread_id_t log_copying_thread_id;

	os_thread_create(log_copying_thread, NULL, &log_copying_thread_id);



	if (!xtrabackup_stream) { /* stream mode is transaction log only */
        //mutex_enter(&(system->mutex)); /* NOTE: It may not needed at "--backup" for now */

        space = UT_LIST_GET_FIRST(system->space_list);
	last_src = UT_LIST_GET_LAST(system->space_list);

        while (space != NULL) {
		if (space->purpose == FIL_TABLESPACE) { /* datafile only */

                //printf("space: name=%s, id=%d, purpose=%d, size=%d\n",
                //        space->name, space->id, space->purpose, space->size);

		/* mkdir if not exist */
		ptr1 = strstr(space->name, "/");
		ptr2 = strstr(ptr1 + 1, "/");
		if(ptr2) {
			/* single table space */
			*ptr2 = 0; /* temporary (it's my lazy..)*/
			sprintf(path, "%s%s",xtrabackup_target_dir,ptr1);
			*ptr2 = '/';

			if (!my_stat(path,&stat_info,MYF(0))
				&& (my_mkdir(path,0777,MYF(0)) < 0)){

				fprintf(stderr,"Error: cannot mkdir %d: %s\n",my_errno,path);
				exit(1);
			}
		}

		node = UT_LIST_GET_FIRST(space->chain);
                while (node != NULL) {
                        //printf("  node: name=%s, open=%d, size=%d\n",
                        //       node->name, node->open, node->size);

			/* copy the datafile */
			if(xtrabackup_copy_datafile(node))
				printf("continuing anyway.\n");

                        node = UT_LIST_GET_NEXT(chain, node);
                }
		}

		if (space == last_src)
			break;

                space = UT_LIST_GET_NEXT(space_list, space);
        }

	} //if (!xtrabackup_stream)

        //mutex_exit(&(system->mutex));
        }


	/* suspend-at-end */
	if (xtrabackup_suspend_at_end) {
		os_file_t	suspend_file = -1;
		char	suspend_path[FN_REFLEN];
		ibool	success, exists;
		os_file_type_t	type;

		sprintf(suspend_path, "%s%s", xtrabackup_target_dir,
			"/xtrabackup_suspended");

		suspend_file = os_file_create(suspend_path, OS_FILE_OVERWRITE,
						OS_FILE_AIO, OS_DATA_FILE, &success);

		if (!success) {
			fprintf(stderr, "Error: failed to create file 'xtrabackup_suspended'\n");
		}

		if (!suspend_file == -1)
			os_file_close(suspend_file);

		exists = TRUE;
		while (exists) {
			usleep(200000); /*0.2 sec*/
			success = os_file_status(suspend_path, &exists, &type);
			if (!success)
				break;
		}
		xtrabackup_suspend_at_end = FALSE; /* suspend is 1 time */
	}


	/* stop log_copying_thread */
	log_copying = FALSE;
	if (!xtrabackup_stream) {
		printf("Stopping log copying thread");
		while (log_copying_running) {
			printf(".");
			usleep(200000); /*0.2 sec*/
		}
		printf("\n");
	} else {
		while (log_copying_running)
			usleep(200000); /*0.2 sec*/
	}

	if (!log_copying_succeed) {
		fprintf(stderr, "Error: log_copying_thread failed.\n");
		exit(1);
	}

	if (!xtrabackup_stream)
	        os_file_close(dst_log);

	if (wait_throttle)
		os_event_free(wait_throttle);

	if (!xtrabackup_stream) {
        	printf("Transaction log of lsn (%lu %lu) to (%lu %lu) was copied.\n",
                	checkpoint_lsn_start.high, checkpoint_lsn_start.low,
                	log_copy_scanned_lsn.high, log_copy_scanned_lsn.low);
	} else {
		fprintf(stderr, "Transaction log of lsn (%lu %lu) to (%lu %lu) was copied.\n",
			checkpoint_lsn_start.high, checkpoint_lsn_start.low,
			log_copy_scanned_lsn.high, log_copy_scanned_lsn.low);
	}
}

/* ================= prepare ================= */

my_bool
xtrabackup_init_temp_log(void)
{
	os_file_t	src_file = -1;
	os_file_t	dst_file = -1;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	ibool	success;

	ulint	field;
	byte*	log_buf;
	byte*	log_buf_ = NULL;

	ib_longlong	file_size;
	ib_longlong	src_offset;
	ib_longlong	dst_offset;

	dulint	max_no;
	dulint	max_lsn;
	ulint	max_field;
	dulint	checkpoint_no;


	max_no = ut_dulint_zero;

	sprintf(dst_path, "%s%s", xtrabackup_target_dir, "/ib_logfile0");

	/* open 'xtrabackup_logfile' */
	sprintf(src_path, "%s%s", xtrabackup_target_dir, "/xtrabackup_logfile");
retry:
	src_file = os_file_create_simple_no_error_handling(
					src_path, OS_FILE_OPEN,
					OS_FILE_READ_WRITE /* OS_FILE_READ_ONLY */, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: Warning: cannot open %s. will try to find.\n",
			src_path);

		/* check if ib_logfile0 may be xtrabackup_logfile */
		src_file = os_file_create_simple_no_error_handling(
				dst_path, OS_FILE_OPEN,
				OS_FILE_READ_WRITE /* OS_FILE_READ_ONLY */, &success);
		if (!success) {
			os_file_get_last_error(TRUE);
			fprintf(stderr,
"  InnoDB: Fatal error: cannot find %s.\n",
			src_path);

			goto error;
		}

		log_buf_ = ut_malloc(LOG_FILE_HDR_SIZE * 2);
		log_buf = ut_align(log_buf_, LOG_FILE_HDR_SIZE);

		success = os_file_read(src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
		if (!success) {
			goto error;
		}

		if ( ut_memcmp(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
				(byte*)"xtrabkup", (sizeof "xtrabkup") - 1) == 0) {
			fprintf(stderr,
"  InnoDB: 'ib_logfile0' seems to be 'xtrabackup_logfile'. will retry.\n");

			ut_free(log_buf_);
			log_buf_ = NULL;

			os_file_close(src_file);
			src_file = -1;

			/* rename and try again */
			success = os_file_rename(dst_path, src_path);
			if (!success) {
				goto error;
			}

			goto retry;
		}

		fprintf(stderr,
"  InnoDB: Fatal error: cannot find %s.\n",
		src_path);

		ut_free(log_buf_);
		log_buf_ = NULL;

		os_file_close(src_file);
		src_file = -1;

		goto error;
	}

	file_size = os_file_get_size_as_iblonglong(src_file);


	/* TODO: We should skip the following modifies, if it is not the first time. */
	log_buf_ = ut_malloc(UNIV_PAGE_SIZE * 2);
	log_buf = ut_align(log_buf_, UNIV_PAGE_SIZE);

	/* read log file header */
	success = os_file_read(src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	if ( ut_memcmp(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
			(byte*)"xtrabkup", (sizeof "xtrabkup") - 1) != 0 ) {
		printf("notice: xtrabackup_logfile was already used to '--prepare'.\n");
		goto skip_modify;
	} else {
		/* clear it later */
		//memset(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
		//		' ', 4);
	}

	/* read last checkpoint lsn */
	for (field = LOG_CHECKPOINT_1; field <= LOG_CHECKPOINT_2;
			field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {
		if (!recv_check_cp_is_consistent(log_buf + field))
			goto not_consistent;

		checkpoint_no = mach_read_from_8(log_buf + field + LOG_CHECKPOINT_NO);

		if (ut_dulint_cmp(checkpoint_no, max_no) >= 0) {
			max_no = checkpoint_no;
			max_lsn = mach_read_from_8(log_buf + field + LOG_CHECKPOINT_LSN);
			max_field = field;
/*
			mach_write_to_4(log_buf + field + LOG_CHECKPOINT_OFFSET,
					LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
					ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE)));

			ulint	fold;
			fold = ut_fold_binary(log_buf + field, LOG_CHECKPOINT_CHECKSUM_1);
			mach_write_to_4(log_buf + field + LOG_CHECKPOINT_CHECKSUM_1, fold);

			fold = ut_fold_binary(log_buf + field + LOG_CHECKPOINT_LSN,
				LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
			mach_write_to_4(log_buf + field + LOG_CHECKPOINT_CHECKSUM_2, fold);
*/
		}
not_consistent:
		;
	}

	if (ut_dulint_cmp(max_no, ut_dulint_zero) == 0) {
		fprintf(stderr, "InnoDB: No valid checkpoint found.\n");
		goto error;
	}


	/* It seems to be needed to overwrite the both checkpoint area. */
	ulint	fold;
	mach_write_to_8(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN, max_lsn);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_OFFSET,
			LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
			ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE)));
	fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_1, LOG_CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN,
		LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_CHECKSUM_2, fold);

	mach_write_to_8(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN, max_lsn);
        mach_write_to_4(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_OFFSET,
                        LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
                        ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE)));
        fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_2, LOG_CHECKPOINT_CHECKSUM_1);
        mach_write_to_4(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_CHECKSUM_1, fold);

        fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN,
                LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
        mach_write_to_4(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_CHECKSUM_2, fold);


	success = os_file_write(src_path, src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	/* align file size to UNIV_PAGE_SIZE */

	if (file_size % UNIV_PAGE_SIZE) {
		memset(log_buf, 0, UNIV_PAGE_SIZE);
		success = os_file_write(src_path, src_file, log_buf,
				(ulint)(file_size & 0xFFFFFFFFUL),
				(ulint)(file_size >> 32),
				UNIV_PAGE_SIZE - (file_size % UNIV_PAGE_SIZE));

		if (!success) {
			goto error;
		}

		file_size = os_file_get_size_as_iblonglong(src_file);
	}

	/* make larger than 2MB */
	if (file_size < 2*1024*1024L) {
		memset(log_buf, 0, UNIV_PAGE_SIZE);
		while (file_size < 2*1024*1024L) {
			success = os_file_write(src_path, src_file, log_buf,
				(ulint)(file_size & 0xFFFFFFFFUL),
				(ulint)(file_size >> 32),
				UNIV_PAGE_SIZE);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE;
		}
		file_size = os_file_get_size_as_iblonglong(src_file);
	}

	printf("xtrabackup_logfile detected: size=%lld, start_lsn=(%lu %lu)\n",
		file_size, max_lsn.high, max_lsn.low);

	os_file_close(src_file);
	src_file = -1;

	/* Backup log parameters */
	innobase_log_group_home_dir_backup = innobase_log_group_home_dir;
	innobase_log_file_size_backup      = innobase_log_file_size;
	innobase_log_files_in_group_backup = innobase_log_files_in_group;

	/* fake InnoDB */
	innobase_log_group_home_dir = NULL;
	innobase_log_file_size      = file_size;
	innobase_log_files_in_group = 1;

	srv_thread_concurrency = 0;

	/* rename 'xtrabackup_logfile' to 'ib_logfile0' */
	success = os_file_rename(src_path, dst_path);
	if (!success) {
		goto error;
	}
	xtrabackup_logfile_is_renamed = TRUE;

	ut_free(log_buf_);

	return(FALSE);

skip_modify:
	os_file_close(src_file);
	src_file = -1;
	ut_free(log_buf_);
	return(FALSE);

error:
	if (src_file != -1)
		os_file_close(src_file);
	if (log_buf_)
		ut_free(log_buf_);
	fprintf(stderr, "Error: xtrabackup_init_temp_log() failed.\n");
	return(TRUE); /*ERROR*/
}

my_bool
xtrabackup_close_temp_log(my_bool clear_flag)
{
	os_file_t	src_file = -1;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	ibool	success;

	byte*	log_buf;
	byte*	log_buf_ = NULL;


	if (!xtrabackup_logfile_is_renamed)
		return(FALSE);

	/* Restore log parameters */
	innobase_log_group_home_dir = innobase_log_group_home_dir_backup;
	innobase_log_file_size      = innobase_log_file_size_backup;
	innobase_log_files_in_group = innobase_log_files_in_group_backup;

	/* rename 'ib_logfile0' to 'xtrabackup_logfile' */
	sprintf(dst_path, "%s%s", xtrabackup_target_dir, "/ib_logfile0");
	sprintf(src_path, "%s%s", xtrabackup_target_dir, "/xtrabackup_logfile");

	success = os_file_rename(dst_path, src_path);
	if (!success) {
		goto error;
	}
	xtrabackup_logfile_is_renamed = FALSE;

	if (!clear_flag)
		return(FALSE);

	/* clear LOG_FILE_WAS_CREATED_BY_HOT_BACKUP field */
	src_file = os_file_create_simple_no_error_handling(
				src_path, OS_FILE_OPEN,
				OS_FILE_READ_WRITE, &success);
	if (!success) {
		goto error;
	}

	log_buf_ = ut_malloc(LOG_FILE_HDR_SIZE * 2);
	log_buf = ut_align(log_buf_, LOG_FILE_HDR_SIZE);

	success = os_file_read(src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	memset(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP, ' ', 4);

	success = os_file_write(src_path, src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	os_file_close(src_file);
	src_file = -1;

	return(FALSE);
error:
	if (src_file != -1)
		os_file_close(src_file);
	if (log_buf_)
		ut_free(log_buf_);
	fprintf(stderr, "Error: xtrabackup_close_temp_log() failed.\n");
	return(TRUE); /*ERROR*/
}

void
xtrabackup_prepare_func(void)
{
	/* cd to target-dir */

	if (my_setwd(xtrabackup_real_target_dir,MYF(MY_WME)))
	{
		fprintf(stderr, "cannot my_setwd %s\n", xtrabackup_real_target_dir);
		exit(1);
	}
	xtrabackup_target_dir= mysql_data_home_buff;
	xtrabackup_target_dir[0]=FN_CURLIB;		// all paths are relative from here
	xtrabackup_target_dir[1]=0;


	/* Create logfiles for recovery from 'xtrabackup_logfile', before start InnoDB */
	srv_max_n_threads = 1000;
	os_sync_mutex = NULL;
	os_sync_init();
	sync_init();
	os_io_init_simple();
	if(xtrabackup_init_temp_log())
		goto error;
	sync_close();
	sync_initialized = FALSE;
	os_sync_free();
	os_sync_mutex = NULL;

	/* check the accessibility of target-dir */
	/* ############# TODO ##################### */

	if(innodb_init_param())
		goto error;

	if(innodb_init())
		goto error;

	//printf("Hello InnoDB world!\n");

	/* TEST: innodb status*/
/*
	ulint	trx_list_start = ULINT_UNDEFINED;
	ulint	trx_list_end = ULINT_UNDEFINED;
	srv_printf_innodb_monitor(stdout, &trx_list_start, &trx_list_end);
*/
	/* TEST: list of datafiles and transaction log files and LSN*/
/*
	{
	fil_system_t*   system = fil_system;
	fil_space_t*	space;
	fil_node_t*	node;

        mutex_enter(&(system->mutex));

        space = UT_LIST_GET_FIRST(system->space_list);

        while (space != NULL) {
		printf("space: name=%s, id=%d, purpose=%d, size=%d\n",
			space->name, space->id, space->purpose, space->size);

                node = UT_LIST_GET_FIRST(space->chain);

                while (node != NULL) {
			printf("node: name=%s, open=%d, size=%d\n",
				node->name, node->open, node->size);

                        node = UT_LIST_GET_NEXT(chain, node);
                }
                space = UT_LIST_GET_NEXT(space_list, space);
        }

        mutex_exit(&(system->mutex));
	}
*/

	/* print binlog position (again?) */
	printf("\n[notice (again)]\n"
		"  If you use binary log and don't use any hack of group commit,\n"
		"  the binary log position seems to be:\n");
	trx_sys_print_mysql_binlog_offset();
	printf("\n");

	if(innodb_end())
		goto error;

	sync_initialized = FALSE;
	os_sync_mutex = NULL;

	if(xtrabackup_close_temp_log(TRUE))
		exit(1);

	if(!xtrabackup_create_ib_logfile)
		return;

	/* TODO: make more smart */

	printf("\n[notice]\nWe cannot call InnoDB second time during the process lifetime.\n");
	printf("Please re-execte to create ib_logfile*. Sorry.\n");
/*
	printf("Restart InnoDB to create ib_logfile*.\n");

	if(innodb_init_param())
		goto error;

	if(innodb_init())
		goto error;

	if(innodb_end())
		goto error;
*/

	return;

error:
	xtrabackup_close_temp_log(FALSE);

	exit(1);
}

/* ================= main =================== */

int main(int argc, char **argv)
{
	int ho_error;

	MY_INIT(argv[0]);

	load_defaults("my",load_default_groups,&argc,&argv);

	/* ignore unsupported options */
	{
	int i,j,argc_new,find;
	char *optend, *prev_found;
	argc_new = argc;

	j=1;
	for (i=1 ; i < argc ; i++) {
		optend= strcend((argv)[i], '=');
		uint count;
		struct my_option *opt= (struct my_option *) my_long_options;
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
	}

	if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
		exit(ho_error);

	if (strcmp(mysql_data_home, "./") == 0) {
		if (!xtrabackup_print_param)
			usage();
		printf("\nError: Please set parameter 'datadir'\n");
		exit(-1);
	}

	/* --print-param */
	if (xtrabackup_print_param) {
		printf("# This MySQL options file was generated by XtraBackup.\n");
		printf("[mysqld]\n");
		printf("datadir = %s\n", mysql_data_home);
		printf("innodb_data_home_dir = %s\n",
			innobase_data_home_dir ? innobase_data_home_dir : mysql_data_home);
		printf("innodb_data_file_path = %s\n",
			innobase_data_file_path ? innobase_data_file_path : "ibdata1:10M:autoextend");
		printf("innodb_log_group_home_dir = %s\n",
			innobase_log_group_home_dir ? innobase_log_group_home_dir : mysql_data_home);
		printf("innodb_log_files_in_group = %ld\n", innobase_log_files_in_group);
		printf("innodb_log_file_size = %lld\n", innobase_log_file_size);
		exit(0);
	}

	if (!xtrabackup_stream) {
		print_version();
	} else {
		if (xtrabackup_backup) {
			xtrabackup_suspend_at_end = TRUE;
			fprintf(stderr, "suspend-at-end is enabled.\n");
		}
	}

	/* cannot execute both for now */
	if (xtrabackup_backup == xtrabackup_prepare) { /* !XOR (for now) */
		usage();
		exit(-1);
	}

	/* --backup */
	if (xtrabackup_backup)
		xtrabackup_backup_func();

	/* --prepare */
	if (xtrabackup_prepare)
		xtrabackup_prepare_func();

	exit(0);
}
