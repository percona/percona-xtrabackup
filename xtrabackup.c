/******************************************************
XtraBackup: The another hot backup tool for InnoDB
(c) 2009 Percona Inc.
Created 3/3/2009 Yasufumi Kinoshita
*******************************************************/

#ifndef XTRABACKUP_VERSION
#define XTRABACKUP_VERSION "undefined"
#endif
#ifndef XTRABACKUP_REVISION
#define XTRABACKUP_REVISION "undefined"
#endif

//#define XTRABACKUP_TARGET_IS_PLUGIN

#include <my_base.h>
#include <my_getopt.h>
#include <mysql_version.h>
#include <mysql_com.h>

#if (MYSQL_VERSION_ID < 50100)
#define G_PTR gptr
#else /* MYSQL_VERSION_ID < 51000 */
#define G_PTR uchar*
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

#ifdef __WIN__
#define SRV_PATH_SEPARATOR	'\\'
#define SRV_PATH_SEPARATOR_STR	"\\"	
#else
#define SRV_PATH_SEPARATOR	'/'
#define SRV_PATH_SEPARATOR_STR	"/"
#endif

/* prototypes for static functions in original */
page_t*
btr_node_ptr_get_child(
/*===================*/
				/* out: child page, x-latched */
	rec_t*		node_ptr,/* in: node pointer */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	mtr_t*		mtr);	/* in: mtr */

int
fil_file_readdir_next_file(
/*=======================*/
				/* out: 0 if ok, -1 if error even after the
				retries, 1 if at the end of the directory */
	ulint*		err,	/* out: this is set to DB_ERROR if an error
				was encountered, otherwise not changed */
	const char*	dirname,/* in: directory name or path */
	os_file_dir_t	dir,	/* in: directory stream */
	os_file_stat_t*	info);	/* in/out: buffer where the info is returned */

ibool
recv_check_cp_is_consistent(
/*========================*/
			/* out: TRUE if ok */
	byte*	buf);	/* in: buffer containing checkpoint info */

ulint
recv_find_max_checkpoint(
/*=====================*/
					/* out: error code or DB_SUCCESS */
	log_group_t**	max_group,	/* out: max group */
	ulint*		max_field);	/* out: LOG_CHECKPOINT_1 or
					LOG_CHECKPOINT_2 */

ibool
log_block_checksum_is_ok_or_old_format(
/*===================================*/
			/* out: TRUE if ok, or if the log block may be in the
			format of InnoDB version < 3.23.52 */
	byte*	block);	/* in: pointer to a log block */

ulint
open_or_create_log_file(
/*====================*/
					/* out: DB_SUCCESS or error code */
        ibool   create_new_db,          /* in: TRUE if we should create a
                                        new database */
	ibool*	log_file_created,	/* out: TRUE if new log file
					created */
	ibool	log_file_has_been_opened,/* in: TRUE if a log file has been
					opened before: then it is an error
					to try to create another log file */
	ulint	k,			/* in: log group number */
	ulint	i);			/* in: log file number in group */

ulint
open_or_create_data_files(
/*======================*/
				/* out: DB_SUCCESS or error code */
	ibool*	create_new_db,	/* out: TRUE if new database should be
								created */
#ifdef UNIV_LOG_ARCHIVE
	ulint*	min_arch_log_no,/* out: min of archived log numbers in data
				files */
	ulint*	max_arch_log_no,/* out: */
#endif /* UNIV_LOG_ARCHIVE */
	dulint*	min_flushed_lsn,/* out: min of flushed lsn values in data
				files */
	dulint*	max_flushed_lsn,/* out: */
	ulint*	sum_of_new_sizes);/* out: sum of sizes of the new files added */

void
os_file_set_nocache(
/*================*/
	int		fd,		/* in: file descriptor to alter */
	const char*	file_name,	/* in: used in the diagnostic message */
	const char*	operation_name);	/* in: used in the diagnostic message,
					we call os_file_set_nocache()
					immediately after opening or creating
					a file, so this is either "open" or
					"create" */

#include <fcntl.h>
#include <regex.h>

#ifdef POSIX_FADV_NORMAL
#define USE_POSIX_FADVISE
#endif

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
my_bool xtrabackup_stats = FALSE;
my_bool xtrabackup_prepare = FALSE;
my_bool xtrabackup_print_param = FALSE;

my_bool xtrabackup_export = FALSE;

my_bool xtrabackup_suspend_at_end = FALSE;
longlong xtrabackup_use_memory = 100*1024*1024L;
my_bool xtrabackup_create_ib_logfile = FALSE;

long xtrabackup_throttle = 0; /* 0:unlimited */
lint io_ticket;
os_event_t wait_throttle = NULL;

my_bool xtrabackup_stream = FALSE;
char *xtrabackup_incremental = NULL;
dulint incremental_lsn;
dulint incremental_to_lsn;
byte* incremental_buffer = NULL;
byte* incremental_buffer_base = NULL;

char *xtrabackup_incremental_basedir = NULL; /* for --backup */
char *xtrabackup_incremental_dir = NULL; /* for --prepare */

char *xtrabackup_tables = NULL;
regex_t tables_regex;
regmatch_t tables_regmatch[1];

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

/* === metadata of backup === */
#define XTRABACKUP_METADATA_FILENAME "xtrabackup_checkpoints"
char metadata_type[30] = ""; /*[full-backuped|full-prepared|incremental]*/
dulint metadata_from_lsn = {0, 0};
dulint metadata_to_lsn = {0, 0};

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

