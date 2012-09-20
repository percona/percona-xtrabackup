/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009-2012 Percona Inc.
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

*******************************************************/

/* InnoDB portability helpers and interface to internal functions */

#ifndef INNODB_INT_H
#define INNODB_INT_H

#include <my_base.h>
#include <univ.i>
#include <fil0fil.h>
#include <os0file.h>
#include <hash0hash.h>
#include <btr0sea.h>
#include <log0log.h>
#include <log0recv.h>
#include <trx0sys.h>

#ifndef INNODB_VERSION_SHORT
#  define IB_INT64 ib_longlong
#  define LSN64 dulint
#  define MACH_READ_64 mach_read_from_8
#  define MACH_WRITE_64 mach_write_to_8
#  define OS_MUTEX_CREATE() os_mutex_create(NULL)
#  define xb_buf_page_is_corrupted(page, zip_size) buf_page_is_corrupted(page)
#  define xb_fil_space_create(name, space_id, zip_size, purpose) \
	fil_space_create(name, space_id, purpose)
#  define PAGE_ZIP_MIN_SIZE_SHIFT	10
#  define PAGE_ZIP_MIN_SIZE	(1 << PAGE_ZIP_MIN_SIZE_SHIFT)
#  define DICT_TF_ZSSIZE_SHIFT	1
#  define DICT_TF_ZSSIZE_MASK		(15 << DICT_TF_ZSSIZE_SHIFT)
#  define DICT_TF_FORMAT_ZIP	1
#  define DICT_TF_FORMAT_SHIFT		5
#  define SRV_SHUTDOWN_NONE	0
#else
#  define IB_INT64 ib_int64_t
#  define LSN64 ib_uint64_t
#  if (MYSQL_VERSION_ID < 50500)
#    define MACH_READ_64 mach_read_ull
#    define MACH_WRITE_64 mach_write_ull
#    define OS_MUTEX_CREATE() os_mutex_create(NULL)
#  else
#    define MACH_READ_64 mach_read_from_8
#    define MACH_WRITE_64 mach_write_to_8
#    define OS_MUTEX_CREATE() os_mutex_create()
#  endif
#  define xb_buf_page_is_corrupted(page, zip_size)	\
	buf_page_is_corrupted(page, zip_size)
#  define xb_fil_space_create(name, space_id, zip_size, purpose) \
	fil_space_create(name, space_id, zip_size, purpose)
#  define ut_dulint_zero 0
#  define ut_dulint_cmp(A, B) (A > B ? 1 : (A == B ? 0 : -1))
#  define ut_dulint_add(A, B) (A + B)
#  define ut_dulint_minus(A, B) (A - B)
#  define ut_dulint_align_down(A, B) (A & ~((ib_int64_t)B - 1))
#  define ut_dulint_align_up(A, B) ((A + B - 1) & ~((ib_int64_t)B - 1))
#endif

#ifndef XTRADB_BASED
#define trx_sys_sys_space(id) (id == 0)
#endif

#ifdef __WIN__
#define SRV_PATH_SEPARATOR	'\\'
#define SRV_PATH_SEPARATOR_STR	"\\"
#else
#define SRV_PATH_SEPARATOR	'/'
#define SRV_PATH_SEPARATOR_STR	"/"
#endif

#ifndef UNIV_PAGE_SIZE_MAX
#define UNIV_PAGE_SIZE_MAX UNIV_PAGE_SIZE
#endif
#ifndef UNIV_PAGE_SIZE_SHIFT_MAX
#define UNIV_PAGE_SIZE_SHIFT_MAX UNIV_PAGE_SIZE_SHIFT
#endif

#ifdef __WIN__
#define XB_FILE_UNDEFINED NULL
#else
#define XB_FILE_UNDEFINED (-1)
#endif

#ifdef INNODB_VERSION_SHORT
#define XB_HASH_SEARCH(NAME, TABLE, FOLD, DATA, ASSERTION, TEST) \
	HASH_SEARCH(NAME, TABLE, FOLD, xtrabackup_tables_t*, DATA, ASSERTION, \
		    TEST)
#else
#define XB_HASH_SEARCH(NAME, TABLE, FOLD, DATA, ASSERTION, TEST) \
	HASH_SEARCH(NAME, TABLE, FOLD, DATA, TEST)
#endif

