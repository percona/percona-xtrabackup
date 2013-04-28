/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009-2012 Percona Ireland Ltd.
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

#include <mysql_version.h>
#include <my_base.h>

/* Only include InnoDB headers here, please keep the list sorted */

#if MYSQL_VERSION_ID < 50600
extern "C" {
#endif

#include <univ.i>

#include <btr0btr.h>
#include <btr0cur.h>
#include <btr0pcur.h>
#include <btr0sea.h>
#include <buf0buf.h>
#include <buf0flu.h>
#include <buf0lru.h>
#include <dict0crea.h>
#include <dict0dict.h>
#include <dict0load.h>
#if MYSQL_VERSION_ID >= 50600
#include <dict0priv.h>
#include <dict0stats.h>
#endif
#include <fil0fil.h>
#include <fsp0fsp.h>
#include <fsp0types.h>
#include <ha_prototypes.h>
#if MYSQL_VERSION_ID >= 50600
#include <handler0alter.h>
#endif
#include <hash0hash.h>
#include <ibuf0ibuf.h>
#include <lock0lock.h>
#include <log0log.h>
#include <log0recv.h>
#include <mtr0mtr.h>
#include <os0file.h>
#include <os0sync.h>
#include <os0thread.h>
#include <page0page.h>
#include <page0zip.h>
#include <row0ins.h>
#include <row0merge.h>
#include <row0mysql.h>
#include <row0sel.h>
#include <row0types.h>
#include <row0upd.h>
#include <srv0srv.h>
#include <srv0start.h>
#include <sync0sync.h>
#include <trx0roll.h>
#include <trx0sys.h>
#include <trx0trx.h>
#include <trx0xa.h>
#if MYSQL_VERSION_ID >= 50600
#include <ut0crc32.h>
#endif
#include <ut0mem.h>
#include <ut0rbt.h>

#if MYSQL_VERSION_ID < 50600
}
#endif

#  if (MYSQL_VERSION_ID < 50500)
     /* MySQL and Percona Server 5.1 */
#    define MACH_READ_64 mach_read_ull
#    define MACH_WRITE_64 mach_write_ull
#    define INDEX_ID_T dulint
#    define INDEX_ID_CMP(a,b) ((a).high != (b).high ?  \
			       ((a).high - (b).high) : \
			       ((a).low - (b).low))
#    define OS_MUTEX_CREATE() os_mutex_create(NULL)
#    define xb_os_file_write(name, file, buf, offset, n)		\
	os_file_write(name, file, buf,					\
		      (ulint)(offset & 0xFFFFFFFFUL),			\
		      (ulint)(((ullint)offset) >> 32),			\
		      n)
#    define xb_os_file_read(file, buf, offset, n)			\
	os_file_read(file, buf,						\
		     (ulint)(offset & 0xFFFFFFFFUL),			\
		     (ulint)(((ullint)offset) >> 32),			\
		     n)
#    define xb_trx_id_to_index_trx_id(trx_id) \
	((ib_uint64_t) ut_conv_dulint_to_longlong(trx_id))
#  else /* MYSQL_VERSION_ID < 50500 */
     /* MySQL and Percona Server 5.5+ */
#    define MACH_READ_64 mach_read_from_8
#    define MACH_WRITE_64 mach_write_to_8
#    define INDEX_ID_T index_id_t
#    define INDEX_ID_CMP(a,b) ((a) - (b))
#    define OS_MUTEX_CREATE() os_mutex_create()
#    define xb_trx_id_to_index_trx_id(trx_id) (trx_id)
#  endif /* MYSQL_VERSION_ID < 50500 */
#  if (MYSQL_VERSION_ID >= 50600)
   /* MySQL 5.6+ */
#  define PAGE_ZIP_MIN_SIZE_SHIFT	10
#  define DICT_TF_ZSSIZE_SHIFT	1
#  define DICT_TF_FORMAT_ZIP	1
#  define DICT_TF_FORMAT_SHIFT		5
#  define xb_os_file_read(file, buf, offset, n)				\
	pfs_os_file_read_func(file, buf, offset, n, __FILE__, __LINE__)