ulong	innobase_fast_shutdown			= 1;
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
  OPT_XTRA_STATS,
  OPT_XTRA_PREPARE,
  OPT_XTRA_EXPORT,
  OPT_XTRA_PRINT_PARAM,
  OPT_XTRA_SUSPEND_AT_END,
  OPT_XTRA_USE_MEMORY,
  OPT_XTRA_THROTTLE,
  OPT_XTRA_STREAM,
  OPT_XTRA_INCREMENTAL,
  OPT_XTRA_INCREMENTAL_BASEDIR,
  OPT_XTRA_INCREMENTAL_DIR,
  OPT_XTRA_TABLES,
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
  {"print-param", OPT_XTRA_PRINT_PARAM, "print parameter of mysqld needed for copyback.",
   (G_PTR*) &xtrabackup_print_param, (G_PTR*) &xtrabackup_print_param,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"use-memory", OPT_XTRA_USE_MEMORY, "The value is used instead of buffer_pool_size",
   (G_PTR*) &xtrabackup_use_memory, (G_PTR*) &xtrabackup_use_memory,
   0, GET_LL, REQUIRED_ARG, 100*1024*1024L, 1024*1024L, LONGLONG_MAX, 0,
   1024*1024L, 0},
  {"suspend-at-end", OPT_XTRA_SUSPEND_AT_END, "creates a file 'xtrabackup_suspended' and waits until the user deletes that file at the end of '--backup'",
   (G_PTR*) &xtrabackup_suspend_at_end, (G_PTR*) &xtrabackup_suspend_at_end,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"throttle", OPT_XTRA_THROTTLE, "limit count of IO operations (pairs of read&write) per second to IOS values (for '--backup')",
   (G_PTR*) &xtrabackup_throttle, (G_PTR*) &xtrabackup_throttle,
   0, GET_LONG, REQUIRED_ARG, 0, 0, LONG_MAX, 0, 1, 0},
  {"log-stream", OPT_XTRA_STREAM, "outputs the contents of 'xtrabackup_logfile' to stdout only until the file 'xtrabackup_suspended' deleted (for '--backup').",
   (G_PTR*) &xtrabackup_stream, (G_PTR*) &xtrabackup_stream,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"incremental-lsn", OPT_XTRA_INCREMENTAL, "(for --backup): copy only .ibd pages newer than specified LSN 'high:low'. ##ATTENTION##: checkpoint lsn must be used. anyone can detect your mistake. be carefully!",
   (G_PTR*) &xtrabackup_incremental, (G_PTR*) &xtrabackup_incremental,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"incremental-basedir", OPT_XTRA_INCREMENTAL_BASEDIR, "(for --backup): copy only .ibd pages newer than backup at specified directory.",
   (G_PTR*) &xtrabackup_incremental_basedir, (G_PTR*) &xtrabackup_incremental_basedir,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"incremental-dir", OPT_XTRA_INCREMENTAL_DIR, "(for --prepare): apply .delta files and logfile in the specified directory.",
   (G_PTR*) &xtrabackup_incremental_dir, (G_PTR*) &xtrabackup_incremental_dir,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tables", OPT_XTRA_TABLES, "filtering by regexp for table names.",
   (G_PTR*) &xtrabackup_tables, (G_PTR*) &xtrabackup_tables,
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
  {"innodb_file_per_table", OPT_INNODB_FILE_PER_TABLE,
   "Stores each InnoDB table to an .ibd file in the database dir.",
   (G_PTR*) &innobase_file_per_table,
   (G_PTR*) &innobase_file_per_table, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
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
/*
  {"innodb_log_arch_dir", OPT_INNODB_LOG_ARCH_DIR,
   "Where full logs should be archived.", (G_PTR*) &innobase_log_arch_dir,
   (G_PTR*) &innobase_log_arch_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
*/
  {"innodb_log_buffer_size", OPT_INNODB_LOG_BUFFER_SIZE,
   "The size of the buffer which InnoDB uses to write log to the log files on disk.",
   (G_PTR*) &innobase_log_buffer_size, (G_PTR*) &innobase_log_buffer_size, 0,
   GET_LONG, REQUIRED_ARG, 1024*1024L, 256*1024L, LONG_MAX, 0, 1024, 0},
  {"innodb_log_file_size", OPT_INNODB_LOG_FILE_SIZE,
   "Size of each log file in a log group.",
   (G_PTR*) &innobase_log_file_size, (G_PTR*) &innobase_log_file_size, 0,
   GET_LL, REQUIRED_ARG, 5*1024*1024L, 1*1024*1024L, LONGLONG_MAX, 0,
   1024*1024L, 0},
  {"innodb_log_files_in_group", OPT_INNODB_LOG_FILES_IN_GROUP,
   "Number of log files in the log group. InnoDB writes to the files in a circular fashion. Value 3 is recommended here.",
   (G_PTR*) &innobase_log_files_in_group, (G_PTR*) &innobase_log_files_in_group,
   0, GET_LONG, REQUIRED_ARG, 2, 2, 100, 0, 1, 0},
  {"innodb_log_group_home_dir", OPT_INNODB_LOG_GROUP_HOME_DIR,
   "Path to InnoDB log files.", (G_PTR*) &innobase_log_group_home_dir,
   (G_PTR*) &innobase_log_group_home_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
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
/*
  {"innodb_rollback_on_timeout", OPT_INNODB_ROLLBACK_ON_TIMEOUT,
   "Roll back the complete transaction on lock wait timeout, for 4.x compatibility (disabled by default)",
   (G_PTR*) &innobase_rollback_on_timeout, (G_PTR*) &innobase_rollback_on_timeout,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
*/
/*
  {"innodb_status_file", OPT_INNODB_STATUS_FILE,
   "Enable SHOW INNODB STATUS output in the innodb_status.<pid> file",
   (G_PTR*) &innobase_create_status_file, (G_PTR*) &innobase_create_status_file,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
*/
/*
  {"innodb_sync_spin_loops", OPT_INNODB_SYNC_SPIN_LOOPS,
   "Count of spin-loop rounds in InnoDB mutexes",
   (G_PTR*) &srv_n_spin_wait_rounds,
   (G_PTR*) &srv_n_spin_wait_rounds,
   0, GET_ULONG, REQUIRED_ARG, 20L, 0L, ULONG_MAX, 0, 1L, 0},
*/
/*
  {"innodb_thread_concurrency", OPT_INNODB_THREAD_CONCURRENCY,
   "Helps in performance tuning in heavily concurrent environments. "
   "Sets the maximum number of threads allowed inside InnoDB. Value 0"
   " will disable the thread throttling.",
   (G_PTR*) &srv_thread_concurrency, (G_PTR*) &srv_thread_concurrency,
   0, GET_ULONG, REQUIRED_ARG, 8, 0, 1000, 0, 1, 0},
*/
/*
  {"innodb_thread_sleep_delay", OPT_INNODB_THREAD_SLEEP_DELAY,
   "Time of innodb thread sleeping before joining InnoDB queue (usec). Value 0"
    " disable a sleep",
   (G_PTR*) &srv_thread_sleep_delay,
   (G_PTR*) &srv_thread_sleep_delay,
   0, GET_ULONG, REQUIRED_ARG, 10000L, 0L, ULONG_MAX, 0, 1L, 0},
*/

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static const char *load_default_groups[]= { "mysqld","xtrabackup",0 };

static void print_version(void)
{
  printf("%s  Ver %s Rev %s for %s %s (%s)\n" ,my_progname,
	  XTRABACKUP_VERSION, XTRABACKUP_REVISION, MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage(void)
{
  puts("Open source backup tool for InnoDB and XtraDB\n\
\n\
Copyright (C) 2009 Percona Inc.\n\
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
	fprintf(stderr, "xtrabackup: thd_is_replication_slave_thread() is called\n");
	return(FALSE);
}

ibool
thd_has_edited_nontrans_tables(
	void*	thd)
{
	fprintf(stderr, "xtrabackup: thd_has_edited_nontrans_tables() is called\n");
	return(FALSE);
}

ibool
thd_is_select(
	const void*	thd)
{
	fprintf(stderr, "xtrabackup: thd_is_select() is called\n");
	return(FALSE);
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
	fprintf(stderr, "xtrabackup: innobase_mysql_print_thd() is called\n");
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
	fprintf(stderr, "xtrabackup: innobase_convert_from_table_id() is called\n");
}

void
innobase_convert_from_id(
	char*	to,
	const char*	from,
	ulint	len)
{
	fprintf(stderr, "xtrabackup: innobase_convert_from_id() is called\n");
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
	fprintf(stderr, "xtrabackup: innobase_get_charset() is called\n");
	return(NULL);
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
			fprintf(stderr, "xtrabackup: Got error %d on dup\n",fd2);
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
        const char*     e = s + namelen;
        int             q;

        q = '"';

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
			  fprintf(stderr, "xtrabackup: InnoDB needs charset %lu for doing "
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
	fprintf(stderr, "xtrabackup: innobase_query_is_update() is called\n");
	return(0);
}

/* control innodb */

my_bool
innodb_init_param(void)
{
	/* === some variables from mysqld === */
	bzero((G_PTR) &mysql_tmpdir_list, sizeof(mysql_tmpdir_list));

	if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
		exit(1);

	/* dummy for initialize all_charsets[] */
	get_charset_name(0);


	/* innobase_init */

	static char	current_dir[3];		/* Set if using current lib */
	my_bool		ret;
	char 	        *default_path;

	/* Check that values don't overflow on 32-bit systems. */
	if (sizeof(ulint) == 4) {
		if (xtrabackup_use_memory > UINT_MAX32) {
			fprintf(stderr,
				"xtrabackup: use-memory can't be over 4GB"
				" on 32-bit systems\n");
		}

		if (innobase_buffer_pool_size > UINT_MAX32) {
			fprintf(stderr,
				"xtrabackup: innobase_buffer_pool_size can't be over 4GB"
				" on 32-bit systems\n");

			goto error;
		}

		if (innobase_log_file_size > UINT_MAX32) {
			fprintf(stderr,
				"xtrabackup: innobase_log_file_size can't be over 4GB"
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

	if (xtrabackup_backup || xtrabackup_stats) {
		fprintf(stderr, "xtrabackup: Target instance is assumed as followings.\n");
	} else {
		fprintf(stderr, "xtrabackup: Temporary instance for recovery is set as followings.\n");
	}

	/*--------------- Data files -------------------------*/

	/* The default dir for data files is the datadir of MySQL */

	srv_data_home = ((xtrabackup_backup || xtrabackup_stats) && innobase_data_home_dir
			 ? innobase_data_home_dir : default_path);
	fprintf(stderr, "xtrabackup:   innodb_data_home_dir = %s\n", srv_data_home);

	/* Set default InnoDB data file size to 10 MB and let it be
  	auto-extending. Thus users can use InnoDB in >= 4.0 without having
	to specify any startup options. */

	if (!innobase_data_file_path) {
  		innobase_data_file_path = (char*) "ibdata1:10M:autoextend";
	}
	fprintf(stderr, "xtrabackup:   innodb_data_file_path = %s\n",
		innobase_data_file_path);

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
			"xtrabackup: syntax error in innodb_data_file_path\n");
	  	free(internal_innobase_data_file_path);
                goto error;
	}

	if (xtrabackup_prepare) {
		/* "--prepare" needs filenames only */
		ulint i;

		for (i=0; i < srv_n_data_files; i++) {
			char *p;

			p = srv_data_file_names[i];
			while (p = strstr(p, SRV_PATH_SEPARATOR_STR))
			{
				p++;
				srv_data_file_names[i] = p;
			}
		}
	}

	/* -------------- Log files ---------------------------*/

	/* The default dir for log files is the datadir of MySQL */

	if (!((xtrabackup_backup || xtrabackup_stats) && innobase_log_group_home_dir)) {
	  	innobase_log_group_home_dir = default_path;
	}
	if (xtrabackup_prepare && xtrabackup_incremental_dir) {
		innobase_log_group_home_dir = xtrabackup_incremental_dir;
	}
	fprintf(stderr, "xtrabackup:   innodb_log_group_home_dir = %s\n",
		innobase_log_group_home_dir);

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
	  fprintf(stderr, "xtrabackup: syntax error in innodb_log_group_home_dir, or a "
			  "wrong number of mirrored log groups\n");

	  	free(internal_innobase_data_file_path);
                goto error;
	}

	/* --------------------------------------------------*/

	srv_file_flush_method_str = innobase_unix_file_flush_method;

	srv_n_log_groups = (ulint) innobase_mirrored_log_groups;
	srv_n_log_files = (ulint) innobase_log_files_in_group;
	srv_log_file_size = (ulint) innobase_log_file_size;
	fprintf(stderr, "xtrabackup:   innodb_log_files_in_group = %ld\n",
		srv_n_log_files);
	fprintf(stderr, "xtrabackup:   innodb_log_file_size = %ld\n",
		srv_log_file_size);

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
	fprintf(stderr, "xtrabackup: innodb_init_param(): Error occured.\n");
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
	fprintf(stderr, "xtrabackup: innodb_init(): Error occured.\n");
	return(TRUE);
}

my_bool
innodb_end(void)
{
	srv_fast_shutdown = (ulint) innobase_fast_shutdown;
	innodb_inited = 0;

	fprintf(stderr, "xtrabackup: starting shutdown with innodb_fast_shutdown = %lu\n",
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
	fprintf(stderr, "xtrabackup: innodb_end(): Error occured.\n");
	return(TRUE);
}

/* ================= common ================= */
my_bool
xtrabackup_read_metadata(char *filename)
{
	FILE *fp;

	fp = fopen(filename,"r");
	if(!fp) {
		fprintf(stderr, "xtrabackup: Error: cannot open %s\n", filename);
		return(TRUE);
	}

	if (fscanf(fp, "backup_type = %29s\n", metadata_type)
			!= 1)
		return(TRUE);
	if (fscanf(fp, "from_lsn = %lu:%lu\n", &metadata_from_lsn.high, &metadata_from_lsn.low)
			!= 2)
		return(TRUE);
	if (fscanf(fp, "to_lsn = %lu:%lu\n", &metadata_to_lsn.high, &metadata_to_lsn.low)
			!= 2)
		return(TRUE);

	fclose(fp);

	return(FALSE);
}

my_bool
xtrabackup_write_metadata(char *filename)
{
	FILE *fp;

	fp = fopen(filename,"w");
	if(!fp) {
		fprintf(stderr, "xtrabackup: Error: cannot open %s\n", filename);
		return(TRUE);
	}

	if (fprintf(fp, "backup_type = %s\n", metadata_type)
			< 0)
		return(TRUE);
	if (fprintf(fp, "from_lsn = %lu:%lu\n", metadata_from_lsn.high, metadata_from_lsn.low)
			< 0)
		return(TRUE);
	if (fprintf(fp, "to_lsn = %lu:%lu\n", metadata_to_lsn.high, metadata_to_lsn.low)
			< 0)
		return(TRUE);

	fclose(fp);

	return(FALSE);
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
	ib_longlong	file_size;
	ib_longlong	offset;
	ibool		src_exist = TRUE;
	ulint		page_in_buffer;
	ulint		incremental_buffers = 0;

	if (xtrabackup_tables && (node->space->id != 0)) { /* must backup id==0 */
		char *p;
		int p_len, regres;
		char *next, *prev;
		char tmp;

		p = node->name;
		prev = NULL;
		while (next = strstr(p, SRV_PATH_SEPARATOR_STR))
		{
			prev = p;
			p = next + 1;
		}
		p_len = strlen(p) - strlen(".ibd");

		if (p_len < 1) {
			/* unknown situation: skip filtering */
			goto skip_filter;
		}

		/* TODO: Fix this lazy implementation... */
		tmp = p[p_len];
		p[p_len] = 0;
		*(p - 1) = '.';

		regres = regexec(&tables_regex, prev, 1, tables_regmatch, 0);

		p[p_len] = tmp;
		*(p - 1) = SRV_PATH_SEPARATOR;

		if ( regres == REG_NOMATCH ) {
			printf("Copying %s is skipped.\n", node->name);
			return(FALSE);
		}
	}
skip_filter:

	if (node->space->id == 0) {
		char *next, *p;
		/* system datafile "/fullpath/datafilename.ibd" or "./datafilename.ibd" */
		p = node->name;
		while (next = strstr(p, SRV_PATH_SEPARATOR_STR))
		{
			p = next + 1;
		}
		sprintf(dst_path, "%s/%s", xtrabackup_target_dir, p);
	} else {
		/* file per table style "./database/table.ibd" */
		sprintf(dst_path, "%s%s", xtrabackup_target_dir, strstr(node->name, SRV_PATH_SEPARATOR_STR));
	}

	if (xtrabackup_incremental) {
		strcat(dst_path, ".delta");

		/* clear buffer */
		bzero(incremental_buffer, (UNIV_PAGE_SIZE/4) * UNIV_PAGE_SIZE);
		page_in_buffer = 0;
		mach_write_to_4(incremental_buffer, 0x78747261UL);/*"xtra"*/
		page_in_buffer++;
	}

	/* open src_file*/
	if (!node->open) {
		src_file = os_file_create_simple_no_error_handling(
						node->name, OS_FILE_OPEN,
						OS_FILE_READ_ONLY, &success);
		if (!success) {
			/* The following call prints an error message */
			os_file_get_last_error(TRUE);

			fprintf(stderr,
"xtrabackup: error: cannot open %s\n"
"xtrabackup: Have you deleted .ibd files under a running mysqld server?\n",
				node->name);
			src_exist = FALSE;
		}

		if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
			os_file_set_nocache(src_file, node->name, "OPEN");
		}
	} else {
		src_file = node->handle;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(src_file, 0, 0, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(src_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	/* open dst_file */
	/* os_file_create reads srv_unix_file_flush_method */
	dst_file = os_file_create(dst_path, OS_FILE_CREATE,
					OS_FILE_AIO, OS_DATA_FILE, &success);
                if (!success) {
                        /* The following call prints an error message */
                        os_file_get_last_error(TRUE);

                        fprintf(stderr,
"xtrabackup: error: cannot open %s\n",
                                dst_path);
                        goto error;
                }

#ifdef USE_POSIX_FADVISE
	posix_fadvise(dst_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

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

	file_size = os_file_get_size_as_iblonglong(src_file);

	for (offset = 0; offset < file_size; offset += COPY_CHUNK * UNIV_PAGE_SIZE) {
		ulint chunk;
		ulint chunk_offset;
		ulint retry_count = 10;
copy_loop:
		if (file_size - offset > COPY_CHUNK * UNIV_PAGE_SIZE) {
			chunk = COPY_CHUNK * UNIV_PAGE_SIZE;
		} else {
			chunk = (ulint)(file_size - offset);
		}

read_retry:
		xtrabackup_io_throttling();

		success = os_file_read(src_file, page,
				(ulint)(offset & 0xFFFFFFFFUL),
				(ulint)(offset >> 32), chunk);
		if (!success) {
			goto error;
		}

		/* check corruption and retry */
		for (chunk_offset = 0; chunk_offset < chunk; chunk_offset += UNIV_PAGE_SIZE) {
			if (buf_page_is_corrupted(page + chunk_offset)) {
				retry_count--;
				if (retry_count == 0) {
					fprintf(stderr, "xtrabackup: Error: 10 retries resulted in fail. This file seems to be corrupted.\n");
					goto error;
				}
				fprintf(stderr, "xtrabackup: Database page corruption detected at page %lu. retrying...\n",
					(ulint)((offset + (ib_longlong)chunk_offset) >> UNIV_PAGE_SIZE_SHIFT));
				goto read_retry;
			}
		}

		if (xtrabackup_incremental) {
			for (chunk_offset = 0; chunk_offset < chunk; chunk_offset += UNIV_PAGE_SIZE) {
				/* newer page */
				/* This condition may be OK for header, ibuf and fsp */
				if (ut_dulint_cmp(incremental_lsn,
					mach_read_from_8(page + chunk_offset + FIL_PAGE_LSN)) < 0) {
	/* ========================================= */
	ib_longlong page_offset;

	if (page_in_buffer == UNIV_PAGE_SIZE/4) {
		/* flush buffer */
		success = os_file_write(dst_path, dst_file, incremental_buffer,
			((incremental_buffers * (UNIV_PAGE_SIZE/4))
				<< UNIV_PAGE_SIZE_SHIFT) & 0xFFFFFFFFUL,
			(incremental_buffers * (UNIV_PAGE_SIZE/4))
				>> (32 - UNIV_PAGE_SIZE_SHIFT),
			page_in_buffer * UNIV_PAGE_SIZE);
		if (!success) {
			goto error;
		}

		incremental_buffers++;

		/* clear buffer */
		bzero(incremental_buffer, (UNIV_PAGE_SIZE/4) * UNIV_PAGE_SIZE);
		page_in_buffer = 0;
		mach_write_to_4(incremental_buffer, 0x78747261UL);/*"xtra"*/
		page_in_buffer++;
	}

	page_offset = ((offset + (ib_longlong)chunk_offset) >> UNIV_PAGE_SIZE_SHIFT);
	ut_a(page_offset >> 32 == 0);

	mach_write_to_4(incremental_buffer + page_in_buffer * 4, (ulint)page_offset);
	memcpy(incremental_buffer + page_in_buffer * UNIV_PAGE_SIZE,
	       page + chunk_offset, UNIV_PAGE_SIZE);

	page_in_buffer++;
	/* ========================================= */
				}
			}
		} else {
			success = os_file_write(dst_path, dst_file, page,
				(ulint)(offset & 0xFFFFFFFFUL),
				(ulint)(offset >> 32), chunk);
			if (!success) {
				goto error;
			}
		}

	}

	if (xtrabackup_incremental) {
		/* termination */
		if (page_in_buffer != UNIV_PAGE_SIZE/4) {
			mach_write_to_4(incremental_buffer + page_in_buffer * 4, 0xFFFFFFFFUL);
		}

		mach_write_to_4(incremental_buffer, 0x58545241UL);/*"XTRA"*/

		/* flush buffer */
		success = os_file_write(dst_path, dst_file, incremental_buffer,
			((incremental_buffers * (UNIV_PAGE_SIZE/4))
				<< UNIV_PAGE_SIZE_SHIFT) & 0xFFFFFFFFUL,
			(incremental_buffers * (UNIV_PAGE_SIZE/4))
				>> (32 - UNIV_PAGE_SIZE_SHIFT),
			page_in_buffer * UNIV_PAGE_SIZE);
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
	if (src_file != -1)
		os_file_close(src_file);
	if (dst_file != -1)
		os_file_close(dst_file);
	if (buf2)
		ut_free(buf2);
	fprintf(stderr, "xtrabackup: Error: xtrabackup_copy_datafile() failed.\n");
	return(TRUE); /*ERROR*/
}

my_bool
xtrabackup_copy_logfile(dulint from_lsn, my_bool is_last)
{
	/* definition from recv_recovery_from_checkpoint_start() */
	log_group_t*	group;
	log_group_t*	up_to_date_group;
	dulint		old_scanned_lsn;
	dulint		group_scanned_lsn;
	dulint		contiguous_lsn;

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
"xtrabackup: Log block no %lu at lsn %lu %lu has\n"
"xtrabackup: ok header, but checksum field contains %lu, should be %lu\n",
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
			if (finished && !is_last
			    && group_scanned_lsn.low % OS_FILE_LOG_BLOCK_SIZE)
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

		if (finished && group_scanned_lsn.low % OS_FILE_LOG_BLOCK_SIZE) {
			/* if continue, it will start from align_down(group_scanned_lsn) */
			log_copy_offset -= OS_FILE_LOG_BLOCK_SIZE;
		}

		if(!success) {
			if (!xtrabackup_stream) {
				fprintf(stderr, "xtrabackup: Error: os_file_write to %s\n", dst_log_path);
			} else {
				fprintf(stderr, "xtrabackup: Error: write to stdout\n");
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
	fprintf(stderr, "xtrabackup: Error: xtrabackup_copy_logfile() failed.\n");
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
		os_thread_sleep(200000); /*0.2 sec*/

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
	/* currently, for --backup only */
	ut_a(xtrabackup_backup);

	while (log_copying) {
		os_thread_sleep(1000000); /*1 sec*/

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
	ulint	i;
	
	segment = *((ulint*)arg);

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
	dulint latest_cp;

#ifdef USE_POSIX_FADVISE
	fprintf(stderr, "xtrabackup: uses posix_fadvise().\n");
#endif

	/* cd to datadir */

	if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
	{
		fprintf(stderr, "xtrabackup: cannot my_setwd %s\n", mysql_real_data_home);
		exit(1);
	}
	fprintf(stderr, "xtrabackup: cd to %s\n", mysql_real_data_home);

	mysql_data_home= mysql_data_home_buff;
	mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
	mysql_data_home[1]=0;

	/* set read only */
	srv_read_only = TRUE;

	/* initialize components */
        if(innodb_init_param())
                exit(1);

        if (srv_file_flush_method_str == NULL) {
        	/* These are the default options */
#if (MYSQL_VERSION_ID < 50100)
		srv_unix_file_flush_method = SRV_UNIX_FDATASYNC;
#else /* MYSQL_VERSION_ID < 51000 */
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;
#endif
		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
#ifndef __WIN__        
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
		fprintf(stderr,"xtrabackup: use O_DIRECT\n");
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "littlesync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_LITTLESYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "nosync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_NOSYNC;
#else
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "normal")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_NORMAL;
	  	os_aio_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "unbuffered")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
	  	os_aio_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str,
							"async_unbuffered")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;	
#endif
	} else {
	  	fprintf(stderr, 
          	"xtrabackup: Unrecognized value %s for innodb_flush_method\n",
          				srv_file_flush_method_str);
	  	exit(1);
	}

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
	ibool	create_new_db;
	ibool	log_file_created;
	ibool	log_created	= FALSE;
	ibool	log_opened	= FALSE;
	dulint	min_flushed_lsn;
	dulint	max_flushed_lsn;
	ulint   sum_of_new_sizes;
	ulint	err;
	ulint	i;




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
"xtrabackup: Could not open or create data files.\n"
"xtrabackup: If you tried to add new data files, and it failed here,\n"
"xtrabackup: you should now edit innodb_data_file_path in my.cnf back\n"
"xtrabackup: to what it was, and remove the new ibdata files InnoDB created\n"
"xtrabackup: in this failed attempt. InnoDB only wrote those files full of\n"
"xtrabackup: zeros, but did not yet use them in any way. But be careful: do not\n"
"xtrabackup: remove old data files which contain your precious data!\n");

		//return((int) err);
		exit(1);
	}

	/* create_new_db must not be TRUE.. */
	if (create_new_db) {
		fprintf(stderr, "xtrabackup: Something wrong with source files...\n");
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
	"xtrabackup: Error: all log files must be created at the same time.\n"
	"xtrabackup: All log files must be created also in database creation.\n"
	"xtrabackup: If you want bigger or smaller log files, shut down the\n"
	"xtrabackup: database and make sure there were no errors in shutdown.\n"
	"xtrabackup: Then delete the existing log files. Edit the .cnf file\n"
	"xtrabackup: and start the database again.\n");

			//return(DB_ERROR);
			exit(1);
		}
	}

	/* log_file_created must not be TRUE, if online */
	if (log_file_created) {
		fprintf(stderr, "xtrabackup: Something wrong with source files...\n");
		exit(1);
	}

	fil_load_single_table_tablespaces();

	}

	if (!xtrabackup_stream) {

	/* create target dir if not exist */
	if (!my_stat(xtrabackup_target_dir,&stat_info,MYF(0))
		&& (my_mkdir(xtrabackup_target_dir,0777,MYF(0)) < 0)){
		fprintf(stderr,"xtrabackup: Error: cannot mkdir %d: %s\n",my_errno,xtrabackup_target_dir);
		exit(1);
	}

	} else {
		fprintf(stderr,"xtrabackup: Stream mode.\n");
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
	log_group_t*	max_cp_group;
	ulint		max_cp_field;
	byte*		buf;
	byte		log_hdr_buf_[LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE];
	byte*		log_hdr_buf;
	ulint		err;

	ibool		success;

	/* start back ground thread to copy newer log */
	os_thread_id_t log_copying_thread_id;

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
		srv_normalize_path_for_win(dst_log_path);
		/* os_file_create reads srv_unix_file_flush_method for OS_DATA_FILE*/
		dst_log = os_file_create(dst_log_path, OS_FILE_CREATE,
						OS_FILE_AIO, OS_DATA_FILE, &success);

                if (!success) {
                        /* The following call prints an error message */
                        os_file_get_last_error(TRUE);

                        fprintf(stderr,
"xtrabackup: error: cannot open %s\n",
                                dst_log_path);
                        exit(1);
                }

#ifdef USE_POSIX_FADVISE
		posix_fadvise(dst_log, 0, 0, POSIX_FADV_DONTNEED);
#endif

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
		if (dst_log != -1)
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
		ptr1 = strstr(space->name, SRV_PATH_SEPARATOR_STR);
		ptr2 = strstr(ptr1 + 1, SRV_PATH_SEPARATOR_STR);
		if(space->id && ptr2) {
			/* single table space */
			*ptr2 = 0; /* temporary (it's my lazy..)*/
			sprintf(path, "%s%s",xtrabackup_target_dir,ptr1);
			*ptr2 = SRV_PATH_SEPARATOR;

			if (!my_stat(path,&stat_info,MYF(0))
				&& (my_mkdir(path,0777,MYF(0)) < 0)){

				fprintf(stderr,"xtrabackup: Error: cannot mkdir %d: %s\n",my_errno,path);
				exit(1);
			}
		}

		node = UT_LIST_GET_FIRST(space->chain);
                while (node != NULL) {
                        //printf("  node: name=%s, open=%d, size=%d\n",
                        //       node->name, node->open, node->size);

			/* copy the datafile */
			if(xtrabackup_copy_datafile(node)) {
				if(node->space->id == 0) {
					fprintf(stderr,"xtrabackup: Error: failed to copy system datafile.\n");
					exit(1);
				} else {
					printf("xtrabackup: Warining: failed to copy, but continuing.\n");
				}
			}

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

		srv_normalize_path_for_win(suspend_path);
		/* os_file_create reads srv_unix_file_flush_method */
		suspend_file = os_file_create(suspend_path, OS_FILE_OVERWRITE,
						OS_FILE_AIO, OS_DATA_FILE, &success);

		if (!success) {
			fprintf(stderr, "xtrabackup: Error: failed to create file 'xtrabackup_suspended'\n");
		}

		if (suspend_file != -1)
			os_file_close(suspend_file);

		exists = TRUE;
		while (exists) {
			os_thread_sleep(200000); /*0.2 sec*/
			success = os_file_status(suspend_path, &exists, &type);
			if (!success)
				break;
		}
		xtrabackup_suspend_at_end = FALSE; /* suspend is 1 time */
	}

	/* read the latest checkpoint lsn */
	latest_cp = ut_dulint_zero;
	{
		log_group_t*	max_cp_group;
		ulint	max_cp_field;
		ulint	err;

		err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

		if (err != DB_SUCCESS) {
			fprintf(stderr, "xtrabackup: Error: recv_find_max_checkpoint() failed.\n");
			goto skip_last_cp;
		}

		log_group_read_checkpoint_info(max_cp_group, max_cp_field);

		latest_cp = mach_read_from_8(log_sys->checkpoint_buf + LOG_CHECKPOINT_LSN);

		if (!xtrabackup_stream) {
			printf("xtrabackup: The latest check point (for incremental): '%lu:%lu'\n",
				latest_cp.high, latest_cp.low);
		} else {
			fprintf(stderr, "xtrabackup: The latest check point (for incremental): '%lu:%lu'\n",
				latest_cp.high, latest_cp.low);
		}
	}
skip_last_cp:

	/* output to metadata file */
	{
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_target_dir, XTRABACKUP_METADATA_FILENAME);

		if(!xtrabackup_incremental) {
			strcpy(metadata_type, "full-backuped");
			metadata_from_lsn = ut_dulint_zero;
		} else {
			strcpy(metadata_type, "incremental");
			metadata_from_lsn = incremental_lsn;
		}
		metadata_to_lsn = latest_cp;

		if (xtrabackup_write_metadata(filename))
			fprintf(stderr, "xtrabackup: error: xtrabackup_write_metadata()\n");
	}

	/* stop log_copying_thread */
	log_copying = FALSE;
	if (!xtrabackup_stream) {
		printf("xtrabackup: Stopping log copying thread");
		while (log_copying_running) {
			printf(".");
			os_thread_sleep(200000); /*0.2 sec*/
		}
		printf("\n");
	} else {
		while (log_copying_running)
			os_thread_sleep(200000); /*0.2 sec*/
	}

	if (!log_copying_succeed) {
		fprintf(stderr, "xtrabackup: Error: log_copying_thread failed.\n");
		exit(1);
	}

	if (!xtrabackup_stream)
	        os_file_close(dst_log);

	if (wait_throttle)
		os_event_free(wait_throttle);

	if (!xtrabackup_stream) {
        	printf("xtrabackup: Transaction log of lsn (%lu %lu) to (%lu %lu) was copied.\n",
                	checkpoint_lsn_start.high, checkpoint_lsn_start.low,
                	log_copy_scanned_lsn.high, log_copy_scanned_lsn.low);
	} else {
		fprintf(stderr, "xtrabackup: Transaction log of lsn (%lu %lu) to (%lu %lu) was copied.\n",
			checkpoint_lsn_start.high, checkpoint_lsn_start.low,
			log_copy_scanned_lsn.high, log_copy_scanned_lsn.low);
	}
}

/* ================= stats ================= */
my_bool
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

	n_pages = sum_data = n_recs = 0;
	n_pages_extern = sum_data_extern = 0;

	if (level == 0)
		fprintf(stdout, "        leaf pages: ");
	else
		fprintf(stdout, "     level %lu pages: ", level);

	mtr_start(&mtr);

#if (MYSQL_VERSION_ID < 50100)
	mtr_x_lock(&(index->tree->lock), &mtr);
	page = btr_root_get(index->tree, &mtr);
#else /* MYSQL_VERSION_ID < 51000 */
	mtr_x_lock(&(index->lock), &mtr);
	page = btr_root_get(index, &mtr);
#endif

	space = buf_frame_get_space_id(page);

	while (level != btr_page_get_level(page, &mtr)) {

		ut_a(btr_page_get_level(page, &mtr) > 0);

		page_cur_set_before_first(page, &cursor);
		page_cur_move_to_next(&cursor);

		node_ptr = page_cur_get_rec(&cursor);
		offsets = rec_get_offsets(node_ptr, index, offsets,
					ULINT_UNDEFINED, &heap);
		page = btr_node_ptr_get_child(node_ptr, offsets, &mtr);
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
		mem_heap_t*	heap	= NULL;
		ulint	offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*	offsets	= offsets_;

		*offsets_ = (sizeof offsets_) / sizeof *offsets_;

		page_cur_set_before_first(page, &cur);
		page_cur_move_to_next(&cur);

		for (;;) {
			if (page_cur_is_after_last(&cur)) {
				break;
			}

			offsets = rec_get_offsets(cur.rec, index, offsets,
						ULINT_UNDEFINED, &heap);
			n_fields = rec_offs_n_fields(offsets);

			for (i = 0; i < n_fields; i++) {
				if (rec_offs_nth_extern(offsets, i)) {
					page_t*	page;
					ulint	space_id;
					ulint	page_no;
					ulint	offset;
					ulint	extern_len;
					byte*	blob_header;
					ulint	part_len;
					mtr_t	mtr;
					ulint	local_len;
					byte*	data;

					data = rec_get_nth_field(cur.rec, offsets, i, &local_len);

					ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);
					local_len -= BTR_EXTERN_FIELD_REF_SIZE;

					space_id = mach_read_from_4(data + local_len + BTR_EXTERN_SPACE_ID);
					page_no = mach_read_from_4(data + local_len + BTR_EXTERN_PAGE_NO);
					offset = mach_read_from_4(data + local_len + BTR_EXTERN_OFFSET);
					extern_len = mach_read_from_4(data + local_len + BTR_EXTERN_LEN + 4);

					if (offset != FIL_PAGE_DATA)
						fprintf(stderr, "\nWarning: several record may share same external page.\n");

					for (;;) {
						mtr_start(&mtr);

						page = buf_page_get(space_id, page_no, RW_S_LATCH, &mtr);
						blob_header = page + offset;
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


						mtr_commit(&mtr);

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
	
		page = btr_page_get(space, right_page_no, RW_X_LATCH, &mtr);

		goto loop;
	}
	mem_heap_free(heap);

	if (level == 0)
		fprintf(stdout, "recs=%lld, ", n_recs);

	fprintf(stdout, "pages=%lld, data=%lld bytes, data/pages=%lld%%",
		n_pages, sum_data,
		((sum_data * 100)/ UNIV_PAGE_SIZE)/n_pages);


	if (level == 0 && n_pages_extern) {
		putc('\n', stdout);
		/* also scan blob pages*/
		fprintf(stdout, "    external pages: ");

		fprintf(stdout, "pages=%lld, data=%lld bytes, data/pages=%lld%%",
			n_pages_extern, sum_data_extern,
			((sum_data_extern * 100)/ UNIV_PAGE_SIZE)/n_pages_extern);
	}

	putc('\n', stdout);

	if (level > 0) {
		xtrabackup_stats_level(index, level - 1);
	}

	return(TRUE);
}

void
xtrabackup_stats_func(void)
{
	/* cd to datadir */

	if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
	{
		fprintf(stderr, "xtrabackup: cannot my_setwd %s\n", mysql_real_data_home);
		exit(1);
	}
	fprintf(stderr, "xtrabackup: cd to %s\n", mysql_real_data_home);

	mysql_data_home= mysql_data_home_buff;
	mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
	mysql_data_home[1]=0;

	/* set read only */
	srv_read_only = TRUE;
	srv_fake_write = TRUE;

	/* initialize components */
	if(innodb_init_param())
		exit(1);

	fprintf(stderr, "xtrabackup: Starting 'read-only' InnoDB instance to gather index statistics.\n"
		"xtrabackup: Using %lld bytes for buffer pool (set by --use-memory parameter)\n",
		xtrabackup_use_memory);

	if(innodb_init())
		exit(1);

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

	mutex_enter(&kernel_mutex);
	srv_fatal_semaphore_wait_threshold += 72000; /* 20 hours */
	mutex_exit(&kernel_mutex);

	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);

	btr_pcur_open_at_index_side(TRUE, sys_index, BTR_SEARCH_LEAF, &pcur,
								TRUE, &mtr);
loop:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)) {
		/* end of index */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		
		mutex_exit(&(dict_sys->mutex));

		/* Restore the fatal semaphore wait timeout */

		mutex_enter(&kernel_mutex);
		srv_fatal_semaphore_wait_threshold -= 72000; /* 20 hours */
		mutex_exit(&kernel_mutex);

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

		table = dict_table_get_low(table_name);
		mem_free(table_name);


		if (xtrabackup_tables) {
			char *p;
			int regres;

			p = strstr(table->name, SRV_PATH_SEPARATOR_STR);

			if (p)
				*p = '.';

			regres = regexec(&tables_regex, table->name, 1, tables_regmatch, 0);

			if (p)
				*p = SRV_PATH_SEPARATOR;

			if ( regres == REG_NOMATCH )
				goto skip;
		}


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
				dict_update_statistics_low(table, TRUE);
			}

			//dict_table_print_low(table);

			index = UT_LIST_GET_FIRST(table->indexes);
			while (index != NULL) {
{
	ib_longlong	n_vals;

	if (index->n_user_defined_cols > 0) {
		n_vals = index->stat_n_diff_key_vals[
					index->n_user_defined_cols];
	} else {
		n_vals = index->stat_n_diff_key_vals[1];
	}

	fprintf(stdout,
		"  table: %s, index: %s, space id: %lu, root page %lu\n"
		"  estimated statistics in dictionary:\n"
		"    key vals: %lu, leaf pages %lu, size pages %lu\n"
		"  real statistics:\n",
		table->name, index->name,
		(ulong) index->space,
#if (MYSQL_VERSION_ID < 50100)
		(ulong) index->tree->page,
#else /* MYSQL_VERSION_ID < 51000 */
		(ulong) index->page,
#endif
		(ulong) n_vals,
		(ulong) index->stat_n_leaf_pages,
		(ulong) index->stat_index_size);

	{
		mtr_t	mtr;
		page_t*	root;
		ulint	n;

		mtr_start(&mtr);

#if (MYSQL_VERSION_ID < 50100)
		mtr_x_lock(&(index->tree->lock), &mtr);
		root = btr_root_get(index->tree, &mtr);
#else /* MYSQL_VERSION_ID < 51000 */
		mtr_x_lock(&(index->lock), &mtr);
		root = btr_root_get(index, &mtr);
#endif
		n = btr_page_get_level(root, &mtr);

		xtrabackup_stats_level(index, n);

		mtr_commit(&mtr);
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

	/* shutdown InnoDB */
	if(innodb_end())
		exit(1);
}

/* ================= prepare ================= */

my_bool
xtrabackup_init_temp_log(void)
{
	os_file_t	src_file = -1;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	ibool	success;

	ulint	field;
	byte*	log_buf;
	byte*	log_buf_ = NULL;

	ib_longlong	file_size;

	dulint	max_no;
	dulint	max_lsn;
	ulint	max_field;
	dulint	checkpoint_no;

	ulint	fold;

	max_no = ut_dulint_zero;

	if(!xtrabackup_incremental_dir) {
		sprintf(dst_path, "%s%s", xtrabackup_target_dir, "/ib_logfile0");
		sprintf(src_path, "%s%s", xtrabackup_target_dir, "/xtrabackup_logfile");
	} else {
		sprintf(dst_path, "%s%s", xtrabackup_incremental_dir, "/ib_logfile0");
		sprintf(src_path, "%s%s", xtrabackup_incremental_dir, "/xtrabackup_logfile");
	}

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);
retry:
	src_file = os_file_create_simple_no_error_handling(
					src_path, OS_FILE_OPEN,
					OS_FILE_READ_WRITE /* OS_FILE_READ_ONLY */, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		fprintf(stderr,
"xtrabackup: Warning: cannot open %s. will try to find.\n",
			src_path);

		/* check if ib_logfile0 may be xtrabackup_logfile */
		src_file = os_file_create_simple_no_error_handling(
				dst_path, OS_FILE_OPEN,
				OS_FILE_READ_WRITE /* OS_FILE_READ_ONLY */, &success);
		if (!success) {
			os_file_get_last_error(TRUE);
			fprintf(stderr,
"  xtrabackup: Fatal error: cannot find %s.\n",
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
"  xtrabackup: 'ib_logfile0' seems to be 'xtrabackup_logfile'. will retry.\n");

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
"  xtrabackup: Fatal error: cannot find %s.\n",
		src_path);

		ut_free(log_buf_);
		log_buf_ = NULL;

		os_file_close(src_file);
		src_file = -1;

		goto error;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(src_file, 0, 0, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(src_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(src_file, src_path, "OPEN");
	}

	file_size = os_file_get_size_as_iblonglong(src_file);


	/* TODO: We should skip the following modifies, if it is not the first time. */
	log_buf_ = ut_malloc(UNIV_PAGE_SIZE * 129);
	log_buf = ut_align(log_buf_, UNIV_PAGE_SIZE);

	/* read log file header */
	success = os_file_read(src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	if ( ut_memcmp(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
			(byte*)"xtrabkup", (sizeof "xtrabkup") - 1) != 0 ) {
		printf("xtrabackup: notice: xtrabackup_logfile was already used to '--prepare'.\n");
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
		fprintf(stderr, "xtrabackup: No valid checkpoint found.\n");
		goto error;
	}


	/* It seems to be needed to overwrite the both checkpoint area. */
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

	/* expand file size (9/8) and align to UNIV_PAGE_SIZE */

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

	/* TODO: We should judge whether the file is already expanded or not... */
	{
		ulint	expand;

		memset(log_buf, 0, UNIV_PAGE_SIZE * 128);
		expand = file_size / UNIV_PAGE_SIZE / 8;

		for (; expand > 128; expand -= 128) {
			success = os_file_write(src_path, src_file, log_buf,
					(ulint)(file_size & 0xFFFFFFFFUL),
					(ulint)(file_size >> 32),
					UNIV_PAGE_SIZE * 128);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE * 128;
		}

		if (expand) {
			success = os_file_write(src_path, src_file, log_buf,
					(ulint)(file_size & 0xFFFFFFFFUL),
					(ulint)(file_size >> 32),
					expand * UNIV_PAGE_SIZE);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE * expand;
		}
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

	printf("xtrabackup: xtrabackup_logfile detected: size=%lld, start_lsn=(%lu %lu)\n",
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
	fprintf(stderr, "xtrabackup: Error: xtrabackup_init_temp_log() failed.\n");
	return(TRUE); /*ERROR*/
}

void
xtrabackup_apply_delta(
	const char*	dirname,	/* in: dir name of incremental */
	const char*	dbname,		/* in: database name (ibdata: NULL) */
	const char*	filename,	/* in: file name (not a path),
					including the .delta extension */
	my_bool check_newer)
{
	os_file_t	src_file = -1;
	os_file_t	dst_file = -1;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	ibool	success;

	ibool	last_buffer = FALSE;
	ulint	page_in_buffer;
	ulint	incremental_buffers = 0;


	ut_a(xtrabackup_incremental);

	if (dbname) {
		sprintf(src_path, "%s/%s/%s", dirname, dbname, filename);
		sprintf(dst_path, "%s/%s/%s", xtrabackup_real_target_dir, dbname, filename);
	} else {
		sprintf(src_path, "%s/%s", dirname, filename);
		sprintf(dst_path, "%s/%s", xtrabackup_real_target_dir, filename);
	}
	dst_path[strlen(dst_path) - 6] = '\0';

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);

	src_file = os_file_create_simple_no_error_handling(
		src_path, OS_FILE_OPEN, OS_FILE_READ_WRITE, &success);
	if (!success) {
		os_file_get_last_error(TRUE);
		fprintf(stderr,
			"xtrabackup: error: cannot open %s\n",
			src_path);
		goto error;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(src_file, 0, 0, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(src_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(src_file, src_path, "OPEN");
	}

	dst_file = os_file_create_simple_no_error_handling(
		dst_path, OS_FILE_OPEN, OS_FILE_READ_WRITE, &success);
	if (!success) {
		os_file_get_last_error(TRUE);
		fprintf(stderr,
			"xtrabackup: error: cannot open %s\n",
			dst_path);
		goto error;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(dst_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(dst_file, dst_path, "OPEN");
	}

	printf("Applying %s ...\n", src_path);

	while (!last_buffer) {
		ulint cluster_header;

		/* read to buffer */
		/* first block of block cluster */
		success = os_file_read(src_file, incremental_buffer,
			((incremental_buffers * (UNIV_PAGE_SIZE/4))
				<< UNIV_PAGE_SIZE_SHIFT) & 0xFFFFFFFFUL,
			(incremental_buffers * (UNIV_PAGE_SIZE/4))
				>> (32 - UNIV_PAGE_SIZE_SHIFT),
			UNIV_PAGE_SIZE);
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
				fprintf(stderr,
					"xtrabackup: error: %s seems not .delta file.\n",
					src_path);
				goto error;
		}

		for (page_in_buffer = 1; page_in_buffer < UNIV_PAGE_SIZE/4; page_in_buffer++) {
			if (mach_read_from_4(incremental_buffer + page_in_buffer * 4)
			    == 0xFFFFFFFFUL)
				break;
		}

		ut_a(last_buffer || page_in_buffer == UNIV_PAGE_SIZE/4);

		/* read whole of the cluster */
		success = os_file_read(src_file, incremental_buffer,
			((incremental_buffers * (UNIV_PAGE_SIZE/4))
				<< UNIV_PAGE_SIZE_SHIFT) & 0xFFFFFFFFUL,
			(incremental_buffers * (UNIV_PAGE_SIZE/4))
				>> (32 - UNIV_PAGE_SIZE_SHIFT),
			page_in_buffer * UNIV_PAGE_SIZE);
		if (!success) {
			goto error;
		}

		for (page_in_buffer = 1; page_in_buffer < UNIV_PAGE_SIZE/4; page_in_buffer++) {
			ulint page_offset;

			page_offset = mach_read_from_4(incremental_buffer + page_in_buffer * 4);

			if (page_offset == 0xFFFFFFFFUL)
				break;

			/* apply blocks in the cluster */
			if (ut_dulint_cmp(incremental_lsn,
				mach_read_from_8(incremental_buffer
						 + page_in_buffer * UNIV_PAGE_SIZE
						 + FIL_PAGE_LSN)) >= 0)
				continue;

			success = os_file_write(dst_path, dst_file,
					incremental_buffer + page_in_buffer * UNIV_PAGE_SIZE,
					(page_offset << UNIV_PAGE_SIZE_SHIFT) & 0xFFFFFFFFUL,
					page_offset >> (32 - UNIV_PAGE_SIZE_SHIFT),
					UNIV_PAGE_SIZE);
			if (!success) {
				goto error;
			}
		}

		incremental_buffers++;
	}

	if (src_file != -1)
		os_file_close(src_file);
	if (dst_file != -1)
		os_file_close(dst_file);
	return;

error:
	if (src_file != -1)
		os_file_close(src_file);
	if (dst_file != -1)
		os_file_close(dst_file);
	fprintf(stderr, "xtrabackup: Error: xtrabackup_apply_delta() failed.\n");
	return;
}

void
xtrabackup_apply_deltas(my_bool check_newer)
{
	int		ret;
	char		dbpath[FN_REFLEN];
	os_file_dir_t	dir;
	os_file_dir_t	dbdir;
	os_file_stat_t	dbinfo;
	os_file_stat_t	fileinfo;
	ulint		err 		= DB_SUCCESS;
	static char	current_dir[2];

	current_dir[0] = FN_CURLIB;
	current_dir[1] = 0;
	srv_data_home = current_dir;

	/* datafile */
	dbdir = os_file_opendir(xtrabackup_incremental_dir, FALSE);

	if (dbdir != NULL) {
		ret = fil_file_readdir_next_file(&err, xtrabackup_incremental_dir, dbdir,
							&fileinfo);
		while (ret == 0) {
			if (fileinfo.type == OS_FILE_TYPE_DIR) {
				goto next_file_item_1;
			}

			if (strlen(fileinfo.name) > 6
			    && 0 == strcmp(fileinfo.name + 
					strlen(fileinfo.name) - 6,
					".delta")) {
				xtrabackup_apply_delta(
					xtrabackup_incremental_dir, NULL,
					fileinfo.name, check_newer);
			}
next_file_item_1:
			ret = fil_file_readdir_next_file(&err,
							xtrabackup_incremental_dir, dbdir,
							&fileinfo);
		}

		os_file_closedir(dbdir);
	} else {
		fprintf(stderr, "xtrabackup: Cannot open dir %s\n", xtrabackup_incremental_dir);
	}

	/* single table tablespaces */
	dir = os_file_opendir(xtrabackup_incremental_dir, FALSE);

	if (dir == NULL) {
		fprintf(stderr, "xtrabackup: Cannot open dir %s\n", xtrabackup_incremental_dir);
	}

		ret = fil_file_readdir_next_file(&err, xtrabackup_incremental_dir, dir,
								&dbinfo);
	while (ret == 0) {
		if (dbinfo.type == OS_FILE_TYPE_FILE
		    || dbinfo.type == OS_FILE_TYPE_UNKNOWN) {

		        goto next_datadir_item;
		}

		sprintf(dbpath, "%s/%s", xtrabackup_incremental_dir,
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

				if (strlen(fileinfo.name) > 6
				    && 0 == strcmp(fileinfo.name + 
						strlen(fileinfo.name) - 6,
						".delta")) {
				        /* The name ends in .ibd; try opening
					the file */
					xtrabackup_apply_delta(
						xtrabackup_incremental_dir, dbinfo.name,
						fileinfo.name, check_newer);
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
						xtrabackup_incremental_dir,
								dir, &dbinfo);
	}

	os_file_closedir(dir);

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
	if(!xtrabackup_incremental_dir) {
		sprintf(dst_path, "%s%s", xtrabackup_target_dir, "/ib_logfile0");
		sprintf(src_path, "%s%s", xtrabackup_target_dir, "/xtrabackup_logfile");
	} else {
		sprintf(dst_path, "%s%s", xtrabackup_incremental_dir, "/ib_logfile0");
		sprintf(src_path, "%s%s", xtrabackup_incremental_dir, "/xtrabackup_logfile");
	}

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);

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

#ifdef USE_POSIX_FADVISE
	posix_fadvise(src_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(src_file, src_path, "OPEN");
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
	fprintf(stderr, "xtrabackup: Error: xtrabackup_close_temp_log() failed.\n");
	return(TRUE); /*ERROR*/
}

void
xtrabackup_prepare_func(void)
{
	/* cd to target-dir */

	if (my_setwd(xtrabackup_real_target_dir,MYF(MY_WME)))
	{
		fprintf(stderr, "xtrabackup: cannot my_setwd %s\n", xtrabackup_real_target_dir);
		exit(1);
	}
	fprintf(stderr, "xtrabackup: cd to %s\n", xtrabackup_real_target_dir);

	xtrabackup_target_dir= mysql_data_home_buff;
	xtrabackup_target_dir[0]=FN_CURLIB;		// all paths are relative from here
	xtrabackup_target_dir[1]=0;

	/* read metadata of target */
	{
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_target_dir, XTRABACKUP_METADATA_FILENAME);

		if (xtrabackup_read_metadata(filename))
			fprintf(stderr, "xtrabackup: error: xtrabackup_read_metadata()\n");

		if (!strcmp(metadata_type, "full-backuped")) {
			fprintf(stderr, "xtrabackup: This target seems to be not prepared yet.\n");
		} else if (!strcmp(metadata_type, "full-prepared")) {
			fprintf(stderr, "xtrabackup: This target seems to be already prepared.\n");
			goto skip_check;
		} else {
			fprintf(stderr, "xtrabackup: This target seems not to have correct metadata...\n");
		}

		if (xtrabackup_incremental) {
			fprintf(stderr,
			"xtrabackup: error: applying incremental backup needs target prepared.\n");
			exit(1);
		}
skip_check:
		if (xtrabackup_incremental
		    && ut_dulint_cmp(metadata_to_lsn, incremental_lsn) < 0) {
			fprintf(stderr,
			"xtrabackup: error: This incremental backup seems to be too new for the target.\n");
			exit(1);
		}
	}

	/* Create logfiles for recovery from 'xtrabackup_logfile', before start InnoDB */
	srv_max_n_threads = 1000;
	os_sync_mutex = NULL;
	os_sync_init();
	sync_init();
	os_io_init_simple();
	if(xtrabackup_init_temp_log())
		goto error;

	if(xtrabackup_incremental)
		xtrabackup_apply_deltas(TRUE);

	sync_close();
	sync_initialized = FALSE;
	os_sync_free();
	os_sync_mutex = NULL;

	/* check the accessibility of target-dir */
	/* ############# TODO ##################### */

	if(innodb_init_param())
		goto error;

	/* increase IO threads */
	if(srv_n_file_io_threads < 10) {
		srv_n_file_io_threads = 10;
	}

	fprintf(stderr, "xtrabackup: Starting InnoDB instance for recovery.\n"
		"xtrabackup: Using %lld bytes for buffer pool (set by --use-memory parameter)\n",
		xtrabackup_use_memory);

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
	if (xtrabackup_export) {
		printf("xtrabackup: export option is specified.\n");
		if (innobase_file_per_table) {
			fil_system_t*	system = fil_system;
			fil_space_t*	space;
			fil_node_t*	node;
			os_file_t	info_file = -1;
			char		info_file_path[FN_REFLEN];
			ibool		success;
			char		table_name[FN_REFLEN];

			byte*		page;
			byte*		buf = NULL;

			buf = ut_malloc(UNIV_PAGE_SIZE * 2);
			page = ut_align(buf, UNIV_PAGE_SIZE);

			/* flush insert buffer at shutdwon */
			innobase_fast_shutdown = 0;

			mutex_enter(&(system->mutex));

			space = UT_LIST_GET_FIRST(system->space_list);
			while (space != NULL) {
				/* treat file_per_table only */
				if (space->purpose != FIL_TABLESPACE
				    || space->id == 0) {
					space = UT_LIST_GET_NEXT(space_list, space);
					continue;
				}

				node = UT_LIST_GET_FIRST(space->chain);
				while (node != NULL) {
					int len;
					char *next, *prev, *p;
					dict_table_t*	table;
					dict_index_t*	index;
					ulint		n_index;

					/* node exist == file exist, here */
					strncpy(info_file_path, node->name, FN_REFLEN);
					len = strlen(info_file_path);
					info_file_path[len - 3] = 'e';
					info_file_path[len - 2] = 'x';
					info_file_path[len - 1] = 'p';

					p = info_file_path;
					prev = NULL;
					while (next = strstr(p, SRV_PATH_SEPARATOR_STR))
					{
						prev = p;
						p = next + 1;
					}
					info_file_path[len - 4] = 0;
					strncpy(table_name, prev, FN_REFLEN);

					info_file_path[len - 4] = '.';

					mutex_exit(&(system->mutex));
					mutex_enter(&(dict_sys->mutex));

					table = dict_table_get_low(table_name);
					if (!table) {
						fprintf(stderr,
"xtrabackup: error: cannot find dictionary record of table %s\n", table_name);
						goto next_node;
					}
					index = dict_table_get_first_index(table);
					n_index = UT_LIST_GET_LEN(table->indexes);
					if (n_index > 31) {
						fprintf(stderr,
"xtrabackup: error: sorry, cannot export over 31 indexes for now.\n");
						goto next_node;
					}

					/* init exp file */
					bzero(page, UNIV_PAGE_SIZE);
					mach_write_to_4(page    , 0x78706f72UL);
					mach_write_to_4(page + 4, 0x74696e66UL);/*"xportinf"*/
					mach_write_to_4(page + 8, n_index);
					strncpy(page + 12, table_name, 500);

					printf(
"xtrabackup: export metadata of table '%s' to file `%s` (%lu indexes)\n",
						table_name, info_file_path, n_index);

					n_index = 1;
					while (index) {
						mach_write_to_8(page + n_index * 512, index->id);
						mach_write_to_4(page + n_index * 512 + 8,
#if (MYSQL_VERSION_ID < 50100)
								index->tree->page);
#else /* MYSQL_VERSION_ID < 51000 */
								index->page);
#endif
						strncpy(page + n_index * 512 + 12, index->name, 500);

						printf(
"xtrabackup:     name=%s, id.low=%lu, page=%lu\n",
							index->name,
							index->id.low,
#if (MYSQL_VERSION_ID < 50100)
							index->tree->page);
#else /* MYSQL_VERSION_ID < 51000 */
							index->page);
#endif

						index = dict_table_get_next_index(index);
						n_index++;
					}

					srv_normalize_path_for_win(info_file_path);
					info_file = os_file_create(info_file_path, OS_FILE_OVERWRITE,
								OS_FILE_AIO, OS_DATA_FILE, &success);
					if (!success) {
						os_file_get_last_error(TRUE);
						goto next_node;
					}
					success = os_file_write(info_file_path, info_file, page,
								0, 0, UNIV_PAGE_SIZE);
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
					if (info_file != -1) {
						os_file_close(info_file);
						info_file = -1;
					}
					mutex_exit(&(dict_sys->mutex));
					mutex_enter(&(system->mutex));

					node = UT_LIST_GET_NEXT(chain, node);
				}

				space = UT_LIST_GET_NEXT(space_list, space);
			}
			mutex_exit(&(system->mutex));

			ut_free(buf);
		} else {
			printf("xtrabackup: export option is for file_per_table only, disabled.\n");
		}
	}

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

	/* re-init necessary components */
	os_sync_init();
	sync_init();
	os_io_init_simple();

	if(xtrabackup_close_temp_log(TRUE))
		exit(1);

	/* output to metadata file */
	{
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_target_dir, XTRABACKUP_METADATA_FILENAME);

		strcpy(metadata_type, "full-prepared");

		if(xtrabackup_incremental
		   && ut_dulint_cmp(metadata_to_lsn, incremental_to_lsn) < 0)
			metadata_to_lsn = incremental_to_lsn;

		if (xtrabackup_write_metadata(filename))
			fprintf(stderr, "xtrabackup: error: xtrabackup_write_metadata()\n");
	}

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
		uint count;
		struct my_option *opt= (struct my_option *) my_long_options;
		optend= strcend((argv)[i], '=');
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

	if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
		exit(ho_error);

	if (strcmp(mysql_data_home, "./") == 0) {
		if (!xtrabackup_print_param)
			usage();
		printf("\nxtrabackup: Error: Please set parameter 'datadir'\n");
		exit(-1);
	}

	if (xtrabackup_tables) {
		/* init regexp */
		char errbuf[100];
		regerror(regcomp(&tables_regex,xtrabackup_tables,REG_EXTENDED),
				&tables_regex,errbuf,sizeof(errbuf));
		fprintf(stderr, "xtrabackup: tables regcomp(): %s\n",errbuf);
	}

	if (xtrabackup_backup && xtrabackup_incremental) {
		/* direct specification is only for --backup */
		/* and the lsn is prior to the other option */

		char* incremental_low;
		char* endchar;
		long long lsn_high, lsn_low;
		int error = 0;

		incremental_low = strstr(xtrabackup_incremental, ":");
		if (incremental_low) {
			*incremental_low = '\0';

			lsn_high = strtoll(xtrabackup_incremental, &endchar, 10);
			if (*endchar != '\0' || (lsn_high >> 32))
				error = 1;

			*incremental_low = ':';
			incremental_low++;

			lsn_low = strtoll(incremental_low, &endchar, 10);

			if (*endchar != '\0' || (lsn_low >> 32))
				error = 1;

			incremental_lsn = ut_dulint_create((ulint)lsn_high, (ulint)lsn_low);
		} else {
			error = 1;
		}

		if (error) {
			fprintf(stderr, "xtrabackup: value '%s' may be wrong format for incremental option.\n",
				xtrabackup_incremental);
			exit(-1);
		}

		/* allocate buffer for incremental backup (4096 pages) */
		incremental_buffer_base = malloc((UNIV_PAGE_SIZE/4 + 1) * UNIV_PAGE_SIZE);
		incremental_buffer = ut_align(incremental_buffer_base, UNIV_PAGE_SIZE);
	} else if (xtrabackup_backup && xtrabackup_incremental_basedir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_incremental_basedir, XTRABACKUP_METADATA_FILENAME);

		if (xtrabackup_read_metadata(filename)) {
			fprintf(stderr,
				"xtrabackup: error: failed to read metadata from %s\n",
				filename);
			exit(-1);
		}

		incremental_lsn = metadata_to_lsn;
		xtrabackup_incremental = xtrabackup_incremental_basedir; //dummy

		/* allocate buffer for incremental backup (4096 pages) */
		incremental_buffer_base = malloc((UNIV_PAGE_SIZE/4 + 1) * UNIV_PAGE_SIZE);
		incremental_buffer = ut_align(incremental_buffer_base, UNIV_PAGE_SIZE);
	} else if (xtrabackup_prepare && xtrabackup_incremental_dir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_incremental_dir, XTRABACKUP_METADATA_FILENAME);

		if (xtrabackup_read_metadata(filename)) {
			fprintf(stderr,
				"xtrabackup: error: failed to read metadata from %s\n",
				filename);
			exit(-1);
		}

		incremental_lsn = metadata_from_lsn;
		incremental_to_lsn = metadata_to_lsn;
		xtrabackup_incremental = xtrabackup_incremental_dir; //dummy

		/* allocate buffer for incremental backup (4096 pages) */
		incremental_buffer_base = malloc((UNIV_PAGE_SIZE/4 + 1) * UNIV_PAGE_SIZE);
		incremental_buffer = ut_align(incremental_buffer_base, UNIV_PAGE_SIZE);
	} else {
		/* allocate buffer for applying incremental (for header page only) */
		incremental_buffer_base = malloc((1 + 1) * UNIV_PAGE_SIZE);
		incremental_buffer = ut_align(incremental_buffer_base, UNIV_PAGE_SIZE);

		xtrabackup_incremental = NULL;
	}

	/* --print-param */
	if (xtrabackup_print_param) {
		/* === some variables from mysqld === */
		bzero((G_PTR) &mysql_tmpdir_list, sizeof(mysql_tmpdir_list));

		if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
			exit(1);

		printf("# This MySQL options file was generated by XtraBackup.\n");
		printf("[mysqld]\n");
		printf("datadir = %s\n", mysql_data_home);
		printf("tmpdir = %s\n", mysql_tmpdir_list.list[0]);
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
		if (xtrabackup_incremental) {
			printf("incremental backup from %lu:%lu is enabled.\n",
				incremental_lsn.high, incremental_lsn.low);
		}
	} else {
		if (xtrabackup_backup) {
			xtrabackup_suspend_at_end = TRUE;
			fprintf(stderr, "xtrabackup: suspend-at-end is enabled.\n");
		}
	}

	/* cannot execute both for now */
	{
		int num = 0;

		if (xtrabackup_backup) num++;
		if (xtrabackup_stats) num++;
		if (xtrabackup_prepare) num++;
		if (num != 1) { /* !XOR (for now) */
			usage();
			exit(-1);
		}
	}

	/* --backup */
	if (xtrabackup_backup)
		xtrabackup_backup_func();

	/* --stats */
	if (xtrabackup_stats)
		xtrabackup_stats_func();

	/* --prepare */
	if (xtrabackup_prepare)
		xtrabackup_prepare_func();

	free(incremental_buffer_base);

	if (xtrabackup_tables) {
		/* free regexp */
		regfree(&tables_regex);
	}

	exit(0);
}