/* ==start === definition at fil0fil.c === */
// ##################################################################
// NOTE: We should check the following definitions fit to the source.
// ##################################################################

#ifndef INNODB_VERSION_SHORT
//5.0 5.1
/* File node of a tablespace or the log data space */
struct fil_node_struct {
	fil_space_t*	space;	/* backpointer to the space where this node
				belongs */
	char*		name;	/* path to the file */
	ibool		open;	/* TRUE if file open */
	os_file_t	handle; /* OS handle to the file, if file open */
	ibool		is_raw_disk;/* TRUE if the 'file' is actually a raw
				device or a raw disk partition */
	ulint		size;	/* size of the file in database pages, 0 if
				not known yet; the possible last incomplete
				megabyte may be ignored if space == 0 */
	ulint		n_pending;
				/* count of pending i/o's on this file;
				closing of the file is not allowed if
				this is > 0 */
	ulint		n_pending_flushes;
				/* count of pending flushes on this file;
				closing of the file is not allowed if
				this is > 0 */
	ib_longlong	modification_counter;/* when we write to the file we
				increment this by one */
	ib_longlong	flush_counter;/* up to what modification_counter value
				we have flushed the modifications to disk */
	UT_LIST_NODE_T(fil_node_t) chain;
				/* link field for the file chain */
	UT_LIST_NODE_T(fil_node_t) LRU;
				/* link field for the LRU list */
	ulint		magic_n;
};