#  define xb_os_file_write(name, file, buf, offset, n)	\
	pfs_os_file_write_func(name, file, buf, offset,	\
			       n, __FILE__, __LINE__)
#  define xb_buf_page_is_corrupted(page, zip_size)	\
	buf_page_is_corrupted(TRUE, page, zip_size)
#  define xb_btr_pcur_open_at_index_side(from_left, index, latch_mode, pcur, \
				       init_pcur, level, mtr)		\
	btr_pcur_open_at_index_side(from_left, index, latch_mode, pcur,	\
				    init_pcur, 0, mtr)
#  define xb_os_file_set_size(name, file, size)	\
	os_file_set_size(name, file, size)
#  define xb_fil_rename_tablespace(old_name_in, id, new_name, new_path)	\
	fil_rename_tablespace(old_name_in, id, new_name, new_path)
#  define xb_btr_root_block_get(index, mode, mtr) \
	btr_root_block_get(index, mode, mtr)
   typedef ib_mutex_t	mutex_t;
#  define INNODB_LOG_DIR srv_log_group_home_dir
#  define DEFAULT_LOG_FILE_SIZE 48*1024*1024
   /* InnoDB data dictionary API in MySQL 5.5- works on on tables named
   "./database/table.ibd", and in 5.6+ on "database/table".  This variable
   handles truncating or leaving the final ".ibd".  */
#  define XB_DICT_SUFFIX_LEN 4
#  define xb_os_event_create(name) os_event_create()
#else /* MYSQL_VERSION_ID >= 50600 */
   /* MySQL and Percona Server 5.1 and 5.5 */
#  define xb_buf_page_is_corrupted(page, zip_size)	\
	buf_page_is_corrupted(page, zip_size)
#  define xb_os_event_create(name) os_event_create(name)
#  define XB_DICT_SUFFIX_LEN 0
#  define INT64PF	"%lld"
#  define UINT64PF "%llu"
#  define LSN_PF UINT64PF
#  define DEFAULT_LOG_FILE_SIZE 5*1024*1024
   typedef ulint		dberr_t;
   typedef os_mutex_t		os_ib_mutex_t;
   typedef ib_uint64_t		lsn_t;
   typedef merge_index_def_t	index_def_t;
   typedef merge_index_field_t	index_field_t;
   typedef struct ib_rbt_struct ib_rbt_t;
#  define xb_btr_pcur_open_at_index_side(from_left, index, latch_mode, pcur, \
				       init_pcur, level, mtr)		\
	btr_pcur_open_at_index_side(from_left, index, latch_mode, pcur,	\
				    init_pcur, mtr)
#  define xb_fil_rename_tablespace(old_name_in, id, new_name, new_path)	\
	fil_rename_tablespace(old_name_in, id, new_name)
#  define xb_btr_root_block_get(index, mode, mtr) \
	btr_root_block_get(index, mtr)
#  define UNIV_FORMAT_MAX DICT_TF_FORMAT_51;
#  define dict_tf_get_zip_size dict_table_flags_to_zip_size
#  define os_file_get_size(file) os_file_get_size_as_iblonglong(file)
#  define xb_os_file_set_size(name, file, size)				\
	os_file_set_size(name, file, (ulong)(size & 0xFFFFFFFFUL),	\
			 (ulong)((ullint)size >> 32))
#  define fsp_flags_is_compressed(flags) ((flags) & DICT_TF_ZSSIZE_MASK)
#  define fsp_flags_get_zip_size(flags)					\
	((PAGE_ZIP_MIN_SIZE >> 1) << (((flags) & DICT_TF_ZSSIZE_MASK)	\
				      >> DICT_TF_ZSSIZE_SHIFT))

#  define trx_start_for_ddl(trx, op)
#  define ut_crc32_init()

#  define LOG_CHECKPOINT_OFFSET_LOW32	LOG_CHECKPOINT_OFFSET
#  define INNODB_LOG_DIR innobase_log_group_home_dir
#  ifndef XTRADB_BASED
     /* MySQL 5.1 and 5.5 */
#    define dict_stats_update_transient(table)	\
	dict_update_statistics(table, TRUE)
#  else /* XTRADB_BASED */
     /* Percona Server 5.1 and 5.5 */
#    define dict_stats_update_transient(table)	\
    dict_update_statistics(table, TRUE, FALSE)
#  endif /* XTRADB_BASED */
#endif /* MYSQL_VERSION_ID >= 50600 */
#  define xb_fil_space_create(name, space_id, zip_size, purpose) \
	fil_space_create(name, space_id, zip_size, purpose)
#  define ut_dulint_zero 0
#  define ut_dulint_cmp(A, B) (A > B ? 1 : (A == B ? 0 : -1))
#  define ut_dulint_add(A, B) (A + B)
#  define ut_dulint_minus(A, B) (A - B)
#  define ut_dulint_align_down(A, B) (A & ~((ib_int64_t)B - 1))
#  define ut_dulint_align_up(A, B) ((A + B - 1) & ~((ib_int64_t)B - 1))

#ifndef XTRADB_BASED
# if MYSQL_VERSION_ID >= 50600
#  define trx_sys_sys_space(id) (!fil_is_user_tablespace_id(id))
# else
#  define trx_sys_sys_space(id) (id == 0)
# endif
#endif /* XTRADB_BASED */

#if (MYSQL_VERSION_ID >= 50500) && (MYSQL_VERSION_ID < 50600)
/* MySQL and Percona Server 5.5 */
#define xb_os_file_write(name, file, buf, offset, n)			\
	pfs_os_file_write_func(name, file, buf,				\
			       (ulint)(offset & 0xFFFFFFFFUL),		\
			       (ulint)(((ullint)offset) >> 32),		\
			       n, __FILE__, __LINE__)
#ifdef XTRADB_BASED
/* Percona Server 5.5 only */
#define xb_os_file_read(file, buf, offset, n)			\
	pfs_os_file_read_func(file, buf,				\
			      (ulint)(offset & 0xFFFFFFFFUL),		\
			      (ulint)(((ullint)offset) >> 32),		\
			      n,  NULL, __FILE__, __LINE__)
#else /* XTRADB_BASED */
/* MySQL 5.5 only */
#define xb_os_file_read(file, buf, offset, n)			\
	pfs_os_file_read_func(file, buf,			\
			      (ulint)(offset & 0xFFFFFFFFUL),	\
			      (ulint)(((ullint)offset) >> 32),	\
			      n,  __FILE__, __LINE__)
#endif /* XTRADB_BASED */
#endif /* (MYSQL_VERSION_ID >= 50500) && (MYSQL_VERSION_ID < 50600) */

#ifdef __WIN__
#define SRV_PATH_SEPARATOR	'\\'
#else
#define SRV_PATH_SEPARATOR	'/'
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

#define XB_HASH_SEARCH(NAME, TABLE, FOLD, DATA, ASSERTION, TEST) \
	HASH_SEARCH(NAME, TABLE, FOLD, xtrabackup_tables_t*, DATA, ASSERTION, \
		    TEST)

/* The following constants have been copied from fsp0fsp.c */
#define FSP_HEADER_OFFSET	FIL_PAGE_DATA	/* Offset of the space header
						within a file page */
#define	FSP_SIZE		8	/* Current size of the space in
					pages */
#define	FSP_FREE_LIMIT		12	/* Minimum page number for which the
					free list has not been initialized:
					the pages >= this limit are, by
					definition, free; note that in a
					single-table tablespace where size
					< 64 pages, this number is 64, i.e.,
					we have initialized the space
					about the first extent, but have not
					physically allocted those pages to the
					file */

/* ==start === definition at fil0fil.c === */
// ##################################################################
// NOTE: We should check the following definitions fit to the source.
// ##################################################################

#if MYSQL_VERSION_ID < 50600
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

#else /* MYSQL_VERSION_ID < 50600 */