struct fil_space_struct {
	char*		name;	/* space name = the path to the first file in
				it */
	ulint		id;	/* space id */
	ib_longlong	tablespace_version;
				/* in DISCARD/IMPORT this timestamp is used to
				check if we should ignore an insert buffer
				merge request for a page because it actually
				was for the previous incarnation of the
				space */
	ibool		mark;	/* this is set to TRUE at database startup if
				the space corresponds to a table in the InnoDB
				data dictionary; so we can print a warning of
				orphaned tablespaces */
	ibool		stop_ios;/* TRUE if we want to rename the .ibd file of
				tablespace and want to stop temporarily
				posting of new i/o requests on the file */
	ibool		stop_ibuf_merges;
				/* we set this TRUE when we start deleting a
				single-table tablespace */
	ibool		is_being_deleted;
				/* this is set to TRUE when we start
				deleting a single-table tablespace and its
				file; when this flag is set no further i/o
				or flush requests can be placed on this space,
				though there may be such requests still being
				processed on this space */
	ulint		purpose;/* FIL_TABLESPACE, FIL_LOG, or FIL_ARCH_LOG */
	UT_LIST_BASE_NODE_T(fil_node_t) chain;
				/* base node for the file chain */
	ulint		size;	/* space size in pages; 0 if a single-table
				tablespace whose size we do not know yet;
				last incomplete megabytes in data files may be
				ignored if space == 0 */
	ulint		n_reserved_extents;
				/* number of reserved free extents for
				ongoing operations like B-tree page split */
	ulint		n_pending_flushes; /* this is > 0 when flushing
				the tablespace to disk; dropping of the
				tablespace is forbidden if this is > 0 */
	ulint		n_pending_ibuf_merges;/* this is > 0 when merging
				insert buffer entries to a page so that we
				may need to access the ibuf bitmap page in the
				tablespade: dropping of the tablespace is
				forbidden if this is > 0 */
	hash_node_t	hash;	/* hash chain node */
	hash_node_t	name_hash;/* hash chain the name_hash table */
	rw_lock_t	latch;	/* latch protecting the file space storage
				allocation */
	UT_LIST_NODE_T(fil_space_t) unflushed_spaces;
				/* list of spaces with at least one unflushed
				file we have written to */
	ibool		is_in_unflushed_spaces; /* TRUE if this space is
				currently in the list above */
	UT_LIST_NODE_T(fil_space_t) space_list;
				/* list of all spaces */
	ibuf_data_t*	ibuf_data;
				/* insert buffer data */
	ulint		magic_n;
};
typedef struct fil_system_struct	fil_system_t;
struct fil_system_struct {
	mutex_t		mutex;		/* The mutex protecting the cache */
	hash_table_t*	spaces;		/* The hash table of spaces in the
					system; they are hashed on the space
					id */
	hash_table_t*	name_hash;	/* hash table based on the space
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
	ulint		n_open;		/* number of files currently open */
	ulint		max_n_open;	/* n_open is not allowed to exceed
					this */
	ib_longlong	modification_counter;/* when we write to a file we
					increment this by one */
	ulint		max_assigned_id;/* maximum space id in the existing
					tables, or assigned during the time
					mysqld has been up; at an InnoDB
					startup we scan the data dictionary
					and set here the maximum of the
					space id's of the tables there */
	ib_longlong	tablespace_version;
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
/** File node of a tablespace or the log data space */
struct fil_node_struct {
	fil_space_t*	space;	/*!< backpointer to the space where this node
				belongs */
	char*		name;	/*!< path to the file */
	ibool		open;	/*!< TRUE if file open */
	os_file_t	handle;	/*!< OS handle to the file, if file open */
	ibool		is_raw_disk;/*!< TRUE if the 'file' is actually a raw
				device or a raw disk partition */
	ulint		size;	/*!< size of the file in database pages, 0 if
				not known yet; the possible last incomplete
				megabyte may be ignored if space == 0 */
	ulint		n_pending;
				/*!< count of pending i/o's on this file;
				closing of the file is not allowed if
				this is > 0 */
	ulint		n_pending_flushes;
				/*!< count of pending flushes on this file;
				closing of the file is not allowed if
				this is > 0 */
	ib_int64_t	modification_counter;/*!< when we write to the file we
				increment this by one */
	ib_int64_t	flush_counter;/*!< up to what
				modification_counter value we have
				flushed the modifications to disk */
	UT_LIST_NODE_T(fil_node_t) chain;
				/*!< link field for the file chain */
	UT_LIST_NODE_T(fil_node_t) LRU;
				/*!< link field for the LRU list */
	ulint		magic_n;/*!< FIL_NODE_MAGIC_N */
};

struct fil_space_struct {
	char*		name;	/*!< space name = the path to the first file in
				it */
	ulint		id;	/*!< space id */
	ib_int64_t	tablespace_version;
				/*!< in DISCARD/IMPORT this timestamp
				is used to check if we should ignore
				an insert buffer merge request for a
				page because it actually was for the
				previous incarnation of the space */
	ibool		mark;	/*!< this is set to TRUE at database startup if
				the space corresponds to a table in the InnoDB
				data dictionary; so we can print a warning of
				orphaned tablespaces */
	ibool		stop_ios;/*!< TRUE if we want to rename the
				.ibd file of tablespace and want to
				stop temporarily posting of new i/o
				requests on the file */
	ibool		stop_ibuf_merges;
				/*!< we set this TRUE when we start
				deleting a single-table tablespace */
	ibool		is_being_deleted;
				/*!< this is set to TRUE when we start
				deleting a single-table tablespace and its
				file; when this flag is set no further i/o
				or flush requests can be placed on this space,
				though there may be such requests still being
				processed on this space */
	ulint		purpose;/*!< FIL_TABLESPACE, FIL_LOG, or
				FIL_ARCH_LOG */
	UT_LIST_BASE_NODE_T(fil_node_t) chain;
				/*!< base node for the file chain */
	ulint		size;	/*!< space size in pages; 0 if a single-table
				tablespace whose size we do not know yet;
				last incomplete megabytes in data files may be
				ignored if space == 0 */
	ulint		flags;	/*!< compressed page size and file format, or 0
				*/
	ulint		n_reserved_extents;
				/*!< number of reserved free extents for
				ongoing operations like B-tree page split */
	ulint		n_pending_flushes; /*!< this is positive when flushing
				the tablespace to disk; dropping of the
				tablespace is forbidden if this is positive */
	ulint		n_pending_ibuf_merges;/*!< this is positive
				when merging insert buffer entries to
				a page so that we may need to access
				the ibuf bitmap page in the
				tablespade: dropping of the tablespace
				is forbidden if this is positive */
	hash_node_t	hash;	/*!< hash chain node */
	hash_node_t	name_hash;/*!< hash chain the name_hash table */
#ifndef UNIV_HOTBACKUP
	rw_lock_t	latch;	/*!< latch protecting the file space storage
				allocation */
#endif /* !UNIV_HOTBACKUP */
	UT_LIST_NODE_T(fil_space_t) unflushed_spaces;
				/*!< list of spaces with at least one unflushed
				file we have written to */
	ibool		is_in_unflushed_spaces; /*!< TRUE if this space is
				currently in unflushed_spaces */
#ifdef XTRADB_BASED
	ibool		is_corrupt;
#endif
	UT_LIST_NODE_T(fil_space_t) space_list;
				/*!< list of all spaces */
	ulint		magic_n;/*!< FIL_SPACE_MAGIC_N */
};

typedef	struct fil_system_struct	fil_system_t;

struct fil_system_struct {
#ifndef UNIV_HOTBACKUP
	mutex_t		mutex;		/*!< The mutex protecting the cache */
#ifdef XTRADB_BASED
	mutex_t		file_extend_mutex;
#endif
#endif /* !UNIV_HOTBACKUP */
	hash_table_t*	spaces;		/*!< The hash table of spaces in the
					system; they are hashed on the space
					id */
	hash_table_t*	name_hash;	/*!< hash table based on the space
					name */
	UT_LIST_BASE_NODE_T(fil_node_t) LRU;
					/*!< base node for the LRU list of the
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
					/*!< base node for the list of those
					tablespaces whose files contain
					unflushed writes; those spaces have
					at least one file node where
					modification_counter > flush_counter */
	ulint		n_open;		/*!< number of files currently open */
	ulint		max_n_open;	/*!< n_open is not allowed to exceed
					this */
	ib_int64_t	modification_counter;/*!< when we write to a file we
					increment this by one */
	ulint		max_assigned_id;/*!< maximum space id in the existing
					tables, or assigned during the time
					mysqld has been up; at an InnoDB
					startup we scan the data dictionary
					and set here the maximum of the
					space id's of the tables there */
	ib_int64_t	tablespace_version;
					/*!< a counter which is incremented for
					every space object memory creation;
					every space mem object gets a
					'timestamp' from this; in DISCARD/
					IMPORT this is used to check if we
					should ignore an insert buffer merge
					request */
	UT_LIST_BASE_NODE_T(fil_space_t) space_list;
					/*!< list of all file spaces */
	ibool		space_id_reuse_warned;
					/* !< TRUE if fil_space_create()
					has issued a warning about
					potential space_id reuse */
};

#endif /* INNODB_VERSION_SHORT */

extern fil_system_t*   fil_system;
extern char *opt_mysql_tmpdir;
extern MY_TMPDIR mysql_tmpdir_list;

/** Value of fil_space_struct::magic_n */
#define	FIL_SPACE_MAGIC_N	89472

/* ==end=== definition  at fil0fil.c === */

/* prototypes for static functions in original */
#ifndef INNODB_VERSION_SHORT
page_t*
btr_node_ptr_get_child(
/*===================*/
				/* out: child page, x-latched */
	rec_t*		node_ptr,/* in: node pointer */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	mtr_t*		mtr);	/* in: mtr */
#else
buf_block_t*
btr_node_ptr_get_child(
/*===================*/
	const rec_t*	node_ptr,/*!< in: node pointer */
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	mtr_t*		mtr);	/*!< in: mtr */

buf_block_t*
btr_root_block_get(
/*===============*/
	dict_index_t*	index,	/*!< in: index tree */
	mtr_t*		mtr);	/*!< in: mtr */
#endif

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
	ibool	create_new_db,		/* in: TRUE if we should create a
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
#ifdef XTRADB_BASED
	ibool*	create_new_doublewrite_file,
#endif
#ifdef UNIV_LOG_ARCHIVE
	ulint*	min_arch_log_no,/* out: min of archived log numbers in data
				files */
	ulint*	max_arch_log_no,/* out: */
#endif /* UNIV_LOG_ARCHIVE */
	LSN64*	min_flushed_lsn,/* out: min of flushed lsn values in data
				files */
	LSN64*	max_flushed_lsn,/* out: */
	ulint*	sum_of_new_sizes);/* out: sum of sizes of the new files added
				  */