/** Tablespace or log data space: let us call them by a common name space */
struct fil_space_t {
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
	ibool		stop_new_ops;
				/*!< we set this TRUE when we start
				deleting a single-table tablespace.
				When this is set following new ops
				are not allowed:
				* read IO request
				* ibuf merge
				* file flush
				Note that we can still possibly have
				new write operations because we don't
				check this flag when doing flush
				batches. */
	ulint		purpose;/*!< FIL_TABLESPACE, FIL_LOG, or
				FIL_ARCH_LOG */
	UT_LIST_BASE_NODE_T(fil_node_t) chain;
	/*!< base node for the file chain */
	ulint		size;	/*!< space size in pages; 0 if a single-table
				tablespace whose size we do not know yet;
				last incomplete megabytes in data files may be
				ignored if space == 0 */
	ulint		flags;	/*!< tablespace flags; see
				fsp_flags_is_valid(),
				fsp_flags_get_zip_size() */
	ulint		n_reserved_extents;
				/*!< number of reserved free extents for
				ongoing operations like B-tree page split */
	ulint		n_pending_flushes; /*!< this is positive when flushing
				the tablespace to disk; dropping of the
				tablespace is forbidden if this is positive */
	ulint		n_pending_ops;/*!< this is positive when we
				have pending operations against this
				tablespace. The pending operations can
				be ibuf merges or lock validation code
				trying to read a block.
				Dropping of the tablespace is forbidden
				if this is positive */
	hash_node_t	hash;	/*!< hash chain node */
	hash_node_t	name_hash;/*!< hash chain the name_hash table */
#ifndef UNIV_HOTBACKUP
	rw_lock_t	latch;	/*!< latch protecting the file space storage
				  allocation */
#endif /* !UNIV_HOTBACKUP */
	UT_LIST_NODE_T(fil_space_t) unflushed_spaces;
				/*!< list of spaces with at least one unflushed
				file we have written to */
	bool		is_in_unflushed_spaces;
				/*!< true if this space is currently in
				unflushed_spaces */
	UT_LIST_NODE_T(fil_space_t) space_list;
				/*!< list of all spaces */
	ulint		magic_n;/*!< FIL_SPACE_MAGIC_N */
};

/** The tablespace memory cache; also the totality of logs (the log
    data space) is stored here; below we talk about tablespaces, but also
    the ib_logfiles form a 'space' and it is handled here */
struct fil_system_t {
#ifndef UNIV_HOTBACKUP
	ib_mutex_t		mutex;		/*!< The mutex protecting the cache */
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

/** File node of a tablespace or the log data space */
struct fil_node_t {
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
	ibool		being_extended;
				/*!< TRUE if the node is currently
				being extended. */
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

#endif /* MYSQL_VERSION_ID < 50600 */

extern fil_system_t*   fil_system;
extern char *opt_mysql_tmpdir;

/* InnoDB data dictionary API in MySQL 5.5- works on on tables named
"./database/table.ibd", and in 5.6+ on "database/table".  This variable
handles the presence or absence of "./".  */
extern const char* xb_dict_prefix;

#if MYSQL_VERSION_ID < 50600
extern char*	innobase_log_group_home_dir;
#endif

extern long innobase_mirrored_log_groups;

/** Value of fil_space_struct::magic_n */
#define	FIL_SPACE_MAGIC_N	89472

/* ==end=== definition  at fil0fil.c === */

#if MYSQL_VERSION_ID >= 50500
struct TABLE;
#endif

/* prototypes for static and non-prototyped functions in original */

#if MYSQL_VERSION_ID < 50600
extern "C" {
#endif

#if MYSQL_VERSION_ID >= 50600

buf_block_t*
btr_root_block_get(
/*===============*/
	const dict_index_t*	index,	/*!< in: index tree */
	ulint			mode,	/*!< in: either RW_S_LATCH
					or RW_X_LATCH */
	mtr_t*			mtr);	/*!< in: mtr */

int
fil_file_readdir_next_file(
/*=======================*/
	dberr_t*	err,	/*!< out: this is set to DB_ERROR if an error
				was encountered, otherwise not changed */
	const char*	dirname,/*!< in: directory name or path */
	os_file_dir_t	dir,	/*!< in: directory stream */
	os_file_stat_t*	info);	/*!< in/out: buffer where the
				info is returned */

ibool
recv_check_cp_is_consistent(
/*========================*/
	const byte*	buf);	/*!< in: buffer containing checkpoint info */

dberr_t
open_or_create_data_files(
/*======================*/
	ibool*		create_new_db,	/*!< out: TRUE if new database should be
					created */
	lsn_t*		min_flushed_lsn,/*!< out: min of flushed lsn
					values in data files */
	lsn_t*		max_flushed_lsn,/*!< out: max of flushed lsn
					values in data files */
	ulint*		sum_of_new_sizes);/*!< out: sum of sizes of the
					new files added */

ibool
log_block_checksum_is_ok_or_old_format(
/*===================================*/
	const byte*     block); /*!< in: pointer to a log block */

#else /* MYSQL_VERSION_ID >= 50600 */

buf_block_t*
btr_root_block_get(
/*===============*/
	dict_index_t*	index,	/*!< in: index tree */
	mtr_t*		mtr);	/*!< in: mtr */

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
open_or_create_data_files(
/*======================*/
				/* out: DB_SUCCESS or error code */
	ibool*	create_new_db,	/* out: TRUE if new database should be
				created */
#ifdef XTRADB_BASED
	ibool*	create_new_doublewrite_file,
#endif
	lsn_t*	min_flushed_lsn,/* out: min of flushed lsn values in data
				files */
	lsn_t*	max_flushed_lsn,/* out: */
	ulint*	sum_of_new_sizes);/* out: sum of sizes of the new files added
				*/

ibool
log_block_checksum_is_ok_or_old_format(
/*===================================*/
			/* out: TRUE if ok, or if the log block may be in the
			format of InnoDB version < 3.23.52 */
	byte*	block);	/* in: pointer to a log block */

#endif /* MYSQL_VERSION_ID >= 50600 */

buf_block_t*
btr_node_ptr_get_child(
/*===================*/
	const rec_t*	node_ptr,/*!< in: node pointer */
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	mtr_t*		mtr);	/*!< in: mtr */

ulint
recv_find_max_checkpoint(
/*=====================*/
					/* out: error code or DB_SUCCESS */
	log_group_t**	max_group,	/* out: max group */
	ulint*		max_field);	/* out: LOG_CHECKPOINT_1 or
					LOG_CHECKPOINT_2 */

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

void
os_file_set_nocache(
/*================*/
	int		fd,		/* in: file descriptor to alter */
	const char*	file_name,	/* in: used in the diagnostic message */
	const char*	operation_name);/* in: used in the diagnostic message,
					we call os_file_set_nocache()
					immediately after opening or creating
					a file, so this is either "open" or
					"create" */

#if MYSQL_VERSION_ID < 50600
} /* extern "C" */
#endif

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

/*********************************************************************//**
Normalizes init parameter values to use units we use inside InnoDB.
@return	DB_SUCCESS or error code */
void
xb_normalize_init_values(void);
/*==========================*/

#if MYSQL_VERSION_ID < 50600
extern "C" {
#endif

#if MYSQL_VERSION_ID >= 50500

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

#endif

int
innobase_mysql_cmp(
	int		mysql_type,
	uint		charset_number,
	unsigned char*	a,
	unsigned int	a_length,
	unsigned char*	b,
	unsigned int	b_length);

void
innobase_get_cset_width(
			ulint	cset,
			ulint*	mbminlen,
			ulint*	mbmaxlen);

#ifdef XTRADB_BASED

trx_t*
innobase_get_trx();

ibool
innobase_get_slow_log();

#endif

#if MYSQL_VERSION_ID < 50600

void
innobase_invalidate_query_cache(
	trx_t*	trx,
	const char*	full_name,
	ulint	full_name_len);

#endif /* MYSQL_VERSION_ID < 50600 */

/*****************************************************************//**
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return	pointer to the end of buf */
char*
innobase_convert_name(
/*==================*/
	char*		buf,	/*!< out: buffer for converted identifier */
	ulint		buflen,	/*!< in: length of buf, in bytes */
	const char*	id,	/*!< in: identifier to convert */
	ulint		idlen,	/*!< in: length of id, in bytes */
	void*		thd,	/*!< in: MySQL connection thread, or NULL */
	ibool		table_id);/*!< in: TRUE=id is a table or database name;
				FALSE=id is an index name */

ibool
trx_is_interrupted(
	trx_t*	trx);

ulint
innobase_get_at_most_n_mbchars(
	ulint		charset_id,
	ulint		prefix_len,
	ulint		data_len,
	const char*	str);

ulint
innobase_raw_format(
/*================*/
	const char*	data,		/*!< in: raw data */
	ulint		data_len,	/*!< in: raw data length
					in bytes */
	ulint		charset_coll,	/*!< in: charset collation */
	char*		buf,		/*!< out: output buffer */
	ulint		buf_size);	/*!< in: output buffer size
					in bytes */

ulong
thd_lock_wait_timeout(
/*==================*/
	void*	thd);	/*!< in: thread handle (THD*), or NULL to query
			the global innodb_lock_wait_timeout */

ibool
thd_supports_xa(
/*============*/
	void*	thd);	/*!< in: thread handle (THD*), or NULL to query
			the global innodb_supports_xa */

ibool
trx_is_strict(
/*==========*/
	trx_t*	trx);	/*!< in: transaction */

void
innobase_rec_reset(
/*===============*/
	TABLE*	table);

void
innobase_rec_to_mysql(
/*==================*/
	TABLE*			table,
	const rec_t*		rec,
	const dict_index_t*	index,
	const ulint*		offsets);

#if MYSQL_VERSION_ID < 50600
} /* extern "C" */
#endif

#if MYSQL_VERSION_ID >= 50600

extern os_file_t	files[1000];

/*********************************************************************//**
Creates or opens the log files and closes them.
@return	DB_SUCCESS or error code */
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
	ulint	i);			/*!< in: log file number in group */

#endif

/*****************************************************************//**
Parse, validate, and set the InnoDB variables from
innodb_log_group_home_dir and innodb_mirrored_log_group_groups
options.

@return TRUE if server variables set OK, FALSE otherwise */
ibool
xb_parse_log_group_home_dirs(void);
/*===============================*/

/*****************************************************************//**
Set InnoDB to read-only mode. */
void
xb_set_innodb_read_only(void);
/*==========================*/

/*****************************************************************//**
Adjust srv_fatal_semaphore_wait_threshold in a thread-safe manner.  */
void
xb_adjust_fatal_semaphore_wait_threshold(
/*=====================================*/
	ulint delta);

/*****************************************************************//**
Wrapper around around recv_check_cp_is_consistent() for handling
version differences.

@return TRUE if checkpoint info in the buffer is OK */
ibool
xb_recv_check_cp_is_consistent(
/*===========================*/
	const byte*	buf);	/*!<in: buffer containing checkpoint info */

/*****************************************************************//**
Initialize a index_field_t variable from a dict_field_t */
void
xb_dict_index_field_to_index_field(
/*===============================*/
	 mem_heap_t*		heap,			/*!< in: heap for
							field name string
							allocation on 5.5 or
							earlier version. */
	 const dict_field_t*	dict_index_field,	/*!< in: field */
	 index_field_t*		index_field);		/*!< out: field */

/**********************************************************//**
Waits for an event object until it is in the signaled state or
a timeout is exceeded.
@return 0 if success, OS_SYNC_TIME_EXCEEDED if timeout was exceeded */
ulint
xb_event_wait_time(
/*===============*/
	os_event_t	event,			/*!< in: event to wait */
	ulint		time_in_usec);		/*!< in: timeout in
						microseconds, or
						OS_SYNC_INFINITE_TIME */

#endif /* INNODB_INT_H */