/****************************************************************//**
A simple function to open or create a file.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
os_file_t
xb_file_create_no_error_handling(
/*=============================*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: OS_FILE_OPEN if an existing file
				is opened (if does not exist, error), or
				OS_FILE_CREATE if a new file is created
				(if exists, error) */
	ulint		access_type,/*!< in: OS_FILE_READ_ONLY,
				OS_FILE_READ_WRITE, or
				OS_FILE_READ_ALLOW_DELETE; the last option is
				used by a backup program reading the file */
	ibool*		success);/*!< out: TRUE if succeed, FALSE if error */

/****************************************************************//**
Opens an existing file or creates a new.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
os_file_t
xb_file_create(
/*===========*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: OS_FILE_OPEN if an existing file
				is opened (if does not exist, error), or
				OS_FILE_CREATE if a new file is created
				(if exists, error),
				OS_FILE_OVERWRITE if a new file is created
				or an old overwritten;
				OS_FILE_OPEN_RAW, if a raw device or disk
				partition should be opened */
	ulint		purpose,/*!< in: OS_FILE_AIO, if asynchronous,
				non-buffered i/o is desired,
				OS_FILE_NORMAL, if any normal file;
				NOTE that it also depends on type, os_aio_..
				and srv_.. variables whether we really use
				async i/o or unbuffered i/o: look in the
				function source code for the exact rules */
	ulint		type,	/*!< in: OS_DATA_FILE or OS_LOG_FILE */
	ibool*		success);/*!< out: TRUE if succeed, FALSE if error */


/***********************************************************************//**
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@return	TRUE if success */
ibool
xb_file_rename(
/*===========*/
	const char*	oldpath,/*!< in: old file path as a null-terminated
				string */
	const char*	newpath);/*!< in: new file path */

void
xb_file_set_nocache(
/*================*/
	os_file_t	fd,		/* in: file descriptor to alter */
	const char*	file_name,	/* in: used in the diagnostic message
					*/
	const char*	operation_name);/* in: used in the diagnostic message,
					we call os_file_set_nocache()
					immediately after opening or creating
					a file, so this is either "open" or
					"create" */

/***********************************************************************//**
Compatibility wrapper around os_file_flush().
@return	TRUE if success */
ibool
xb_file_flush(
/*==========*/
	os_file_t	file);	/*!< in, own: handle to a file */

/*******************************************************************//**
Returns the table space by a given id, NULL if not found. */
fil_space_t*
xb_space_get_by_id(
/*================*/
	ulint	id);	/*!< in: space id */

/*******************************************************************//**
Returns the table space by a given name, NULL if not found. */
fil_space_t*
xb_space_get_by_name(
/*==================*/
	const char*	name);	/*!< in: space name */

/****************************************************************//**
Create a new tablespace on disk and return the handle to its opened
file. Code adopted from fil_create_new_single_table_tablespace with
the main difference that only disk file is created without updating
the InnoDB in-memory dictionary data structures.

@return TRUE on success, FALSE on error.  */
ibool
xb_space_create_file(
/*==================*/
	const char*	path,		/*!<in: path to tablespace */
	ulint		space_id,	/*!<in: space id */
	ulint		flags __attribute__((unused)),/*!<in: tablespace
						      flags */
	os_file_t*	file);		/*!<out: file handle */

#ifndef INNODB_VERSION_SHORT

/********************************************************************//**
Extract the compressed page size from table flags.
@return	compressed page size, or 0 if not compressed */
ulint
dict_table_flags_to_zip_size(
/*=========================*/
	ulint	flags);	/*!< in: flags */


/*******************************************************************//**
Free all spaces in space_list. */
void
fil_free_all_spaces(void);
/*=====================*/

#endif

void
innobase_mysql_prepare_print_arbitrary_thd(void);

void
innobase_mysql_end_print_arbitrary_thd(void);

int
mysql_get_identifier_quote_char(
	trx_t*		trx,
	const char*	name,
	ulint		namelen);

void
innobase_print_identifier(
	FILE*	f,
	trx_t*	trx __attribute__((unused)),
	ibool	table_id __attribute__((unused)),
	const char*	name,
	ulint	namelen);

/**********************************************************************//**
It should be safe to use lower_case_table_names=0 for xtrabackup. If it causes
any problems, we can add the lower_case_table_names option to xtrabackup
later.
@return	0 */
ulint
innobase_get_lower_case_table_names(void);

/******************************************************************//**
Strip dir name from a full path name and return only the file name
@return file name or "null" if no file name */
const char*
innobase_basename(
/*==============*/
	const char*	path_name);	/*!< in: full path name */

int
innobase_mysql_cmp(
	int		mysql_type,
	uint		charset_number,
	unsigned char*	a,
	unsigned int	a_length,
	unsigned char*	b,
	unsigned int	b_length);

ibool
innobase_query_is_update(void);

#endif /* INNODB_INT_H */
