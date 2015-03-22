/******************************************************
hot backup tool for InnoDB
(c) 2009-2015 Percona LLC and/or its affiliates
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

#include <my_global.h>
#include <stdio.h>
#include <string.h>
#include <mysql.h>
#include <my_dir.h>
#include <ut0mem.h>
#include <os0sync.h>
#include <os0file.h>
#include <fil0fil.h>
#include <srv0start.h>
#include <algorithm>
#include <mysqld.h>
#include <my_default.h>
#include <my_getopt.h>
#include <strings.h>
#include <fnmatch.h>
#include <string>
#include <sstream>
#include <set>
#include <version_check_pl.h>
#include "common.h"
#include "innobackupex.h"
#include "xtrabackup.h"
#include "xtrabackup_version.h"
#include "xbstream.h"
#include "fil_cur.h"
#include "write_filt.h"

extern "C" {
MYSQL * STDCALL
cli_mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		       const char *passwd, const char *db,
		       uint port, const char *unix_socket,ulong client_flag);
}

#define mysql_real_connect cli_mysql_real_connect

using std::min;
using std::max;

/* special files */
#define XTRABACKUP_SLAVE_INFO "xtrabackup_slave_info"
#define XTRABACKUP_GALERA_INFO "xtrabackup_galera_info"
#define XTRABACKUP_BINLOG_INFO "xtrabackup_binlog_info"
#define XTRABACKUP_INFO "xtrabackup_info"

/* list of files to sync for --rsync mode */
std::set<std::string> rsync_list;

/** connection to mysql server */
static MYSQL *mysql_connection;

/* server capabilities */
static bool have_changed_page_bitmaps = false;
static bool have_backup_locks = false;
static bool have_lock_wait_timeout = false;
static bool have_galera_enabled = false;
static bool have_flush_engine_logs = false;
static bool have_multi_threaded_slave = false;
static bool have_gtid_slave = false;

/* options */
my_bool opt_ibx_version = FALSE;
my_bool opt_ibx_help = FALSE;
my_bool opt_ibx_apply_log = FALSE;
my_bool opt_ibx_copy_back = FALSE;
my_bool opt_ibx_move_back = FALSE;
my_bool opt_ibx_redo_only = FALSE;
my_bool opt_ibx_galera_info = FALSE;
my_bool opt_ibx_slave_info = FALSE;
my_bool opt_ibx_incremental = FALSE;
my_bool opt_ibx_no_lock = FALSE;
my_bool opt_ibx_safe_slave_backup = FALSE;
my_bool opt_ibx_rsync = FALSE;
my_bool opt_ibx_force_non_empty_dirs = FALSE;
my_bool opt_ibx_notimestamp = FALSE;
my_bool opt_ibx_noversioncheck = FALSE;
my_bool opt_ibx_no_backup_locks = FALSE;
my_bool opt_ibx_decompress = FALSE;

char *opt_ibx_incremental_history_name = NULL;
char *opt_ibx_incremental_history_uuid = NULL;

char *opt_ibx_user = NULL;
char *opt_ibx_password = NULL;
char *opt_ibx_host = NULL;
char *opt_ibx_defaults_group = NULL;
char *opt_ibx_socket = NULL;
uint opt_ibx_port = 0;
char *opt_login_path = NULL;

const char *ibx_query_type_names[] = { "ALL", "UPDATE", "SELECT", NullS};

enum ibx_query_type_t {IBX_QUERY_TYPE_ALL, IBX_QUERY_TYPE_UPDATE,
			IBX_QUERY_TYPE_SELECT};

TYPELIB ibx_query_type_typelib= {array_elements(ibx_query_type_names) - 1, "",
	ibx_query_type_names, NULL};

ulong opt_ibx_lock_wait_query_type;
ulong opt_ibx_kill_long_query_type;

ulong opt_ibx_decrypt_algo = 0;

uint opt_ibx_kill_long_queries_timeout = 0;
uint opt_ibx_lock_wait_timeout = 0;
uint opt_ibx_lock_wait_threshold = 0;
uint opt_ibx_debug_sleep_before_unlock = 0;
uint opt_ibx_safe_slave_backup_timeout = 0;

const char *opt_ibx_history = NULL;
char *opt_ibx_include = NULL;
char *opt_ibx_databases = NULL;
bool ibx_partial_backup = false;
bool opt_ibx_decrypt = false;

char *ibx_position_arg = NULL;
char *ibx_backup_directory = NULL;

/* Kill long selects */
os_thread_id_t	kill_query_thread_id;
os_event_t	kill_query_thread_started;
os_event_t	kill_query_thread_stopped;
os_event_t	kill_query_thread_stop;

bool sql_thread_started = false;
char *mysql_slave_position = NULL;
char *mysql_binlog_position = NULL;
char *buffer_pool_filename = NULL;

/* History on server */
time_t history_start_time;
time_t history_end_time;
time_t history_lock_time;

char *tool_name;
char tool_args[2048];

char *innobase_data_file_path_alloc = NULL;

/* copy of proxied xtrabackup options */
my_bool ibx_xb_close_files;
my_bool	ibx_xtrabackup_compact;
const char *ibx_xtrabackup_compress_alg;
uint ibx_xtrabackup_compress_threads;
ulonglong ibx_xtrabackup_compress_chunk_size;
ulong ibx_xtrabackup_encrypt_algo;
char *ibx_xtrabackup_encrypt_key;
char *ibx_xtrabackup_encrypt_key_file;
uint ibx_xtrabackup_encrypt_threads;
ulonglong ibx_xtrabackup_encrypt_chunk_size;
my_bool ibx_xtrabackup_export;
char *ibx_xtrabackup_extra_lsndir;
char *ibx_xtrabackup_incremental_basedir;
char *ibx_xtrabackup_incremental_dir;
my_bool	ibx_xtrabackup_incremental_force_scan;
ulint ibx_xtrabackup_log_copy_interval;
char *ibx_xtrabackup_incremental;
int ibx_xtrabackup_parallel;
my_bool ibx_xtrabackup_rebuild_indexes;
ulint ibx_xtrabackup_rebuild_threads;
char *ibx_xtrabackup_stream_str;
char *ibx_xtrabackup_tables_file;
long ibx_xtrabackup_throttle;
char *ibx_opt_mysql_tmpdir;
longlong ibx_xtrabackup_use_memory;


static inline int ibx_msg(const char *fmt, ...) ATTRIBUTE_FORMAT(printf, 1, 2);
static inline int ibx_msg(const char *fmt, ...)
{
	int	result;
	time_t	t = time(NULL);
	char	date[100];
	char	*line;
	va_list args;

	strftime(date, sizeof(date), "%y%m%d %H:%M:%S", localtime(&t));

	va_start(args, fmt);

	result = vasprintf(&line, fmt, args);

	va_end(args);

	if (result != -1) {
		result = fprintf(stderr, "%s %s: %s",
					date, INNOBACKUPEX_BIN_NAME, line);
		free(line);
	}

	return result;
}

/************************************************************************
Retirn true if character if file separator */
static
bool
is_path_separator(char c)
{
	return(c == FN_LIBCHAR || c == FN_LIBCHAR2);
}

/************************************************************************
Check to see if a file exists.
Takes name of the file to check.
@return true if file exists. */
static
bool
file_exists(const char *filename)
{
	MY_STAT stat_arg;

	if (!my_stat(filename, &stat_arg, MYF(0))) {

		return(false);
	}

	return(true);
}

/************************************************************************
Trim leading slashes from absolute path so it becomes relative */
static
const char *
trim_dotslash(const char *path)
{
	while (*path) {
		if (is_path_separator(*path)) {
			++path;
			continue;
		}
		if (*path == '.' && is_path_separator(path[1])) {
			path += 2;
			continue;
		}
		break;
	}

	return(path);
}

/************************************************************************
Return pointer to a file name part of a given path */
static
const char *
get_filename(const char *path)
{
	const char *ptr;

	for (ptr = path; *ptr; ++ptr) {
		if (is_path_separator(*ptr)) {
			path = ptr + 1;
		}
	}

	return(path);
}



/************************************************************************
Check if string ends with given suffix.
@return true if string ends with given suffix. */
static
bool
ends_with(const char *str, const char *suffix)
{
	size_t suffix_len = strlen(suffix);
	size_t str_len = strlen(str);
	return(str_len >= suffix_len
	       && strcmp(str + str_len - suffix_len, suffix) == 0);
}

/************************************************************************
Create directories recursively.
@return 0 if directories created successfully. */
static
int
mkdirp(const char *pathname, int Flags, myf MyFlags)
{
	char parent[PATH_MAX], *p;

	/* make a parent directory path */
	strncpy(parent, pathname, sizeof(parent));
	parent[sizeof(parent) - 1] = 0;

	for (p = parent + strlen(parent);
	    !is_path_separator(*p) && p != parent; p--);

	*p = 0;

	/* try to make parent directory */
	if (p != parent && mkdirp(parent, Flags, MyFlags) != 0) {
		return(-1);
	}

	/* make this one if parent has been made */
	if (my_mkdir(pathname, Flags, MyFlags) == 0) {
		return(0);
	}

	/* if it already exists that is fine */
	if (errno == EEXIST) {
		return(0);
	}

	return(-1);
}

/************************************************************************
Return true if first and second arguments are the same path. */
static
bool
equal_paths(const char *first, const char *second)
{
	char real_first[PATH_MAX];
	char real_second[PATH_MAX];

	ut_a(realpath(first, real_first) != NULL);
	ut_a(realpath(second, real_second) != NULL);

	return (strcmp(real_first, real_second) == 0);
}

/************************************************************************
Struct represents file or directory. */
struct datadir_node_t {
	ulint		dbpath_len;
	char		*filepath;
	ulint		filepath_len;
	char		*filepath_rel;
	ulint		filepath_rel_len;
	bool		is_empty_dir;
	bool		is_file;
};

/************************************************************************
Holds the state needed to enumerate files in MySQL data directory. */
struct datadir_iter_t {
	char		*datadir_path;
	char		*dbpath;
	ulint		dbpath_len;
	char		*filepath;
	ulint		filepath_len;
	char		*filepath_rel;
	ulint		filepath_rel_len;
	os_ib_mutex_t	mutex;
	os_file_dir_t	dir;
	os_file_dir_t	dbdir;
	os_file_stat_t	dbinfo;
	os_file_stat_t	fileinfo;
	dberr_t		err;
	bool		is_empty_dir;
	bool		is_file;
	bool		skip_first_level;
};

/************************************************************************
Holds the state needed to copy single data file. */
struct datafile_cur_t {
	os_file_t	file;
	fil_node_t*	node;
	char		rel_path[FN_REFLEN];
	char		abs_path[FN_REFLEN];
	MY_STAT		statinfo;
	uint		thread_n;
	byte*		orig_buf;
	byte*		buf;
	ib_int64_t	buf_size;
	ib_int64_t	buf_read;
	ib_int64_t	buf_offset;
};

/************************************************************************
Represents the context of the thread processing MySQL data directory. */
struct datadir_thread_ctxt_t {
	datadir_iter_t		*it;
	uint			n_thread;
	uint			*count;
	os_ib_mutex_t		count_mutex;
	os_thread_id_t		id;
	bool			ret;
};


/************************************************************************
Fill the node struct. Memory for node need to be allocated and freed by
the caller. It is caller responsibility to initialize node with
datadir_node_init and cleanup the memory with datadir_node_free.
Node can not be shared between threads. */
static
void
datadir_node_fill(datadir_node_t *node, datadir_iter_t *it)
{
	if (node->filepath_len < it->filepath_len) {
		free(node->filepath);
		node->filepath = (char*)(ut_malloc(it->filepath_len));
		node->filepath_len = it->filepath_len;
	}
	if (node->filepath_rel_len < it->filepath_rel_len) {
		free(node->filepath_rel);
		node->filepath_rel = (char*)(ut_malloc(it->filepath_rel_len));
		node->filepath_rel_len = it->filepath_rel_len;
	}

	strcpy(node->filepath, it->filepath);
	strcpy(node->filepath_rel, it->filepath_rel);
	node->is_empty_dir = it->is_empty_dir;
	node->is_file = it->is_file;
}

static
void
datadir_node_free(datadir_node_t *node)
{
	ut_free(node->filepath);
	ut_free(node->filepath_rel);
	memset(node, 0, sizeof(datadir_node_t));
}

static
void
datadir_node_init(datadir_node_t *node)
{
	memset(node, 0, sizeof(datadir_node_t));
}

/************************************************************************
Create the MySQL data directory iterator. Memory needs to be released
with datadir_iter_free. Position should be advanced with
datadir_iter_next_file. Iterator can be shared between multiple
threads. It is guaranteed that each thread receives unique file from
data directory into its local node struct. */
static
datadir_iter_t *
datadir_iter_new(const char *path, bool skip_first_level = true)
{
	datadir_iter_t *it;

	it = static_cast<datadir_iter_t *>(ut_malloc(sizeof(datadir_iter_t)));
	memset(it, 0, sizeof(datadir_iter_t));

	it->mutex = os_mutex_create();
	it->datadir_path = strdup(path);

	it->dir = os_file_opendir(it->datadir_path, TRUE);

	if (it->dir == NULL) {

		goto error;
	}

	it->err = DB_SUCCESS;

	it->dbpath_len = FN_REFLEN;
	it->dbpath = static_cast<char*>(ut_malloc(it->dbpath_len));

	it->filepath_len = FN_REFLEN;
	it->filepath = static_cast<char*>(ut_malloc(it->filepath_len));

	it->filepath_rel_len = FN_REFLEN;
	it->filepath_rel = static_cast<char*>(ut_malloc(it->filepath_rel_len));

	it->skip_first_level = skip_first_level;

	return(it);

error:
	ut_free(it);

	return(NULL);
}

static
bool
datadir_iter_next_database(datadir_iter_t *it)
{
	if (it->dbdir != NULL) {
		if (os_file_closedir(it->dbdir) != 0) {

			ibx_msg("Warning: could not"
			      " close database directory %s\n", it->dbpath);

			it->err = DB_ERROR;

		}
		it->dbdir = NULL;
	}

	while (fil_file_readdir_next_file(&it->err, it->datadir_path,
					  it->dir, &it->dbinfo) == 0) {
		ulint	len;

		if ((it->dbinfo.type == OS_FILE_TYPE_FILE
		     && it->skip_first_level)
		    || it->dbinfo.type == OS_FILE_TYPE_UNKNOWN) {

			continue;
		}

		/* We found a symlink or a directory; try opening it to see
		if a symlink is a directory */

		len = strlen(it->datadir_path)
			+ strlen (it->dbinfo.name) + 2;
		if (len > it->dbpath_len) {
			it->dbpath_len = len;

			if (it->dbpath) {

				ut_free(it->dbpath);
			}

			it->dbpath = static_cast<char*>
					(ut_malloc(it->dbpath_len));
		}
		ut_snprintf(it->dbpath, it->dbpath_len,
			    "%s/%s", it->datadir_path,
			    it->dbinfo.name);
		srv_normalize_path_for_win(it->dbpath);

		if (it->dbinfo.type == OS_FILE_TYPE_FILE) {
			it->is_file = true;
			return(true);
		}

		/* We want wrong directory permissions to be a fatal error for
		XtraBackup. */
		it->dbdir = os_file_opendir(it->dbpath, TRUE);

		if (it->dbdir != NULL) {

			it->is_file = false;
			return(true);
		}

	}

	return(false);
}

/************************************************************************
Concatenate n parts into single path */
static
void
make_path_n(int n, char **path, ulint *path_len, ...)
{
	ulint len_needed = n + 1;
	char *p;
	int i;
	va_list vl;

	ut_ad(n > 0);

	va_start(vl, path_len);
	for (i = 0; i < n; i++) {
		p = va_arg(vl, char*);
		len_needed += strlen(p);
	}
	va_end(vl);

	if (len_needed < *path_len) {
		ut_free(*path);
		*path = static_cast<char*>(ut_malloc(len_needed));
	}

	va_start(vl, path_len);
	p = va_arg(vl, char*);
	strcpy(*path, p);
	for (i = 1; i < n; i++) {
		size_t plen;
		p = va_arg(vl, char*);
		plen = strlen(*path);
		if (!is_path_separator((*path)[plen - 1])) {
			(*path)[plen] = FN_LIBCHAR;
			(*path)[plen + 1] = 0;
		}
		strcat(*path + plen, p);
	}
	va_end(vl);
}

static
bool
datadir_iter_next_file(datadir_iter_t *it)
{
	if (it->is_file && it->dbpath) {
		make_path_n(2, &it->filepath, &it->filepath_len,
				it->datadir_path, it->dbinfo.name);

		make_path_n(1, &it->filepath_rel, &it->filepath_rel_len,
				it->dbinfo.name);

		it->is_empty_dir = false;
		it->is_file = false;

		return(true);
	}

	if (!it->dbpath || !it->dbdir) {

		return(false);
	}

	while (fil_file_readdir_next_file(&it->err, it->dbpath, it->dbdir,
					  &it->fileinfo) == 0) {

		if (it->fileinfo.type == OS_FILE_TYPE_DIR) {

			continue;
		}

		/* We found a symlink or a file */
		make_path_n(3, &it->filepath, &it->filepath_len,
				it->datadir_path, it->dbinfo.name,
				it->fileinfo.name);

		make_path_n(2, &it->filepath_rel, &it->filepath_rel_len,
				it->dbinfo.name, it->fileinfo.name);

		it->is_empty_dir = false;

		return(true);
	}

	return(false);
}

static
bool
datadir_iter_next(datadir_iter_t *it, datadir_node_t *node)
{
	bool	ret = true;

	os_mutex_enter(it->mutex);

	if (datadir_iter_next_file(it)) {

		datadir_node_fill(node, it);

		goto done;
	}

	while (datadir_iter_next_database(it)) {

		if (datadir_iter_next_file(it)) {

			datadir_node_fill(node, it);

			goto done;
		}

		make_path_n(2, &it->filepath, &it->filepath_len,
				it->datadir_path, it->dbinfo.name);

		make_path_n(1, &it->filepath_rel, &it->filepath_rel_len,
				it->dbinfo.name);

		it->is_empty_dir = true;

		datadir_node_fill(node, it);

		goto done;
	}

	/* nothing found */
	ret = false;

done:
	os_mutex_exit(it->mutex);

	return(ret);
}

/************************************************************************
Interface to read MySQL data file sequentially. One should open file
with datafile_open to get cursor and close the cursor with
datafile_close. Cursor can not be shared between multiple
threads. */
static
void
datadir_iter_free(datadir_iter_t *it)
{
	os_mutex_free(it->mutex);

	if (it->dbdir) {

		os_file_closedir(it->dbdir);
	}

	if (it->dir) {

		os_file_closedir(it->dir);
	}

	ut_free(it->dbpath);
	ut_free(it->filepath);
	ut_free(it->filepath_rel);
	free(it->datadir_path);
	ut_free(it);
}

static
void
datafile_close(datafile_cur_t *cursor)
{
	if (cursor->file != 0) {
		os_file_close(cursor->file);
	}
	ut_free(cursor->buf);
}

static
bool
datafile_open(const char *file, datafile_cur_t *cursor, uint thread_n)
{
	ulint		success;

	memset(cursor, 0, sizeof(datafile_cur_t));

	strncpy(cursor->abs_path, file, sizeof(cursor->abs_path));

	/* Get the relative path for the destination tablespace name, i.e. the
	one that can be appended to the backup root directory. Non-system
	tablespaces may have absolute paths for remote tablespaces in MySQL
	5.6+. We want to make "local" copies for the backup. */
	strncpy(cursor->rel_path,
		xb_get_relative_path(cursor->abs_path, FALSE),
		sizeof(cursor->rel_path));

	cursor->file = os_file_create_simple_no_error_handling(0,
							cursor->abs_path,
							OS_FILE_OPEN,
							OS_FILE_READ_ONLY,
							&success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		ibx_msg("[%02u] error: cannot open "
			"file %s\n",
			thread_n, cursor->abs_path);

		return(false);
	}

	if (my_fstat(cursor->file, &cursor->statinfo, MYF(MY_WME))) {
		ibx_msg("[%02u] error: cannot stat %s\n",
			thread_n, cursor->abs_path);

		datafile_close(cursor);

		return(false);
	}

	posix_fadvise(cursor->file, 0, 0, POSIX_FADV_SEQUENTIAL);

	cursor->buf_size = 10 * 1024 * 1024;
	cursor->buf = static_cast<byte *>(ut_malloc(cursor->buf_size));

	return(true);
}


static
xb_fil_cur_result_t
datafile_read(datafile_cur_t *cursor)
{
	ulint		success;
	ulint		to_read;

	xtrabackup_io_throttling();

	to_read = min(cursor->statinfo.st_size - cursor->buf_offset, 
		      cursor->buf_size);

	if (to_read == 0) {
		return(XB_FIL_CUR_EOF);
	}

	success = os_file_read(cursor->file, cursor->buf, cursor->buf_offset,
			       to_read);
	if (!success) {
		return(XB_FIL_CUR_ERROR);
	}

	posix_fadvise(cursor->file, cursor->buf_offset, to_read,
			POSIX_FADV_DONTNEED);

	cursor->buf_read = to_read;
	cursor->buf_offset += to_read;

	return(XB_FIL_CUR_SUCCESS);
}

/************************************************************************
Check if directory exists. Optionally create directory if doesn't
exist.
@return true if directory exists and if it was created successfully. */
static
bool
directory_exists(const char *dir, bool create)
{
	os_file_dir_t os_dir;
	MY_STAT stat_arg;
	char errbuf[MYSYS_STRERROR_SIZE];

	if (!my_stat(dir, &stat_arg, MYF(0))) {

		if (mkdirp(dir, 0777, MYF(0)) < 0) {

			ibx_msg("Can not create directory %s: %s\n", dir,
				my_strerror(errbuf, sizeof(errbuf), my_errno));

			return(false);

		}
	}

	/* could be symlink */
	os_dir = os_file_opendir(dir, FALSE);

	if (os_dir == NULL) {

		ibx_msg("Can not open directory %s: %s\n", dir,
			my_strerror(errbuf, sizeof(errbuf), my_errno));

		return(false);
	}

	os_file_closedir(os_dir);

	return(true);
}

/************************************************************************
Check that directory exists and it is empty. */
static
bool
directory_exists_and_empty(const char *dir, const char *comment)
{
	os_file_dir_t os_dir;
	dberr_t err;
	os_file_stat_t info;
	bool empty;

	if (!directory_exists(dir, true)) {
		return(false);
	}

	os_dir = os_file_opendir(dir, FALSE);

	if (os_dir == NULL) {
		ibx_msg("%s can not open directory %s\n", comment, dir);
		return(false);
	}

	empty = (fil_file_readdir_next_file(&err, dir, os_dir, &info) != 0);

	os_file_closedir(os_dir);

	if (!empty) {
		ibx_msg("%s directory %s is not empty!\n", comment, dir);
	}

	return(empty);
}


/************************************************************************
Copy file for backup/restore.
@return true in case of success. */
static
bool
copy_file(const char *src_file_path, const char *dst_file_path, uint thread_n)
{
	char			 dst_name[FN_REFLEN];
	ds_file_t		*dstfile = NULL;
	datafile_cur_t		 cursor;
	xb_fil_cur_result_t	 res;
	const char		*action;

	if (!datafile_open(src_file_path, &cursor, thread_n)) {
		goto error;
	}

	strncpy(dst_name, cursor.rel_path, sizeof(dst_name));

	dstfile = ds_open(ds_data, trim_dotslash(dst_file_path),
				   &cursor.statinfo);
	if (dstfile == NULL) {
		ibx_msg("[%02u] error: "
			"cannot open the destination stream for %s\n",
			thread_n, dst_name);
		goto error;
	}

	action = xb_get_copy_action();
	ibx_msg("[%02u] %s %s to %s\n",
		thread_n, action, src_file_path, dstfile->path);

	/* The main copy loop */
	while ((res = datafile_read(&cursor)) == XB_FIL_CUR_SUCCESS) {

		if (ds_write(dstfile, cursor.buf, cursor.buf_read)) {
			goto error;
		}
	}

	if (res == XB_FIL_CUR_ERROR) {
		goto error;
	}

	/* close */
	ibx_msg("[%02u]        ...done\n", thread_n);
	datafile_close(&cursor);
	ds_close(dstfile);
	return(true);

error:
	datafile_close(&cursor);
	if (dstfile != NULL) {
		ds_close(dstfile);
	}
	ibx_msg("[%02u] Error: copy_file() failed.\n", thread_n);
	return(false); /*ERROR*/
}


/************************************************************************
Try to move file by renaming it. If source and destination are on
different devices fall back to copy and unlink.
@return true in case of success. */
static
bool
move_file(const char *src_file_path, const char *dst_file_path, uint thread_n)
{
	char errbuf[MYSYS_STRERROR_SIZE];
	char dst_file_path_abs[FN_REFLEN];
	char dst_dir_abs[FN_REFLEN];
	size_t dirname_length;

	ut_snprintf(dst_file_path_abs, sizeof(dst_file_path_abs),
			"%s/%s", mysql_data_home, dst_file_path);

	dirname_part(dst_dir_abs, dst_file_path_abs, &dirname_length);

	if (!directory_exists(dst_dir_abs, true)) {
		return(false);
	}

	if (file_exists(dst_file_path_abs)) {
		ibx_msg("Error: Move file %s to %s failed: Destination "
			"file exists\n",
			src_file_path, dst_file_path_abs);
		return(false);
	}

	ibx_msg("[%02u] Moving %s to %s\n",
		thread_n, src_file_path, dst_file_path_abs);

	if (my_rename(src_file_path, dst_file_path_abs, MYF(0)) != 0) {
		if (my_errno == EXDEV) {
			bool ret;
			ret = copy_file(src_file_path,
					dst_file_path, thread_n);
			ibx_msg("[%02u] Removing %s\n",
					thread_n, src_file_path);
			if (unlink(src_file_path) != 0) {
				ibx_msg("Error: unlink %s failed: %s\n",
					src_file_path,
					my_strerror(errbuf,
						    sizeof(errbuf), errno));
			}
			return(ret);
		}
		ibx_msg("Can not move file %s to %s: %s\n",
			src_file_path, dst_file_path_abs,
			my_strerror(errbuf, sizeof(errbuf), my_errno));
		return(false);
	}

	ibx_msg("[%02u]        ...done\n", thread_n);

	return(true);
}


/************************************************************************
Copy or move file depending on current mode.
@return true in case of success. */
static
bool
copy_or_move_file(const char *src_file_path,
			const char *dst_file_path, uint thread_n)
{
	return(ibx_mode == IBX_MODE_COPY_BACK ?
		copy_file(src_file_path, dst_file_path, thread_n) :
		move_file(src_file_path, dst_file_path, thread_n));
}


/************************************************************************
Check if file name ends with given set of suffixes.
@return true if it does. */
static
bool
filename_matches(const char *filename, const char **ext_list)
{
	const char **ext;

	for (ext = ext_list; *ext; ext++) {
		if (ends_with(filename, *ext)) {
			return(true);
		}
	}

	return(false);
}


/************************************************************************
Copy data file for backup. Also check if it is allowed to copy by
comparing its name to the list of known data file types and checking
if passes the rules for partial backup.
@return true if file backed up or skipped successfully. */
static
bool
datafile_copy_backup(const char *filepath, uint thread_n)
{
	const char *ext_list[] = {"frm", "isl", "MYD", "MYI", "MAD", "MAI",
		"MRG", "TRG", "TRN", "ARM", "ARZ", "CSM", "CSV", "opt", "par",
		NULL};

	/* Get the name and the path for the tablespace. node->name always
	contains the path (which may be absolute for remote tablespaces in
	5.6+). space->name contains the tablespace name in the form
	"./database/table.ibd" (in 5.5-) or "database/table" (in 5.6+). For a
	multi-node shared tablespace, space->name contains the name of the first
	node, but that's irrelevant, since we only need node_name to match them
	against filters, and the shared tablespace is always copied regardless
	of the filters value. */

	if (ibx_partial_backup && check_if_skip_table(filepath)) {
		ibx_msg("[%02u] Skipping %s.\n", thread_n, filepath);
		return(true);
	}

	if (filename_matches(filepath, ext_list)) {
		return copy_file(filepath, filepath, thread_n);
	}

	return(true);
}


/************************************************************************
Same as datafile_copy_backup, but put file name into the list for
rsync command. */
static
bool
datafile_rsync_backup(const char *filepath, bool save_to_list, FILE *f)
{
	const char *ext_list[] = {"frm", "isl", "MYD", "MYI", "MAD", "MAI",
		"MRG", "TRG", "TRN", "ARM", "ARZ", "CSM", "CSV", "opt", "par",
		NULL};

	/* Get the name and the path for the tablespace. node->name always
	contains the path (which may be absolute for remote tablespaces in
	5.6+). space->name contains the tablespace name in the form
	"./database/table.ibd" (in 5.5-) or "database/table" (in 5.6+). For a
	multi-node shared tablespace, space->name contains the name of the first
	node, but that's irrelevant, since we only need node_name to match them
	against filters, and the shared tablespace is always copied regardless
	of the filters value. */

	if (ibx_partial_backup && check_if_skip_table(filepath)) {
		return(true);
	}

	if (filename_matches(filepath, ext_list)) {
		fprintf(f, "%s\n", filepath);
		if (save_to_list) {
			rsync_list.insert(filepath);
		}
	}

	return(true);
}


static
bool
backup_file_vprintf(const char *filename, const char *fmt, va_list ap)
{
	ds_file_t	*dstfile	= NULL;
	MY_STAT		 stat;			/* unused for now */
	char		*buf		= 0;
	int		 buf_len;
	const char	*action;

	memset(&stat, 0, sizeof(stat));

	buf_len = vasprintf(&buf, fmt, ap);

	stat.st_size = buf_len;
	stat.st_mtime = my_time(0);

	dstfile = ds_open(ds_data, filename, &stat);
	if (dstfile == NULL) {
		ibx_msg("[%02u] error: "
			"cannot open the destination stream for %s\n",
			0, filename);
		goto error;
	}

	action = xb_get_copy_action("Writing");
	ibx_msg("[%02u] %s %s\n", 0, action, filename);

	if (buf_len == -1) {
		goto error;
	}

	if (ds_write(dstfile, buf, buf_len)) {
		goto error;
	}

	/* close */
	ibx_msg("[%02u]        ...done\n", 0);
	free(buf);
	ds_close(dstfile);
	return(true);

error:
	free(buf);
	if (dstfile != NULL) {
		ds_close(dstfile);
	}
	ibx_msg("[%02u] Error: backup file failed.\n", 0);
	return(false); /*ERROR*/
}


static bool
backup_file_printf(const char *filename, const char *fmt, ...)
		ATTRIBUTE_FORMAT(printf, 2, 0);

static
bool
backup_file_printf(const char *filename, const char *fmt, ...)
{
	bool result;
	va_list ap;

	va_start(ap, fmt);

	result = backup_file_vprintf(filename, fmt, ap);

	va_end(ap);

	return(result);
}

static
bool
run_data_threads(datadir_iter_t *it, os_thread_func_t func, uint n)
{
	datadir_thread_ctxt_t	*data_threads;
	uint			i, count;
	os_ib_mutex_t		count_mutex;
	bool			ret;

	data_threads = (datadir_thread_ctxt_t*)
				(ut_malloc(sizeof(datadir_thread_ctxt_t) * n));

	count_mutex = os_mutex_create();
	count = n;

	for (i = 0; i < n; i++) {
		data_threads[i].it = it;
		data_threads[i].n_thread = i + 1;
		data_threads[i].count = &count;
		data_threads[i].count_mutex = count_mutex;
		os_thread_create(func, data_threads + i, &data_threads[i].id);
	}

	/* Wait for threads to exit */
	while (1) {
		os_thread_sleep(100000);
		os_mutex_enter(count_mutex);
		if (count == 0) {
			os_mutex_exit(count_mutex);
			break;
		}
		os_mutex_exit(count_mutex);
	}

	os_mutex_free(count_mutex);

	ret = true;
	for (i = 0; i < n; i++) {
		ret = data_threads[i].ret && ret;
		if (!data_threads[i].ret) {
			ibx_msg("Error: thread %u failed.\n", i);
		}
	}

	ut_free(data_threads);

	return(ret);
}

enum innobackupex_options
{
	OPT_USER = 256,
	OPT_HOST,
	OPT_PORT,
	OPT_PASSWORD,
	OPT_SOCKET,
	OPT_APPLY_LOG,
	OPT_COPY_BACK,
	OPT_MOVE_BACK,
	OPT_REDO_ONLY,
	OPT_GALERA_INFO,
	OPT_SLAVE_INFO,
	OPT_INCREMENTAL,
	OPT_INCREMENTAL_HISTORY_NAME,
	OPT_INCREMENTAL_HISTORY_UUID,
	OPT_LOCK_WAIT_QUERY_TYPE,
	OPT_KILL_LONG_QUERY_TYPE,
	OPT_KILL_LONG_QUERIES_TIMEOUT,
	OPT_LOCK_WAIT_TIMEOUT,
	OPT_LOCK_WAIT_THRESHOLD,
	OPT_DEBUG_SLEEP_BEFORE_UNLOCK,
	OPT_NO_LOCK,
	OPT_SAFE_SLAVE_BACKUP,
	OPT_SAFE_SLAVE_BACKUP_TIMEOUT,
	OPT_RSYNC,
	OPT_HISTORY,
	OPT_INCLUDE,
	OPT_FORCE_NON_EMPTY_DIRS,
	OPT_NO_TIMESTAMP,
	OPT_NO_VERSION_CHECK,
	OPT_NO_BACKUP_LOCKS,
	OPT_DATABASES,
	OPT_DECRYPT,
	OPT_DECOMPRESS,

	/* options wich are passed directly to xtrabackup */
	OPT_CLOSE_FILES,
	OPT_COMPACT,
	OPT_COMPRESS,
	OPT_COMPRESS_THREADS,
	OPT_COMPRESS_CHUNK_SIZE,
	OPT_ENCRYPT,
	OPT_ENCRYPT_KEY,
	OPT_ENCRYPT_KEY_FILE,
	OPT_ENCRYPT_THREADS,
	OPT_ENCRYPT_CHUNK_SIZE,
	OPT_EXPORT,
	OPT_EXTRA_LSNDIR,
	OPT_INCREMENTAL_BASEDIR,
	OPT_INCREMENTAL_DIR,
	OPT_INCREMENTAL_FORCE_SCAN,
	OPT_LOG_COPY_INTERVAL,
	OPT_PARALLEL,
	OPT_REBUILD_INDEXES,
	OPT_REBUILD_THREADS,
	OPT_STREAM,
	OPT_TABLES_FILE,
	OPT_THROTTLE,
	OPT_USE_MEMORY
};

ibx_mode_t ibx_mode = IBX_MODE_BACKUP;

static struct my_option ibx_long_options[] =
{
	{"version", 'v', "print xtrabackup version information",
	 (uchar *) &opt_ibx_version, (uchar *) &opt_ibx_version, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"help", '?', "This option displays a help screen and exits.",
	 (uchar *) &opt_ibx_help, (uchar *) &opt_ibx_help, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"apply-log", OPT_APPLY_LOG, "Prepare a backup in BACKUP-DIR by "
	"applying the transaction log file named \"xtrabackup_logfile\" "
	"located in the same directory. Also, create new transaction logs. "
	"The InnoDB configuration is read from the file \"backup-my.cnf\".",
	(uchar*) &opt_ibx_apply_log, (uchar*) &opt_ibx_apply_log,
	0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"redo-only", OPT_REDO_ONLY, "This option should be used when "
	 "preparing the base full backup and when merging all incrementals "
	 "except the last one. This forces xtrabackup to skip the \"rollback\" "
	 "phase and do a \"redo\" only. This is necessary if the backup will "
	 "have incremental changes applied to it later. See the xtrabackup "
	 "documentation for details.",
	 (uchar *) &opt_ibx_redo_only, (uchar *) &opt_ibx_redo_only, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"copy-back", OPT_COPY_BACK, "Copy all the files in a previously made "
	 "backup from the backup directory to their original locations.",
	 (uchar *) &opt_ibx_copy_back, (uchar *) &opt_ibx_copy_back, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"move-back", OPT_MOVE_BACK, "Move all the files in a previously made "
	 "backup from the backup directory to the actual datadir location. "
	 "Use with caution, as it removes backup files.",
	 (uchar *) &opt_ibx_move_back, (uchar *) &opt_ibx_move_back, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"galera-info", OPT_GALERA_INFO, "This options creates the "
	 "xtrabackup_galera_info file which contains the local node state at "
	 "the time of the backup. Option should be used when performing the "
	 "backup of Percona-XtraDB-Cluster. Has no effect when backup locks "
	 "are used to create the backup.",
	 (uchar *) &opt_ibx_galera_info, (uchar *) &opt_ibx_galera_info, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"slave-info", OPT_SLAVE_INFO, "This option is useful when backing "
	 "up a replication slave server. It prints the binary log position "
	 "and name of the master server. It also writes this information to "
	 "the \"xtrabackup_slave_info\" file as a \"CHANGE MASTER\" command. "
	 "A new slave for this master can be set up by starting a slave server "
	 "on this backup and issuing a \"CHANGE MASTER\" command with the "
	 "binary log position saved in the \"xtrabackup_slave_info\" file.",
	 (uchar *) &opt_ibx_slave_info, (uchar *) &opt_ibx_slave_info, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"incremental", OPT_INCREMENTAL, "This option tells xtrabackup to "
	 "create an incremental backup, rather than a full one. It is passed "
	 "to the xtrabackup child process. When this option is specified, "
	 "either --incremental-lsn or --incremental-basedir can also be given. "
	 "If neither option is given, option --incremental-basedir is passed "
	 "to xtrabackup by default, set to the first timestamped backup "
	 "directory in the backup base directory.",
	 (uchar *) &opt_ibx_incremental, (uchar *) &opt_ibx_incremental, 0,
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
	 (uchar *) &opt_ibx_no_lock, (uchar *) &opt_ibx_no_lock, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"safe-slave-backup", OPT_SAFE_SLAVE_BACKUP, "Stop slave SQL thread "
	 "and wait to start backup until Slave_open_temp_tables in "
	 "\"SHOW STATUS\" is zero. If there are no open temporary tables, "
	 "the backup will take place, otherwise the SQL thread will be "
	 "started and stopped until there are no open temporary tables. "
	 "The backup will fail if Slave_open_temp_tables does not become "
	 "zero after --safe-slave-backup-timeout seconds. The slave SQL "
	 "thread will be restarted when the backup finishes.",
	 (uchar *) &opt_ibx_safe_slave_backup,
	 (uchar *) &opt_ibx_safe_slave_backup,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"rsync", OPT_RSYNC, "Uses the rsync utility to optimize local file "
	 "transfers. When this option is specified, innobackupex uses rsync "
	 "to copy all non-InnoDB files instead of spawning a separate cp for "
	 "each file, which can be much faster for servers with a large number "
	 "of databases or tables.  This option cannot be used together with "
	 "--stream.",
	 (uchar *) &opt_ibx_rsync, (uchar *) &opt_ibx_rsync,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"force-non-empty-directories", OPT_FORCE_NON_EMPTY_DIRS, "This "
	 "option, when specified, makes --copy-back or --move-back transfer "
	 "files to non-empty directories. Note that no existing files will be "
	 "overwritten. If --copy-back or --nove-back has to copy a file from "
	 "the backup directory which already exists in the destination "
	 "directory, it will still fail with an error.",
	 (uchar *) &opt_ibx_force_non_empty_dirs,
	 (uchar *) &opt_ibx_force_non_empty_dirs,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"no-timestamp", OPT_NO_TIMESTAMP, "This option prevents creation of a "
	 "time-stamped subdirectory of the BACKUP-ROOT-DIR given on the "
	 "command line. When it is specified, the backup is done in "
	 "BACKUP-ROOT-DIR instead.",
	 (uchar *) &opt_ibx_notimestamp,
	 (uchar *) &opt_ibx_notimestamp,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"no-version-check", OPT_NO_VERSION_CHECK, "This option disables the "
	 "version check which is enabled by the --version-check option.",
	 (uchar *) &opt_ibx_noversioncheck,
	 (uchar *) &opt_ibx_noversioncheck,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"no-backup-locks", OPT_NO_BACKUP_LOCKS, "This option controls if "
	 "backup locks should be used instead of FLUSH TABLES WITH READ LOCK "
	 "on the backup stage. The option has no effect when backup locks are "
	 "not supported by the server. This option is enabled by default, "
	 "disable with --no-backup-locks.",
	 (uchar *) &opt_ibx_no_backup_locks,
	 (uchar *) &opt_ibx_no_backup_locks,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"decompress", OPT_DECOMPRESS, "Decompresses all files with the .qp "
	 "extension in a backup previously made with the --compress option.",
	 (uchar *) &opt_ibx_decompress,
	 (uchar *) &opt_ibx_decompress,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"user", OPT_USER, "This option specifies the MySQL username used "
	 "when connecting to the server, if that's not the current user. "
	 "The option accepts a string argument. See mysql --help for details.",
	 (uchar*) &opt_ibx_user, (uchar*) &opt_ibx_user, 0, GET_STR,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"host", OPT_HOST, "This option specifies the host to use when "
	 "connecting to the database server with TCP/IP.  The option accepts "
	 "a string argument. See mysql --help for details.",
	 (uchar*) &opt_ibx_host, (uchar*) &opt_ibx_host, 0, GET_STR,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"port", OPT_PORT, "This option specifies the port to use when "
	 "connecting to the database server with TCP/IP.  The option accepts "
	 "a string argument. See mysql --help for details.",
	 &opt_ibx_port, &opt_ibx_port, 0, GET_UINT, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"password", OPT_PASSWORD, "This option specifies the password to use "
	 "when connecting to the database. It accepts a string argument.  "
	 "See mysql --help for details.",
	 (uchar*) &opt_ibx_password, (uchar*) &opt_ibx_password, 0, GET_STR,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"socket", OPT_SOCKET, "This option specifies the socket to use when "
	 "connecting to the local database server with a UNIX domain socket.  "
	 "The option accepts a string argument. See mysql --help for details.",
	 (uchar*) &opt_ibx_socket, (uchar*) &opt_ibx_socket, 0, GET_STR,
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
	 (uchar*) &opt_ibx_incremental_history_name,
	 (uchar*) &opt_ibx_incremental_history_name, 0, GET_STR,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"incremental-history-uuid", OPT_INCREMENTAL_HISTORY_UUID,
	 "This option specifies the UUID of the specific history record "
	 "stored in the PERCONA_SCHEMA.xtrabackup_history to base an "
	 "incremental backup on. --incremental-history-name, "
	 "--incremental-basedir and --incremental-lsn. If no valid lsn can be "
	 "found (no success record with that uuid) xtrabackup will return "
	 "with an error. It is used with the --incremental option.",
	 (uchar*) &opt_ibx_incremental_history_uuid,
	 (uchar*) &opt_ibx_incremental_history_uuid, 0, GET_STR,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"decrypt", OPT_DECRYPT, "Decrypts all files with the .xbcrypt "
	 "extension in a backup previously made with --encrypt option.",
	 &opt_ibx_decrypt_algo, &opt_ibx_decrypt_algo,
	 &xtrabackup_encrypt_algo_typelib, GET_ENUM, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"ftwrl-wait-query-type", OPT_LOCK_WAIT_QUERY_TYPE,
	 "This option specifies which types of queries are allowed to complete "
	 "before innobackupex will issue the global lock. Default is all.",
	 (uchar*) &opt_ibx_lock_wait_query_type,
	 (uchar*) &opt_ibx_lock_wait_query_type, &ibx_query_type_typelib,
	 GET_ENUM, REQUIRED_ARG, IBX_QUERY_TYPE_UPDATE, 0, 0, 0, 0, 0},

	{"kill-long-query-type", OPT_KILL_LONG_QUERY_TYPE,
	 "This option specifies which types of queries should be killed to "
	 "unblock the global lock. Default is \"all\".",
	 (uchar*) &opt_ibx_kill_long_query_type,
	 (uchar*) &opt_ibx_kill_long_query_type, &ibx_query_type_typelib,
	 GET_ENUM, REQUIRED_ARG, IBX_QUERY_TYPE_SELECT, 0, 0, 0, 0, 0},

	{"history", OPT_HISTORY,
	 "This option enables the tracking of backup history in the "
	 "PERCONA_SCHEMA.xtrabackup_history table. An optional history "
	 "series name may be specified that will be placed with the history "
	 "record for the current backup being taken.",
	 NULL, NULL, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

	{"include", OPT_INCLUDE,
	 "This option is a regular expression to be matched against table "
	 "names in databasename.tablename format. It is passed directly to "
	 "xtrabackup's --tables option. See the xtrabackup documentation for "
	 "details.",
	 (uchar*) &opt_ibx_include,
	 (uchar*) &opt_ibx_include, 0, GET_STR,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"databases", OPT_DATABASES,
	 "This option specifies the list of databases that innobackupex should "
	 "back up. The option accepts a string argument or path to file that "
	 "contains the list of databases to back up. The list is of the form "
	 "\"databasename1[.table_name1] databasename2[.table_name2] . . .\". "
	 "If this option is not specified, all databases containing MyISAM and "
	 "InnoDB tables will be backed up.  Please make sure that --databases "
	 "contains all of the InnoDB databases and tables, so that all of the "
	 "innodb.frm files are also backed up. In case the list is very long, "
	 "this can be specified in a file, and the full path of the file can "
	 "be specified instead of the list. (See option --tables-file.)",
	 (uchar*) &opt_ibx_databases,
	 (uchar*) &opt_ibx_databases, 0, GET_STR,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"kill-long-queries-timeout", OPT_KILL_LONG_QUERIES_TIMEOUT,
	 "This option specifies the number of seconds innobackupex waits "
	 "between starting FLUSH TABLES WITH READ LOCK and killing those "
	 "queries that block it. Default is 0 seconds, which means "
	 "innobackupex will not attempt to kill any queries.",
	 (uchar*) &opt_ibx_kill_long_queries_timeout,
	 (uchar*) &opt_ibx_kill_long_queries_timeout, 0, GET_UINT,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"ftwrl-wait-timeout", OPT_LOCK_WAIT_TIMEOUT,
	 "This option specifies time in seconds that innobackupex should wait "
	 "for queries that would block FTWRL before running it. If there are "
	 "still such queries when the timeout expires, innobackupex terminates "
	 "with an error. Default is 0, in which case innobackupex does not "
	 "wait for queries to complete and starts FTWRL immediately.",
	 (uchar*) &opt_ibx_lock_wait_timeout,
	 (uchar*) &opt_ibx_lock_wait_timeout, 0, GET_UINT,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"ftwrl-wait-threshold", OPT_LOCK_WAIT_THRESHOLD,
	 "This option specifies the query run time threshold which is used by "
	 "innobackupex to detect long-running queries with a non-zero value "
	 "of --ftwrl-wait-timeout. FTWRL is not started until such "
	 "long-running queries exist. This option has no effect if "
	 "--ftwrl-wait-timeout is 0. Default value is 60 seconds.",
	 (uchar*) &opt_ibx_lock_wait_threshold,
	 (uchar*) &opt_ibx_lock_wait_threshold, 0, GET_UINT,
	 REQUIRED_ARG, 60, 0, 0, 0, 0, 0},

	{"debug-sleep-before-unlock", OPT_DEBUG_SLEEP_BEFORE_UNLOCK,
	 "This is a debug-only option used by the XtraBackup test suite.",
	 (uchar*) &opt_ibx_debug_sleep_before_unlock,
	 (uchar*) &opt_ibx_debug_sleep_before_unlock, 0, GET_UINT,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"safe-slave-backup-timeout", OPT_SAFE_SLAVE_BACKUP_TIMEOUT,
	 "How many seconds --safe-slave-backup should wait for "
	 "Slave_open_temp_tables to become zero. (default 300)",
	 (uchar*) &opt_ibx_safe_slave_backup_timeout,
	 (uchar*) &opt_ibx_safe_slave_backup_timeout, 0, GET_UINT,
	 REQUIRED_ARG, 300, 0, 0, 0, 0, 0},

	{"decrypt", OPT_DECRYPT, "Decrypts all files with the .xbcrypt "
	 "extension in a backup previously made with --encrypt option.",
	 &opt_ibx_decrypt_algo, &opt_ibx_decrypt_algo,
	 &xtrabackup_encrypt_algo_typelib, GET_ENUM, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},


	/* Following command-line options are actually handled by xtrabackup.
	We put them here with only purpose for them to showup in
	innobackupex --help output */

	{"close_files", OPT_CLOSE_FILES, "Do not keep files opened. This "
	 "option is passed directly to xtrabackup. Use at your own risk.",
	 (uchar*) &ibx_xb_close_files, (uchar*) &ibx_xb_close_files, 0,
	 GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"compact", OPT_COMPACT, "Create a compact backup with all secondary "
	 "index pages omitted. This option is passed directly to xtrabackup. "
	 "See xtrabackup documentation for details.",
	 (uchar*) &ibx_xtrabackup_compact, (uchar*) &ibx_xtrabackup_compact,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"compress", OPT_COMPRESS, "This option instructs xtrabackup to "
	 "compress backup copies of InnoDB data files. It is passed directly "
	 "to the xtrabackup child process. Try 'xtrabackup --help' for more "
	 "details.", (uchar*) &ibx_xtrabackup_compress_alg,
	 (uchar*) &ibx_xtrabackup_compress_alg, 0,
	 GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},

	{"compress-threads", OPT_COMPRESS_THREADS,
	 "This option specifies the number of worker threads that will be used "
	 "for parallel compression. It is passed directly to the xtrabackup "
	 "child process. Try 'xtrabackup --help' for more details.",
	 (uchar*) &ibx_xtrabackup_compress_threads,
	 (uchar*) &ibx_xtrabackup_compress_threads,
	 0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

	{"compress-chunk-size", OPT_COMPRESS_CHUNK_SIZE, "Size of working "
	 "buffer(s) for compression threads in bytes. The default value "
	 "is 64K.", (uchar*) &ibx_xtrabackup_compress_chunk_size,
	 (uchar*) &ibx_xtrabackup_compress_chunk_size,
	 0, GET_ULL, REQUIRED_ARG, (1 << 16), 1024, ULONGLONG_MAX, 0, 0, 0},

	{"encrypt", OPT_ENCRYPT, "This option instructs xtrabackup to encrypt "
	 "backup copies of InnoDB data files using the algorithm specified in "
	 "the ENCRYPTION-ALGORITHM. It is passed directly to the xtrabackup "
	 "child process. Try 'xtrabackup --help' for more details.",
	 &ibx_xtrabackup_encrypt_algo, &ibx_xtrabackup_encrypt_algo,
	 &xtrabackup_encrypt_algo_typelib, GET_ENUM, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"encrypt-key", OPT_ENCRYPT_KEY, "This option instructs xtrabackup to "
	 "use the given ENCRYPTION-KEY when using the --encrypt or --decrypt "
	 "options. During backup it is passed directly to the xtrabackup child "
	 "process. Try 'xtrabackup --help' for more details.",
	 (uchar*) &ibx_xtrabackup_encrypt_key,
	 (uchar*) &ibx_xtrabackup_encrypt_key, 0,
	 GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"encrypt-key-file", OPT_ENCRYPT_KEY_FILE, "This option instructs "
	 "xtrabackup to use the encryption key stored in the given "
	 "ENCRYPTION-KEY-FILE when using the --encrypt or --decrypt options.",
	 (uchar*) &ibx_xtrabackup_encrypt_key_file,
	 (uchar*) &ibx_xtrabackup_encrypt_key_file, 0,
	 GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"encrypt-threads", OPT_ENCRYPT_THREADS,
	 "This option specifies the number of worker threads that will be used "
	 "for parallel encryption. It is passed directly to the xtrabackup "
	 "child process. Try 'xtrabackup --help' for more details.",
	 (uchar*) &ibx_xtrabackup_encrypt_threads,
	 (uchar*) &ibx_xtrabackup_encrypt_threads,
	 0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

	{"encrypt-chunk-size", OPT_ENCRYPT_CHUNK_SIZE,
	 "This option specifies the size of the internal working buffer for "
	 "each encryption thread, measured in bytes. It is passed directly to "
	 "the xtrabackup child process. Try 'xtrabackup --help' for more "
	 "details.",
	 (uchar*) &ibx_xtrabackup_encrypt_chunk_size,
	 (uchar*) &ibx_xtrabackup_encrypt_chunk_size,
	 0, GET_ULL, REQUIRED_ARG, (1 << 16), 1024, ULONGLONG_MAX, 0, 0, 0},

	{"export", OPT_EXPORT, "This option is passed directly to xtrabackup's "
	 "--export option. It enables exporting individual tables for import "
	 "into another server. See the xtrabackup documentation for details.",
	 (uchar*) &ibx_xtrabackup_export, (uchar*) &ibx_xtrabackup_export,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"extra-lsndir", OPT_EXTRA_LSNDIR, "This option specifies the "
	 "directory in which to save an extra copy of the "
	 "\"xtrabackup_checkpoints\" file. The option accepts a string "
	 "argument. It is passed directly to xtrabackup's --extra-lsndir "
	 "option. See the xtrabackup documentation for details.",
	 (uchar*) &ibx_xtrabackup_extra_lsndir,
	 (uchar*) &ibx_xtrabackup_extra_lsndir,
	 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"incremental-basedir", OPT_INCREMENTAL_BASEDIR, "This option "
	 "specifies the directory containing the full backup that is the base "
	 "dataset for the incremental backup.  The option accepts a string "
	 "argument. It is used with the --incremental option.",
	 (uchar*) &ibx_xtrabackup_incremental_basedir,
	 (uchar*) &ibx_xtrabackup_incremental_basedir,
	 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"incremental-dir", OPT_INCREMENTAL_DIR, "This option specifies the "
	 "directory where the incremental backup will be combined with the "
	 "full backup to make a new full backup.  The option accepts a string "
	 "argument. It is used with the --incremental option.",
	 (uchar*) &ibx_xtrabackup_incremental_dir,
	 (uchar*) &ibx_xtrabackup_incremental_dir,
	 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"incremental-force-scan", OPT_INCREMENTAL_FORCE_SCAN,
	 "This options tells xtrabackup to perform full scan of data files "
	 "for taking an incremental backup even if full changed page bitmap "
	 "data is available to enable the backup without the full scan.",
	 (uchar*)&ibx_xtrabackup_incremental_force_scan,
	 (uchar*)&ibx_xtrabackup_incremental_force_scan, 0, GET_BOOL, NO_ARG,
	 0, 0, 0, 0, 0, 0},

	{"log-copy-interval", OPT_LOG_COPY_INTERVAL, "This option specifies "
	 "time interval between checks done by log copying thread in "
	 "milliseconds.", (uchar*) &ibx_xtrabackup_log_copy_interval,
	 (uchar*) &ibx_xtrabackup_log_copy_interval,
	 0, GET_LONG, REQUIRED_ARG, 1000, 0, LONG_MAX, 0, 1, 0},

	{"incremental-lsn", OPT_INCREMENTAL, "This option specifies the log "
	 "sequence number (LSN) to use for the incremental backup.  The option "
	 "accepts a string argument. It is used with the --incremental option. "
	 "It is used instead of specifying --incremental-basedir. For "
	 "databases created by MySQL and Percona Server 5.0-series versions, "
	 "specify the LSN as two 32-bit integers in high:low format. For "
	 "databases created in 5.1 and later, specify the LSN as a single "
	 "64-bit integer.",
	 (uchar*) &ibx_xtrabackup_incremental,
	 (uchar*) &ibx_xtrabackup_incremental,
	 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"parallel", OPT_PARALLEL, "On backup, this option specifies the "
	 "number of threads the xtrabackup child process should use to back "
	 "up files concurrently.  The option accepts an integer argument. It "
	 "is passed directly to xtrabackup's --parallel option. See the "
	 "xtrabackup documentation for details.",
	 (uchar*) &ibx_xtrabackup_parallel, (uchar*) &ibx_xtrabackup_parallel,
	 0, GET_INT, REQUIRED_ARG, 1, 1, INT_MAX, 0, 0, 0},

	{"rebuild-indexes", OPT_REBUILD_INDEXES,
	 "This option only has effect when used together with the --apply-log "
	 "option and is passed directly to xtrabackup. When used, makes "
	 "xtrabackup rebuild all secondary indexes after applying the log. "
	 "This option is normally used to prepare compact backups. See the "
	 "XtraBackup manual for more information.",
	 (uchar*) &ibx_xtrabackup_rebuild_indexes,
	 (uchar*) &ibx_xtrabackup_rebuild_indexes,
	 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"rebuild-threads", OPT_REBUILD_THREADS,
	 "Use this number of threads to rebuild indexes in a compact backup. "
	 "Only has effect with --prepare and --rebuild-indexes.",
	 (uchar*) &ibx_xtrabackup_rebuild_threads,
	 (uchar*) &ibx_xtrabackup_rebuild_threads,
	 0, GET_UINT, REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},

	{"stream", OPT_STREAM, "This option specifies the format in which to "
	 "do the streamed backup.  The option accepts a string argument. The "
	 "backup will be done to STDOUT in the specified format. Currently, "
	 "the only supported formats are tar and xbstream. This option is "
	 "passed directly to xtrabackup's --stream option.",
	 (uchar*) &ibx_xtrabackup_stream_str,
	 (uchar*) &ibx_xtrabackup_stream_str, 0, GET_STR,
	 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"tables-file", OPT_TABLES_FILE, "This option specifies the file in "
	 "which there are a list of names of the form database.  The option "
	 "accepts a string argument.table, one per line. The option is passed "
	 "directly to xtrabackup's --tables-file option.",
	 (uchar*) &ibx_xtrabackup_tables_file,
	 (uchar*) &ibx_xtrabackup_tables_file,
	 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"throttle", OPT_THROTTLE, "This option specifies a number of I/O "
	 "operations (pairs of read+write) per second.  It accepts an integer "
	 "argument.  It is passed directly to xtrabackup's --throttle option.",
	 (uchar*) &ibx_xtrabackup_throttle, (uchar*) &ibx_xtrabackup_throttle,
	 0, GET_LONG, REQUIRED_ARG, 0, 0, LONG_MAX, 0, 1, 0},

	{"tmpdir", 't', "This option specifies the location where a temporary "
	 "files will be stored. If the option is not specified, the default is "
	 "to use the value of tmpdir read from the server configuration.",
	 (uchar*) &ibx_opt_mysql_tmpdir,
	 (uchar*) &ibx_opt_mysql_tmpdir, 0, GET_STR, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"use-memory", OPT_USE_MEMORY, "This option accepts a string argument "
	 "that specifies the amount of memory in bytes for xtrabackup to use "
	 "for crash recovery while preparing a backup. Multiples are supported "
	 "providing the unit (e.g. 1MB, 1GB). It is used only with the option "
	 "--apply-log. It is passed directly to xtrabackup's --use-memory "
	 "option. See the xtrabackup documentation for details.",
	 (uchar*) &ibx_xtrabackup_use_memory,
	 (uchar*) &ibx_xtrabackup_use_memory,
	 0, GET_LL, REQUIRED_ARG, 100*1024*1024L, 1024*1024L, LONGLONG_MAX, 0,
	 1024*1024L, 0},

	{ 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage(void)
{
	puts("Open source backup tool for InnoDB and XtraDB\n\
\n\
Copyright (C) 2009-2015 Percona LLC and/or its affiliates.\n\
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
You can download full text of the license on http://www.gnu.org/licenses/gpl-2.0.txt\n\n");

	puts("innobackupex - Non-blocking backup tool for InnoDB, XtraDB and HailDB databases\n\
\n\
SYNOPOSIS\n\
\n\
innobackupex [--compress] [--compress-threads=NUMBER-OF-THREADS] [--compress-chunk-size=CHUNK-SIZE]\n\
             [--encrypt=ENCRYPTION-ALGORITHM] [--encrypt-threads=NUMBER-OF-THREADS] [--encrypt-chunk-size=CHUNK-SIZE]\n\
             [--encrypt-key=LITERAL-ENCRYPTION-KEY] | [--encryption-key-file=MY.KEY]\n\
             [--include=REGEXP] [--user=NAME]\n\
             [--password=WORD] [--port=PORT] [--socket=SOCKET]\n\
             [--no-timestamp] [--ibbackup=IBBACKUP-BINARY]\n\
             [--slave-info] [--galera-info] [--stream=tar|xbstream]\n\
             [--defaults-file=MY.CNF] [--defaults-group=GROUP-NAME]\n\
             [--databases=LIST] [--no-lock] \n\
             [--tmpdir=DIRECTORY] [--tables-file=FILE]\n\
             [--history=NAME]\n\
             [--incremental] [--incremental-basedir]\n\
             [--incremental-dir] [--incremental-force-scan] [--incremental-lsn]\n\
             [--incremental-history-name=NAME] [--incremental-history-uuid=UUID]\n\
             [--close-files] [--compact]     \n\
             BACKUP-ROOT-DIR\n\
\n\
innobackupex --apply-log [--use-memory=B]\n\
             [--defaults-file=MY.CNF]\n\
             [--export] [--redo-only] [--ibbackup=IBBACKUP-BINARY]\n\
             BACKUP-DIR\n\
\n\
innobackupex --copy-back [--defaults-file=MY.CNF] [--defaults-group=GROUP-NAME] BACKUP-DIR\n\
\n\
innobackupex --move-back [--defaults-file=MY.CNF] [--defaults-group=GROUP-NAME] BACKUP-DIR\n\
\n\
innobackupex [--decompress] [--decrypt=ENCRYPTION-ALGORITHM]\n\
             [--encrypt-key=LITERAL-ENCRYPTION-KEY] | [--encryption-key-file=MY.KEY]\n\
             [--parallel=NUMBER-OF-FORKS] BACKUP-DIR\n\
\n\
DESCRIPTION\n\
\n\
The first command line above makes a hot backup of a MySQL database.\n\
By default it creates a backup directory (named by the current date\n\
	and time) in the given backup root directory.  With the --no-timestamp\n\
option it does not create a time-stamped backup directory, but it puts\n\
the backup in the given directory (which must not exist).  This\n\
command makes a complete backup of all MyISAM and InnoDB tables and\n\
indexes in all databases or in all of the databases specified with the\n\
--databases option.  The created backup contains .frm, .MRG, .MYD,\n\
.MYI, .MAD, .MAI, .TRG, .TRN, .ARM, .ARZ, .CSM, CSV, .opt, .par, and\n\
InnoDB data and log files.  The MY.CNF options file defines the\n\
location of the database.  This command connects to the MySQL server\n\
using the mysql client program, and runs xtrabackup as a child\n\
process.\n\
\n\
The --apply-log command prepares a backup for starting a MySQL\n\
server on the backup. This command recovers InnoDB data files as specified\n\
in BACKUP-DIR/backup-my.cnf using BACKUP-DIR/xtrabackup_logfile,\n\
and creates new InnoDB log files as specified in BACKUP-DIR/backup-my.cnf.\n\
The BACKUP-DIR should be the path to a backup directory created by\n\
xtrabackup. This command runs xtrabackup as a child process, but it does not \n\
connect to the database server.\n\
\n\
The --copy-back command copies data, index, and log files\n\
from the backup directory back to their original locations.\n\
The MY.CNF options file defines the original location of the database.\n\
The BACKUP-DIR is the path to a backup directory created by xtrabackup.\n\
\n\
The --move-back command is similar to --copy-back with the only difference that\n\
it moves files to their original locations rather than copies them. As this\n\
option removes backup files, it must be used with caution. It may be useful in\n\
cases when there is not enough free disk space to copy files.\n\
\n\
The --decompress --decrypt command will decrypt and/or decompress a backup made\n\
with the --compress and/or --encrypt options. When decrypting, the encryption\n\
algorithm and key used when the backup was taken MUST be provided via the\n\
specified options. --decrypt and --decompress may be used together at the same\n\
time to completely normalize a previously compressed and encrypted backup. The\n\
--parallel option will allow multiple files to be decrypted and/or decompressed\n\
simultaneously. In order to decompress, the qpress utility MUST be installed\n\
and accessable within the path. This process will remove the original\n\
compressed/encrypted files and leave the results in the same location.\n\
\n\
On success the exit code innobackupex is 0. A non-zero exit code \n\
indicates an error.\n");
	printf("Usage: [%s [--defaults-file=#] --backup | %s [--defaults-file=#] --prepare] [OPTIONS]\n", my_progname, my_progname);
	my_print_help(ibx_long_options);
}


static
my_bool
ibx_get_one_option(int optid,
		const struct my_option *opt __attribute__((unused)),
		char *argument)
{
	switch(optid) {
	case '?':
		usage();
		exit(0);
		break;
	case 'v':
		msg("innobackupex version %s %s (%s) (revision id: %s)\n",
			XTRABACKUP_VERSION,
			SYSTEM_TYPE, MACHINE_TYPE, XTRABACKUP_REVISION);
		exit(0);
		break;
	case OPT_HISTORY:
		if (argument) {
			opt_ibx_history = argument;
		} else {
			opt_ibx_history = "";
		}
		break;
	case OPT_DECRYPT:
		if (argument == NULL) {
			ibx_msg("Missing --decrypt argument, must specify a "
				"valid encryption  algorithm.\n");
			return(1);
		}
		opt_ibx_decrypt = true;
		break;
	case OPT_STREAM:
		if (!strcasecmp(argument, "tar"))
			xtrabackup_stream_fmt = XB_STREAM_FMT_TAR;
		else if (!strcasecmp(argument, "xbstream"))
			xtrabackup_stream_fmt = XB_STREAM_FMT_XBSTREAM;
		else {
			ibx_msg("Invalid --stream argument: %s\n", argument);
			return 1;
		}
		xtrabackup_stream = TRUE;
		break;
	case OPT_COMPRESS:
		if (argument == NULL)
			xtrabackup_compress_alg = "quicklz";
		else if (strcasecmp(argument, "quicklz"))
		{
			ibx_msg("Invalid --compress argument: %s\n", argument);
			return 1;
		}
		xtrabackup_compress = TRUE;
		break;
	case OPT_ENCRYPT:
		if (argument == NULL)
		{
			msg("Missing --encrypt argument, must specify a "
				"valid encryption algorithm.\n");
			return 1;
		}
		xtrabackup_encrypt = TRUE;
		break;
	}
	return(0);
}

static
MYSQL *
ibx_mysql_connect()
{
	MYSQL *connection = mysql_init(NULL);

	if (connection == NULL) {
		ibx_msg("Failed to init MySQL struct: %s.\n",
			mysql_error(connection));
		return(NULL);
	}

	ibx_msg("Connecting to MySQL server host: %s, user: %s, password: %s, "
		"port: %d, socket: %s\n", opt_ibx_host, opt_ibx_user,
		opt_ibx_password ? "set" : "not set",
		opt_ibx_port, opt_ibx_socket);

	if (!mysql_real_connect(connection,
				opt_ibx_host ? opt_ibx_host : "localhost",
				opt_ibx_user,
				opt_ibx_password,
				"" /*database*/, opt_ibx_port,
				opt_ibx_socket, 0)) {
		ibx_msg("Failed to connect to MySQL server: %s.\n",
			mysql_error(connection));
		mysql_close(connection);
		return(NULL);
	}

	return(connection);
}

/*********************************************************************//**
Execute mysql query. */
static
MYSQL_RES *
ibx_mysql_query(MYSQL *connection, const char *query, bool use_result,
		bool die_on_error = true)
{
	MYSQL_RES *mysql_result = NULL;

	if (mysql_query(connection, query)) {
		ibx_msg("Error: failed to execute query %s: %s\n", query,
			mysql_error(connection));
		if (die_on_error) {
			exit(EXIT_FAILURE);
		}
		return(NULL);
	}

	/* store result set on client if there is a result */
	if (mysql_field_count(connection) > 0) {
		if ((mysql_result = mysql_store_result(connection)) == NULL) {
			ibx_msg("Error: failed to fetch query result %s: %s\n",
				query, mysql_error(connection));
			exit(EXIT_FAILURE);
		}

		if (!use_result) {
			mysql_free_result(mysql_result);
		}
	}

	return mysql_result;
}


struct mysql_variable {
	const char *name;
	char **value;
};


static
void
read_mysql_variables(MYSQL *connection, const char *query, mysql_variable *vars,
	bool vertical_result)
{
	MYSQL_RES *mysql_result;
	MYSQL_ROW row;
	mysql_variable *var;

	mysql_result = ibx_mysql_query(connection, query, true);

	ut_ad(!vertical_result || mysql_num_fields(mysql_result) == 2);

	if (vertical_result) {
		while ((row = mysql_fetch_row(mysql_result))) {
			char *name = row[0];
			char *value = row[1];
			for (var = vars; var->name; var++) {
				if (strcmp(var->name, name) == 0
				    && value != NULL) {
					*(var->value) = strdup(value);
				}
			}
		}
	} else {
		MYSQL_FIELD *field;

		if ((row = mysql_fetch_row(mysql_result)) != NULL) {
			int i = 0;
			while ((field = mysql_fetch_field(mysql_result))
				!= NULL) {
				char *name = field->name;
				char *value = row[i];
				for (var = vars; var->name; var++) {
					if (strcmp(var->name, name) == 0
					    && value != NULL) {
						*(var->value) = strdup(value);
					}
				}
				++i;
			}
		}
	}

	mysql_free_result(mysql_result);
}


static
void
free_mysql_variables(mysql_variable *vars)
{
	mysql_variable *var;

	for (var = vars; var->name; var++) {
		free(*(var->value));
	}
}


static
char *
read_mysql_one_value(MYSQL *connection, const char *query)
{
	MYSQL_RES *mysql_result;
	MYSQL_ROW row;
	char *result = NULL;

	mysql_result = ibx_mysql_query(connection, query, true);

	ut_ad(mysql_num_fields(mysql_result) == 1);

	if ((row = mysql_fetch_row(mysql_result))) {
		result = strdup(row[0]);
	}

	mysql_free_result(mysql_result);

	return(result);
}

static
bool
check_server_version(const char *version, const char *innodb_version)
{
	if (!((fnmatch("5.[123].*", version, FNM_PATHNAME) == 0
	       && innodb_version != NULL)
	      || (fnmatch("5.5.*", version, FNM_PATHNAME) == 0)
	      || (fnmatch("5.6.*", version, FNM_PATHNAME) == 0)
	      || (fnmatch("10.[01].*", version, FNM_PATHNAME) == 0))) {
		if (fnmatch("5.1.*", version, FNM_PATHNAME) == 0
		    && innodb_version == NULL) {
			ibx_msg("Error: Built-in InnoDB in MySQL 5.1 is not "
				"supported in this release. You can either use "
				"Percona XtraBackup 2.0, or upgrade to InnoDB "
				"plugin.\n");
		} else {
			ibx_msg("Error: Unsupported server version: "
				"'%s'. Please report a bug at "
				"https://bugs.launchpad.net/"
				"percona-xtrabackup\n", version);
		}
		return(false);
	}

	return(true);
}


/*********************************************************************//**
Receive options important for XtraBackup from MySQL server.
@return	true on success. */
static
bool
get_mysql_vars(MYSQL *connection)
{
	char *gtid_mode_var = NULL;
	char *version_var = NULL;
	char *innodb_version_var = NULL;
	char *have_backup_locks_var = NULL;
	char *lock_wait_timeout_var= NULL;
	char *wsrep_on_var = NULL;
	char *slave_parallel_workers_var = NULL;
	char *gtid_slave_pos_var = NULL;
	char *innodb_buffer_pool_filename_var = NULL;
	char *datadir_var = NULL;
	char *innodb_log_file_size_var = NULL;
	char *innodb_data_file_path_var = NULL;

	bool ret = true;

	mysql_variable mysql_vars[] = {
		{"have_backup_locks", &have_backup_locks_var},
		{"lock_wait_timeout", &lock_wait_timeout_var},
		{"gtid_mode", &gtid_mode_var},
		{"version", &version_var},
		{"innodb_version", &innodb_version_var},
		{"wsrep_on", &wsrep_on_var},
		{"slave_parallel_workers", &slave_parallel_workers_var},
		{"gtid_slave_pos", &gtid_slave_pos_var},
		{"innodb_buffer_pool_filename",
			&innodb_buffer_pool_filename_var},
		{"datadir", &datadir_var},
		{"innodb_log_file_size", &innodb_log_file_size_var},
		{"innodb_data_file_path", &innodb_data_file_path_var},
		{NULL, NULL}
	};

	read_mysql_variables(connection, "SHOW VARIABLES",
				mysql_vars, true);

	if (have_backup_locks_var != NULL && !opt_ibx_no_backup_locks) {
		have_backup_locks = true;
	}

	if (lock_wait_timeout_var != NULL) {
		have_lock_wait_timeout = true;
	}

	if (wsrep_on_var != NULL) {
		have_galera_enabled = true;
	}

	if (strcmp(version_var, "5.5") >= 0) {
		have_flush_engine_logs = true;
	}

	if (slave_parallel_workers_var != NULL
		&& atoi(slave_parallel_workers_var) > 0) {
		have_multi_threaded_slave = true;
	}

	if (innodb_buffer_pool_filename_var != NULL) {
		buffer_pool_filename = strdup(innodb_buffer_pool_filename_var);
	}

	if ((gtid_mode_var && strcmp(gtid_mode_var, "ON") == 0) ||
	    (gtid_slave_pos_var && *gtid_slave_pos_var)) {
		have_gtid_slave = true;
	}

	if (!(ret = check_server_version(version_var, innodb_version_var))) {
		goto out;
	}

	ibx_msg("Using server version %s\n", version_var);

	/* make sure datadir value is the same in configuration file */
	if (mysql_data_home != NULL && datadir_var != NULL) {
		if (!(ret = equal_paths(mysql_data_home, datadir_var))) {
			ibx_msg("Error: option 'datadir' has different "
				"values:\n"
				"  '%s' in defaults file\n"
				"  '%s' in SHOW VARIABLES\n",
				mysql_data_home, datadir_var);
			goto out;
		}
	}

	/* get some default values is they are missing from my.cnf */
	if (!xtrabackup_innodb_log_file_size_explicit) {
		char *endptr;

		innobase_log_file_size = strtoll(innodb_log_file_size_var,
							&endptr, 10);
		ut_ad(*endptr == 0);
	}

	if (!xtrabackup_innodb_data_file_path_explicit) {
		innobase_data_file_path = innobase_data_file_path_alloc
					= strdup(innodb_data_file_path_var);
	}

out:
	free_mysql_variables(mysql_vars);

	return(ret);
}

/*********************************************************************//**
Query the server to find out what backup capabilities it supports.
@return	true on success. */
static
bool
detect_mysql_capabilities_for_backup()
{
	const char *query = "SELECT 'INNODB_CHANGED_PAGES', COUNT(*) FROM "
				"INFORMATION_SCHEMA.PLUGINS "
			    "WHERE PLUGIN_NAME LIKE 'INNODB_CHANGED_PAGES'";
	char *innodb_changed_pages = NULL;
	mysql_variable vars[] = {
		{"INNODB_CHANGED_PAGES", &innodb_changed_pages}, {NULL, NULL}};

	if (opt_ibx_incremental) {

		read_mysql_variables(mysql_connection, query, vars, true);

		ut_ad(innodb_changed_pages != NULL);

		have_changed_page_bitmaps = (atoi(innodb_changed_pages) == 1);

		free_mysql_variables(vars);
	}

	/* do some sanity checks */
	if (opt_ibx_galera_info && !have_galera_enabled) {
		ibx_msg("--galera-info is specified on the command "
		 	"line, but the server does not support Galera "
		 	"replication. Ignoring the option.\n");
		opt_ibx_galera_info = false;
	}

	if (opt_ibx_slave_info && have_multi_threaded_slave &&
	    !have_gtid_slave) {
	    	ibx_msg("The --slave-info option requires GTID enabled for a "
			"multi-threaded slave.\n");
		return(false);
	}

	return(true);
}

static
bool
select_incremental_lsn_from_history(lsn_t *incremental_lsn)
{
	MYSQL_RES *mysql_result;
	MYSQL_ROW row;
	char query[1000];
	char buf[100];

	if (opt_ibx_incremental_history_name) {
		mysql_real_escape_string(mysql_connection, buf,
				opt_ibx_incremental_history_name,
				strlen(opt_ibx_incremental_history_name));
		ut_snprintf(query, sizeof(query),
			"SELECT innodb_to_lsn "
			"FROM PERCONA_SCHEMA.xtrabackup_history "
			"WHERE name = '%s' "
			"AND innodb_to_lsn IS NOT NULL "
			"ORDER BY innodb_to_lsn DESC LIMIT 1",
			buf);
	}

	if (opt_ibx_incremental_history_uuid) {
		mysql_real_escape_string(mysql_connection, buf,
				opt_ibx_incremental_history_uuid,
				strlen(opt_ibx_incremental_history_uuid));
		ut_snprintf(query, sizeof(query),
			"SELECT innodb_to_lsn "
			"FROM PERCONA_SCHEMA.xtrabackup_history "
			"WHERE uuid = '%s' "
			"AND innodb_to_lsn IS NOT NULL "
			"ORDER BY innodb_to_lsn DESC LIMIT 1",
			buf);
	}

	mysql_result = ibx_mysql_query(mysql_connection, query, true);

	ut_ad(mysql_num_fields(mysql_result) == 1);
	if (!(row = mysql_fetch_row(mysql_result))) {
		ibx_msg("Error while attempting to find history record "
			"for %s %s\n",
			opt_ibx_incremental_history_uuid ? "uuid" : "name",
			opt_ibx_incremental_history_uuid ?
		    		opt_ibx_incremental_history_uuid :
		    		opt_ibx_incremental_history_name);
		return(false);
	}

	*incremental_lsn = strtoull(row[0], NULL, 10);

	mysql_free_result(mysql_result);

	ibx_msg("Found and using lsn: " LSN_PF " for %s %s\n", *incremental_lsn,
		opt_ibx_incremental_history_uuid ? "uuid" : "name",
		opt_ibx_incremental_history_uuid ?
	    		opt_ibx_incremental_history_uuid :
	    		opt_ibx_incremental_history_name);

	return(true);
}

static
const char *
eat_sql_whitespace(const char *query)
{
	bool comment = false;

	while (*query) {
		if (comment) {
			if (query[0] == '*' && query[1] == '/') {
				query += 2;
				comment = false;
				continue;
			}
			++query;
			continue;
		}
		if (query[0] == '/' && query[1] == '*') {
			query += 2;
			comment = true;
			continue;
		}
		if (strchr("\t\n\r (", query[0])) {
			++query;
			continue;
		}
		break;
	}

	return(query);
}

static
bool
is_query_from_list(const char *query, const char **list)
{
	const char **item;

	query = eat_sql_whitespace(query);

	item = list;
	while (*item) {
		if (strncasecmp(query, *item, strlen(*item)) == 0) {
			return(true);
		}
		++item;
	}

	return(false);
}

static
bool
is_query(const char *query)
{
	const char *query_list[] = {"insert", "update", "delete", "replace",
		"alter", "load", "select", "do", "handler", "call", "execute",
		"begin", NULL};

	return is_query_from_list(query, query_list);
}

static
bool
is_select_query(const char *query)
{
	const char *query_list[] = {"select", NULL};

	return is_query_from_list(query, query_list);
}

static
bool
is_update_query(const char *query)
{
	const char *query_list[] = {"insert", "update", "delete", "replace",
					"alter", "load", NULL};

	return is_query_from_list(query, query_list);
}

static
bool
have_queries_to_wait_for(MYSQL *connection, uint threshold)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	bool all_queries;

	result = ibx_mysql_query(connection, "SHOW FULL PROCESSLIST", true);

	all_queries = (opt_ibx_lock_wait_query_type == IBX_QUERY_TYPE_ALL);
	while ((row = mysql_fetch_row(result)) != NULL) {
		const char	*info		= row[7];
		int		duration	= atoi(row[5]);
		char		*id		= row[0];

		if (info != NULL
		    && duration >= (int)threshold
		    && ((all_queries && is_query(info))
		    	|| is_update_query(info))) {
			ibx_msg("Waiting for query %s (duration %d sec): %s",
				id, duration, info);
			return(true);
		}
	}

	return(false);
}

static
void
kill_long_queries(MYSQL *connection, uint timeout)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	bool all_queries;
	char kill_stmt[100];

	result = ibx_mysql_query(connection, "SHOW FULL PROCESSLIST", true);

	all_queries = (opt_ibx_kill_long_query_type == IBX_QUERY_TYPE_ALL);
	while ((row = mysql_fetch_row(result)) != NULL) {
		const char	*info		= row[7];
		int		duration	= atoi(row[5]);
		char		*id		= row[0];

		if (info != NULL &&
		    duration >= (int)timeout &&
		    ((all_queries && is_query(info)) ||
		    	is_select_query(info))) {
			ibx_msg("Killing query %s (duration %d sec): %s\n",
				id, duration, info);
			ut_snprintf(kill_stmt, sizeof(kill_stmt),
				    "KILL %s", id);
			ibx_mysql_query(connection, kill_stmt, false, false);
		}
	}
}

static
bool
wait_for_no_updates(MYSQL *connection, uint timeout, uint threshold)
{
	time_t	start_time;

	start_time = time(NULL);

	ibx_msg("Waiting %u seconds for queries running longer than %u seconds "
		"to finish\n", timeout, threshold);

	while (time(NULL) <= (time_t)(start_time + timeout)) {
		if (!have_queries_to_wait_for(connection, threshold)) {
			return(true);
		}
		os_thread_sleep(1000000);
	}

	ibx_msg("Unable to obtain lock. Please try again later.");

	return(false);
}

static
os_thread_ret_t
kill_query_thread(
/*===============*/
	void *arg __attribute__((unused)))
{
	MYSQL	*mysql;
	time_t	start_time;

	start_time = time(NULL);

	os_event_set(kill_query_thread_started);

	ibx_msg("Kill query timeout %d seconds.\n",
		opt_ibx_kill_long_queries_timeout);

	while (time(NULL) - start_time <
				(time_t)opt_ibx_kill_long_queries_timeout) {
		if (os_event_wait_time(kill_query_thread_stop, 1000) !=
		    OS_SYNC_TIME_EXCEEDED) {
			goto stop_thread;
		}
	}

	if ((mysql = ibx_mysql_connect()) == NULL) {
		ibx_msg("Error: kill query thread failed\n");
		goto stop_thread;
	}

	while (true) {
		kill_long_queries(mysql, time(NULL) - start_time);
		if (os_event_wait_time(kill_query_thread_stop, 1000) !=
		    OS_SYNC_TIME_EXCEEDED) {
			break;
		}
	}

	mysql_close(mysql);

stop_thread:
	ibx_msg("Kill query thread stopped\n");

	os_event_set(kill_query_thread_stopped);

	os_thread_exit(NULL);
	OS_THREAD_DUMMY_RETURN;
}


static
void
start_query_killer()
{
	kill_query_thread_stop		= os_event_create();
	kill_query_thread_started	= os_event_create();
	kill_query_thread_stopped	= os_event_create();

	os_thread_create(kill_query_thread, NULL, &kill_query_thread_id);

	os_event_wait(kill_query_thread_started);
}

static
void
stop_query_killer()
{
	os_event_set(kill_query_thread_stop);
	os_event_wait_time(kill_query_thread_stopped, 60000);
}

/*********************************************************************//**
Function acquires either a backup tables lock, if supported
by the server, or a global read lock (FLUSH TABLES WITH READ LOCK)
otherwise.
@returns true if lock acquired */
static
bool
ibx_lock_tables(MYSQL *connection)
{
	if (have_lock_wait_timeout) {
		/* Set the maximum supported session value for
		lock_wait_timeout to prevent unnecessary timeouts when the
		global value is changed from the default */
		ibx_mysql_query(connection,
			"SET SESSION lock_wait_timeout=31536000", false);
	}

	if (have_backup_locks) {
		ibx_mysql_query(connection, "LOCK TABLES FOR BACKUP", false);
		return(true);
	}

	if (opt_ibx_lock_wait_timeout) {
		if (!wait_for_no_updates(connection, opt_ibx_lock_wait_timeout,
					 opt_ibx_lock_wait_threshold)) {
			return(false);
		}
	}

	ibx_msg("Executing FLUSH TABLES WITH READ LOCK...\n");

	if (opt_ibx_kill_long_queries_timeout) {
		start_query_killer();
	}

	if (have_galera_enabled) {
		ibx_mysql_query(connection,
				"SET SESSION wsrep_causal_reads=0", false);
	}

	ibx_mysql_query(connection, "FLUSH TABLES WITH READ LOCK", false);

	if (opt_ibx_kill_long_queries_timeout) {
		stop_query_killer();
	}

	return(true);
}


/*********************************************************************//**
If backup locks are used, execete LOCK BINLOG FOR BACKUP.
@returns true if lock acquired */
static
bool
ibx_lock_binlog(MYSQL *connection)
{
	if (have_backup_locks) {
		ibx_msg("Executing LOCK BINLOG FOR BACKUP...\n");
		ibx_mysql_query(connection, "LOCK BINLOG FOR BACKUP", false);
		return(true);
	}
	return(true);
}


/*********************************************************************//**
Releases either global read lock acquired with FTWRL and the binlog
lock acquired with LOCK BINLOG FOR BACKUP, depending on
the locking strategy being used */
static
void
ibx_unlock_all(MYSQL *connection)
{
	if (opt_ibx_debug_sleep_before_unlock) {
		ibx_msg("Debug sleep for %u seconds\n",
			opt_ibx_debug_sleep_before_unlock);
		os_thread_sleep(opt_ibx_debug_sleep_before_unlock * 1000);
	}

	if (have_backup_locks) {
		ibx_msg("Executing UNLOCK BINLOG\n");
		ibx_mysql_query(connection, "UNLOCK BINLOG", false);
	}

	ibx_msg("Executing UNLOCK TABLES\n");
	ibx_mysql_query(connection, "UNLOCK TABLES", false);

	ibx_msg("All tables unlocked\n");
}


static
int
get_open_temp_tables(MYSQL *connection)
{
	char *slave_open_temp_tables = NULL;
	mysql_variable status[] = {
		{"Slave_open_temp_tables", &slave_open_temp_tables},
		{NULL, NULL}
	};
	int result = false;

	read_mysql_variables(connection,
		"SHOW STATUS LIKE 'slave_open_temp_tables'", status, true);

	result = slave_open_temp_tables ? atoi(slave_open_temp_tables) : 0;

	free_mysql_variables(status);

	return(result);
}

/*********************************************************************//**
Wait until it's safe to backup a slave.  Returns immediately if
the host isn't a slave.  Currently there's only one check:
Slave_open_temp_tables has to be zero.  Dies on timeout. */
static
bool
wait_for_safe_slave(MYSQL *connection)
{
	char *read_master_log_pos = NULL;
	char *slave_sql_running = NULL;
	int n_attempts = 1;
	const int sleep_time = 3;
	int open_temp_tables = 0;
	bool result = true;

	mysql_variable status[] = {
		{"Read_Master_Log_Pos", &read_master_log_pos},
		{"Slave_SQL_Running", &slave_sql_running},
		{NULL, NULL}
	};

	sql_thread_started = false;

	read_mysql_variables(connection, "SHOW SLAVE STATUS", status, false);

	if (!(read_master_log_pos && slave_sql_running)) {
		ibx_msg("Not checking slave open temp tables for "
			"--safe-slave-backup because host is not a slave\n");
		goto cleanup;
	}

	if (strcmp(slave_sql_running, "Yes") == 0) {
		sql_thread_started = true;
		ibx_mysql_query(connection, "STOP SLAVE SQL_THREAD", false);
	}

	if (opt_ibx_safe_slave_backup_timeout > 0) {
		n_attempts = opt_ibx_safe_slave_backup_timeout / sleep_time;
	}

	open_temp_tables = get_open_temp_tables(connection);
	ibx_msg("Slave open temp tables: %d\n", open_temp_tables);

	while (open_temp_tables && n_attempts--) {
		ibx_msg("Starting slave SQL thread, waiting %d seconds, then "
			"checking Slave_open_temp_tables again (%d attempts "
			"remaining)...\n", sleep_time, n_attempts);

		ibx_mysql_query(connection, "START SLAVE SQL_THREAD", false);
		os_thread_sleep(sleep_time * 1000);
		ibx_mysql_query(connection, "STOP SLAVE SQL_THREAD", false);

		open_temp_tables = get_open_temp_tables(connection);
		ibx_msg("Slave open temp tables: %d\n", open_temp_tables);
	}

	/* Restart the slave if it was running at start */
	if (open_temp_tables == 0) {
		ibx_msg("Slave is safe to backup\n");
		goto cleanup;
	}

	result = false;

	if (sql_thread_started) {
		ibx_msg("Restarting slave SQL thread.\n");
		ibx_mysql_query(connection, "START SLAVE SQL_THREAD", false);
	}

	ibx_msg("Slave_open_temp_tables did not become zero after "
	    "%d seconds\n", opt_ibx_safe_slave_backup_timeout);

cleanup:
	free_mysql_variables(status);

	return(result);
}


/*********************************************************************//**
Retrieves MySQL binlog position of the master server in a replication
setup and saves it in a file. It also saves it in mysql_slave_position
variable. */
static
bool
write_slave_info(MYSQL *connection)
{
	char *master = NULL;
	char *filename = NULL;
	char *gtid_executed = NULL;
	char *position = NULL;
	char *gtid_slave_pos = NULL;
	char *ptr;
	bool result = false;

	mysql_variable status[] = {
		{"Master_Host", &master},
		{"Relay_Master_Log_File", &filename},
		{"Exec_Master_Log_Pos", &position},
		{"Executed_Gtid_Set", &gtid_executed},
		{NULL, NULL}
	};

	mysql_variable variables[] = {
		{"gtid_slave_pos", &gtid_slave_pos},
		{NULL, NULL}
	};

	read_mysql_variables(connection, "SHOW SLAVE STATUS", status, false);
	read_mysql_variables(connection, "SHOW VARIABLES", variables, true);

	if (master == NULL || filename == NULL || position == NULL) {
		ibx_msg("Failed to get master binlog coordinates "
			"from SHOW SLAVE STATUS\n");
		ibx_msg("This means that the server is not a "
			"replication slave. Ignoring the --slave-info "
			"option\n");
		goto cleanup;
	}

	/* Print slave status to a file.
	If GTID mode is used, construct a CHANGE MASTER statement with
	MASTER_AUTO_POSITION and correct a gtid_purged value. */
	if (gtid_executed != NULL && *gtid_executed) {
		/* MySQL >= 5.6 with GTID enabled */

		for (ptr = strchr(gtid_executed, '\n');
		     ptr;
		     ptr = strchr(ptr, '\n')) {
			*ptr = ' ';
		}

		result = backup_file_printf(XTRABACKUP_SLAVE_INFO,
			"SET GLOBAL gtid_purged='%s';\n"
			"CHANGE MASTER TO MASTER_AUTO_POSITION=1\n",
			gtid_executed);

		ut_a(asprintf(&mysql_slave_position,
			"master host '%s', purge list '%s'",
			master, gtid_executed) != -1);
	} else if (gtid_slave_pos && *gtid_slave_pos) {
		/* MariaDB >= 10.0 with GTID enabled */
		result = backup_file_printf(XTRABACKUP_SLAVE_INFO,
			"CHANGE MASTER TO master_use_gtid = slave_pos\n");
		ut_a(asprintf(&mysql_slave_position,
			"master host '%s', gtid_slave_pos %s",
			master, gtid_slave_pos) != -1);
	} else {
		result = backup_file_printf(XTRABACKUP_SLAVE_INFO,
			"CHANGE MASTER TO MASTER_LOG_FILE='%s', "
			"MASTER_LOG_POS=%s\n", filename, position);
		ut_a(asprintf(&mysql_slave_position,
			"master host '%s', filename '%s', position '%s",
			master, filename, position) != -1);
	}

cleanup:
	free_mysql_variables(status);
	free_mysql_variables(variables);

	return(result);
}


/*********************************************************************//**
Retrieves MySQL Galera and
saves it in a file. It also prints it to stdout. */
static
bool
write_galera_info(MYSQL *connection)
{
	char *state_uuid = NULL, *state_uuid55 = NULL;
	char *last_committed = NULL, *last_committed55 = NULL;
	bool result;

	mysql_variable status[] = {
		{"Wsrep_local_state_uuid", &state_uuid},
		{"wsrep_local_state_uuid", &state_uuid55},
		{"Wsrep_last_committed", &last_committed},
		{"wsrep_last_committed", &last_committed55},
		{NULL, NULL}
	};

	/* When backup locks are supported by the server, we should skip
	creating xtrabackup_galera_info file on the backup stage, because
	wsrep_local_state_uuid and wsrep_last_committed will be inconsistent
	without blocking commits. The state file will be created on the prepare
	stage using the WSREP recovery procedure. */
	if (have_backup_locks) {
		return(true);
	}

	read_mysql_variables(connection, "SHOW STATUS", status, true);

	if ((state_uuid == NULL && state_uuid55 == NULL)
		|| (last_committed == NULL && last_committed55 == NULL)) {
		ibx_msg("Failed to get master wsrep state from SHOW STATUS.\n");
		result = false;
		goto cleanup;
	}

	result = backup_file_printf(XTRABACKUP_GALERA_INFO,
		"%s:%s\n", state_uuid ? state_uuid : state_uuid55,
			last_committed ? last_committed : last_committed55);

cleanup:
	free_mysql_variables(status);

	return(result);
}


/*********************************************************************//**
Flush and copy the current binary log file into the backup,
if GTID is enabled */
static
bool
write_current_binlog_file(MYSQL *connection)
{
	char *executed_gtid_set = NULL;
	char *gtid_binlog_state = NULL;
	char *log_bin_file = NULL;
	char *log_bin_dir = NULL;
	char *datadir = NULL;
	bool gtid_exists;
	bool result = true;
	char filepath[FN_REFLEN];

	mysql_variable status[] = {
		{"Executed_Gtid_Set", &executed_gtid_set},
		{NULL, NULL}
	};

	mysql_variable status_after_flush[] = {
		{"File", &log_bin_file},
		{NULL, NULL}
	};

	mysql_variable vars[] = {
		{"gtid_binlog_state", &gtid_binlog_state},
		{"log_bin_basename", &log_bin_dir},
		{"datadir", &datadir},
		{NULL, NULL}
	};

	read_mysql_variables(connection, "SHOW MASTER STATUS", status, false);
	read_mysql_variables(connection, "SHOW VARIABLES", vars, true);

	gtid_exists = (executed_gtid_set && *executed_gtid_set)
			|| (gtid_binlog_state && *gtid_binlog_state);

	if (gtid_exists) {
		size_t log_bin_dir_length;

		ibx_mysql_query(connection, "FLUSH BINARY LOGS", false);

		read_mysql_variables(connection, "SHOW MASTER STATUS",
			status_after_flush, false);

		if (log_bin_dir == NULL) {
			/* log_bin_basename does not exist in MariaDB,
			fallback to datadir */
			log_bin_dir = datadir;
		}

		dirname_part(log_bin_dir, log_bin_dir, &log_bin_dir_length);

		if (log_bin_dir == NULL || log_bin_file == NULL) {
			ibx_msg("Failed to get master binlog coordinates from "
				"SHOW MASTER STATUS");
			result = false;
			goto cleanup;
		}

		ut_snprintf(filepath, sizeof(filepath), "%s/%s",
				log_bin_dir, log_bin_file);
		result = copy_file(filepath, log_bin_file, 0);
	}

cleanup:
	free_mysql_variables(status_after_flush);
	free_mysql_variables(status);
	free_mysql_variables(vars);

	return(result);
}


/*********************************************************************//**
Retrieves MySQL binlog position and
saves it in a file. It also prints it to stdout. */
static
bool
write_binlog_info(MYSQL *connection)
{
	char *filename = NULL;
	char *position = NULL;
	char *gtid_mode = NULL;
	char *gtid_current_pos = NULL;
	char *gtid_executed = NULL;
	char *gtid = NULL;
	bool result;
	bool mysql_gtid;
	bool mariadb_gtid;

	mysql_variable status[] = {
		{"File", &filename},
		{"Position", &position},
		{"Executed_Gtid_Set", &gtid_executed},
		{NULL, NULL}
	};

	mysql_variable vars[] = {
		{"gtid_mode", &gtid_mode},
		{"gtid_current_pos", &gtid_current_pos},
		{NULL, NULL}
	};

	read_mysql_variables(connection, "SHOW MASTER STATUS", status, false);
	read_mysql_variables(connection, "SHOW VARIABLES", vars, true);

	if (filename == NULL || position == NULL) {
		/* Do not create xtrabackup_binlog_info if binary
		log is disabled */
		result = true;
		goto cleanup;
	}

	mysql_gtid = ((gtid_mode != NULL) && (strcmp(gtid_mode, "ON") == 0));
	mariadb_gtid = (gtid_current_pos != NULL);

	gtid = (gtid_executed != NULL ? gtid_executed : gtid_current_pos);

	if (mariadb_gtid) {
		ut_a(asprintf(&mysql_binlog_position,
			"filename '%s', position '%s', "
			"GTID of the last change '%s'",
			filename, position, gtid) != -1);
		result = backup_file_printf(XTRABACKUP_BINLOG_INFO,
				"%s\t%s\t%s", filename, position, gtid);
	} else if (mysql_gtid) {
		ut_a(asprintf(&mysql_binlog_position,
			"GTID of the last change '%s'", gtid) != -1);
		result = backup_file_printf(XTRABACKUP_BINLOG_INFO,
				"%s", gtid);
	} else {
		ut_a(asprintf(&mysql_binlog_position,
			"filename '%s', position '%s'",
			filename, position) != -1);
		result = backup_file_printf(XTRABACKUP_BINLOG_INFO,
				"%s\t%s", filename, position);
	}

cleanup:
	free_mysql_variables(status);
	free_mysql_variables(vars);

	return(result);
}



/*********************************************************************//**
Writes xtrabackup_info file and if backup_history is enable creates
PERCONA_SCHEMA.xtrabackup_history and writes a new history record to the
table containing all the history info particular to the just completed
backup. */
static
bool
write_xtrabackup_info(MYSQL *connection)
{
	MYSQL_STMT *stmt;
	MYSQL_BIND bind[19];
	char *uuid = NULL;
	char *server_version = NULL;
	char buf_start_time[100];
	char buf_end_time[100];
	int idx;
	tm tm;
	my_bool null = TRUE;

	const char *xb_stream_name[] = {"file", "tar", "xbstream"};
	const char *ins_query = "insert into PERCONA_SCHEMA.xtrabackup_history("
		"uuid, name, tool_name, tool_command, tool_version, "
		"ibbackup_version, server_version, start_time, end_time, "
		"lock_time, binlog_pos, innodb_from_lsn, innodb_to_lsn, "
		"partial, incremental, format, compact, compressed, "
		"encrypted) "
		"values(?,?,?,?,?,?,?,from_unixtime(?),from_unixtime(?),"
		"?,?,?,?,?,?,?,?,?,?)";

	ut_ad(xtrabackup_stream_fmt < 3);

	uuid = read_mysql_one_value(connection, "SELECT UUID()");
	server_version = read_mysql_one_value(connection, "SELECT VERSION()");
	localtime_r(&history_start_time, &tm);
	strftime(buf_start_time, sizeof(buf_start_time),
		 "%Y-%m-%d %H:%M:%S", &tm);
	history_end_time = time(NULL);
	localtime_r(&history_end_time, &tm);
	strftime(buf_end_time, sizeof(buf_end_time),
		 "%Y-%m-%d %H:%M:%S", &tm);
	backup_file_printf(XTRABACKUP_INFO,
		"uuid = %s\n"
		"name = %s\n"
		"tool_name = %s\n"
		"tool_command = %s\n"
		"tool_version = %s\n"
		"ibbackup_version = %s\n"
		"server_version = %s\n"
		"start_time = %s\n"
		"end_time = %s\n"
		"lock_time = %d\n"
		"binlog_pos = %s\n"
		"innodb_from_lsn = %llu\n"
		"innodb_to_lsn = %llu\n"
		"partial = %s\n"
		"incremental = %s\n"
		"format = %s\n"
		"compact = %s\n"
		"compressed = %s\n"
		"encrypted = %s\n",
		uuid, /* uuid */
		opt_ibx_history ? opt_ibx_history : "",  /* name */
		tool_name,  /* tool_name */
		tool_args,  /* tool_command */
		XTRABACKUP_VERSION,  /* tool_version */
		XTRABACKUP_VERSION,  /* ibbackup_version */
		server_version,  /* server_version */
		buf_start_time,  /* start_time */
		buf_end_time,  /* end_time */
		history_lock_time, /* lock_time */
		mysql_binlog_position,  /* binlog_pos */
		incremental_lsn, /* innodb_from_lsn */
		metadata_to_lsn, /* innodb_to_lsn */
		(xtrabackup_tables /* partial */
		 || xtrabackup_tables_file
		 || xtrabackup_databases
		 || xtrabackup_databases_file) ? "Y" : "N",
		opt_ibx_incremental ? "Y" : "N", /* incremental */
		xb_stream_name[xtrabackup_stream_fmt], /* format */
		xtrabackup_compact ? "Y" : "N", /* compact */
		xtrabackup_compress ? "compressed" : "N", /* compressed */
		xtrabackup_encrypt ? "Y" : "N"); /* encrypted */

	if (!opt_ibx_history) {
		goto cleanup;
	}

	ibx_mysql_query(connection,
		"CREATE DATABASE IF NOT EXISTS PERCONA_SCHEMA", false);
	ibx_mysql_query(connection,
		"CREATE TABLE IF NOT EXISTS PERCONA_SCHEMA.xtrabackup_history("
		"uuid VARCHAR(40) NOT NULL PRIMARY KEY,"
		"name VARCHAR(255) DEFAULT NULL,"
		"tool_name VARCHAR(255) DEFAULT NULL,"
		"tool_command TEXT DEFAULT NULL,"
		"tool_version VARCHAR(255) DEFAULT NULL,"
		"ibbackup_version VARCHAR(255) DEFAULT NULL,"
		"server_version VARCHAR(255) DEFAULT NULL,"
		"start_time TIMESTAMP NULL DEFAULT NULL,"
		"end_time TIMESTAMP NULL DEFAULT NULL,"
		"lock_time BIGINT UNSIGNED DEFAULT NULL,"
		"binlog_pos VARCHAR(128) DEFAULT NULL,"
		"innodb_from_lsn BIGINT UNSIGNED DEFAULT NULL,"
		"innodb_to_lsn BIGINT UNSIGNED DEFAULT NULL,"
		"partial ENUM('Y', 'N') DEFAULT NULL,"
		"incremental ENUM('Y', 'N') DEFAULT NULL,"
		"format ENUM('file', 'tar', 'xbstream') DEFAULT NULL,"
		"compact ENUM('Y', 'N') DEFAULT NULL,"
		"compressed ENUM('Y', 'N') DEFAULT NULL,"
		"encrypted ENUM('Y', 'N') DEFAULT NULL"
		") CHARACTER SET utf8 ENGINE=innodb", false);

	stmt = mysql_stmt_init(connection);

	mysql_stmt_prepare(stmt, ins_query, strlen(ins_query));

	memset(bind, 0, sizeof(bind));
	idx = 0;

	/* uuid */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = uuid;
	bind[idx].buffer_length = strlen(uuid);
	++idx;

	/* name */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(opt_ibx_history);
	bind[idx].buffer_length = strlen(opt_ibx_history);
	if (!(opt_ibx_history && *opt_ibx_history)) {
		bind[idx].is_null = &null;
	}
	++idx;

	/* tool_name */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = tool_name;
	bind[idx].buffer_length = strlen(tool_name);
	++idx;

	/* tool_command */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = tool_args;
	bind[idx].buffer_length = strlen(tool_args);
	++idx;

	/* tool_version */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(XTRABACKUP_VERSION);
	bind[idx].buffer_length = strlen(XTRABACKUP_VERSION);
	++idx;

	/* ibbackup_version */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(XTRABACKUP_VERSION);
	bind[idx].buffer_length = strlen(XTRABACKUP_VERSION);
	++idx;

	/* server_version */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = server_version;
	bind[idx].buffer_length = strlen(server_version);
	++idx;

	/* start_time */
	bind[idx].buffer_type = MYSQL_TYPE_LONG;
	bind[idx].buffer = &history_start_time;
	++idx;

	/* end_time */
	bind[idx].buffer_type = MYSQL_TYPE_LONG;
	bind[idx].buffer = &history_end_time;
	++idx;

	/* lock_time */
	bind[idx].buffer_type = MYSQL_TYPE_LONG;
	bind[idx].buffer = &history_lock_time;
	++idx;

	/* binlog_pos */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = mysql_binlog_position;
	bind[idx].buffer_length = strlen(mysql_binlog_position);
	++idx;

	/* innodb_from_lsn */
	bind[idx].buffer_type = MYSQL_TYPE_LONGLONG;
	bind[idx].buffer = (char*)(&incremental_lsn);
	++idx;

	/* innodb_to_lsn */
	bind[idx].buffer_type = MYSQL_TYPE_LONGLONG;
	bind[idx].buffer = (char*)(&metadata_to_lsn);
	++idx;

	/* partial (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)((xtrabackup_tables
				    || xtrabackup_tables_file
				    || xtrabackup_databases
				    || xtrabackup_databases_file) ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	/* incremental (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(opt_ibx_incremental ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	/* format (file | tar | xbstream) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(xb_stream_name[xtrabackup_stream_fmt]);
	bind[idx].buffer_length = strlen(xb_stream_name[xtrabackup_stream_fmt]);
	++idx;

	/* compact (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(xtrabackup_compact ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	/* compressed (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(xtrabackup_compress ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	/* encrypted (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(xtrabackup_encrypt ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	ut_ad(idx == 19);

	mysql_stmt_bind_param(stmt, bind);

	mysql_stmt_execute(stmt);
	mysql_stmt_close(stmt);

cleanup:

	free(uuid);
	free(server_version);

	return(true);
}

bool write_backup_config_file()
{
	return backup_file_printf("backup-my.cnf",
		"# This MySQL options file was generated by innobackupex.\n\n"
		"# The MySQL server\n"
		"[mysqld]\n"
		"innodb_checksum_algorithm=%s\n"
		"innodb_log_checksum_algorithm=%s\n"
		"innodb_data_file_path=%s\n"
		"innodb_log_files_in_group=%lu\n"
		"innodb_log_file_size=%lld\n"
		"innodb_fast_checksum=%s\n"
		"innodb_page_size=%lu\n"
		"innodb_log_block_size=%lu\n"
		"innodb_undo_directory=%s\n"
		"innodb_undo_tablespaces=%lu\n"
		"%s%s\n",
		innodb_checksum_algorithm_names[srv_checksum_algorithm],
		innodb_checksum_algorithm_names[srv_log_checksum_algorithm],
		innobase_data_file_path,
		srv_n_log_files,
		innobase_log_file_size,
		srv_fast_checksum ? "true" : "false",
		srv_page_size,
		srv_log_block_size,
		srv_undo_dir,
		srv_undo_tablespaces,
		innobase_doublewrite_file ? "innodb_doublewrite_file=" : "",
		innobase_doublewrite_file ? innobase_doublewrite_file : "");
}



bool
backup_files(const char *from, bool prep_mode)
{
	char rsync_tmpfile_name[FN_REFLEN];
	FILE *rsync_tmpfile = NULL;
	datadir_iter_t *it;
	datadir_node_t node;
	bool ret = true;

	if (prep_mode && !opt_ibx_rsync) {
		return(true);
	}

	if (opt_ibx_rsync) {
		snprintf(rsync_tmpfile_name, sizeof(rsync_tmpfile_name),
			"%s/%s%d", opt_mysql_tmpdir,
			"xtrabackup_rsyncfiles_pass",
			prep_mode ? 1 : 2);
		rsync_tmpfile = fopen(rsync_tmpfile_name, "w");
		if (rsync_tmpfile == NULL) {
			ibx_msg("Error: can't create file %s\n",
				rsync_tmpfile_name);
			return(false);
		}
	}

	ibx_msg("Starting %s non-InnoDB tables and files\n",
		prep_mode ? "prep copy of" : "to backup");

	datadir_node_init(&node);
	it = datadir_iter_new(from);

	while (datadir_iter_next(it, &node)) {

		if (opt_ibx_rsync) {
			if (!node.is_empty_dir) {
				datafile_rsync_backup(node.filepath, !prep_mode,
						rsync_tmpfile);
			} else {
				fprintf(rsync_tmpfile, "%s\n", node.filepath);
			}
			continue;
		}

		if (!node.is_empty_dir) {
			if (!(ret = datafile_copy_backup(node.filepath, 1))) {
				ibx_msg("Failed to copy file %s\n",
					node.filepath);
				goto out;
			}
		} else {
			/* backup fake file into empty directory */
			char path[FN_REFLEN];
			ut_snprintf(path, sizeof(path),
					"%s/db.opt", node.filepath);
			if (!(ret = backup_file_printf(
					trim_dotslash(path), "%s", ""))) {
				ibx_msg("Failed to create file %s\n", path);
				goto out;
			}
		}
	}

	if (opt_ibx_rsync) {
		std::stringstream cmd;
		int err;

		if (buffer_pool_filename && file_exists(buffer_pool_filename)) {
			fprintf(rsync_tmpfile, "%s\n", buffer_pool_filename);
			rsync_list.insert(buffer_pool_filename);
		}
		if (file_exists("ib_lru_dump")) {
			fprintf(rsync_tmpfile, "%s\n", "ib_lru_dump");
			rsync_list.insert("ib_lru_dump");
		}

		fclose(rsync_tmpfile);
		rsync_tmpfile = NULL;

		cmd << "rsync -t . --files-from=" << rsync_tmpfile_name
		    << " " << xtrabackup_target_dir;

		ibx_msg("Starting rsync as: %s\n", cmd.str().c_str());
		if ((err = system(cmd.str().c_str()) && !prep_mode) != 0) {
			ibx_msg("Error: rsync failed with error code %d\n",
				err);
			goto out;
		}
		ibx_msg("rsync finished successfully.\n");

		if (!prep_mode && !opt_ibx_no_lock) {
			char path[FN_REFLEN];
			char dst_path[FN_REFLEN];
			char *newline;

			/* Remove files that have been removed between first and
			second passes. Cannot use "rsync --delete" because it
			does not work with --files-from. */
			snprintf(rsync_tmpfile_name, sizeof(rsync_tmpfile_name),
				"%s/%s", opt_mysql_tmpdir,
				"xtrabackup_rsyncfiles_pass1");

			rsync_tmpfile = fopen(rsync_tmpfile_name, "r");
			if (rsync_tmpfile == NULL) {
				ibx_msg("Error: can't open file %s\n",
					rsync_tmpfile_name);
				return(false);
			}

			while (fgets(path, sizeof(path), rsync_tmpfile)) {

				newline = strchr(path, '\n');
				if (newline) {
					*newline = 0;
				}
				if (rsync_list.count(path) < 1) {
					snprintf(dst_path, sizeof(dst_path),
						"%s/%s", xtrabackup_target_dir,
						path);
					ibx_msg("Removing %s\n", dst_path);
					unlink(dst_path);
				}
			}

			fclose(rsync_tmpfile);
			rsync_tmpfile = NULL;
		}
	}

	ibx_msg("Finished %s non-InnoDB tables and files\n",
		prep_mode ? "a prep copy of" : "backing up");

out:
	datadir_iter_free(it);
	datadir_node_free(&node);

	if (rsync_tmpfile != NULL) {
		fclose(rsync_tmpfile);
	}

	return(ret);
}

bool
make_backup_dir()
{
	time_t t = time(NULL);
	char buf[100];

	if (!opt_ibx_notimestamp) {
		strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", localtime(&t));
		ut_a(asprintf(&ibx_backup_directory, "%s/%s",
				ibx_position_arg, buf) != -1);
	} else {
		ibx_backup_directory = strdup(ibx_position_arg);
	}

	if (!directory_exists(ibx_backup_directory, true)) {
		return(false);
	}

	return(true);
}

static
char *make_argv(char *buf, size_t len, int argc, char **argv)
{
	size_t left= len;
	const char *arg;

	buf[0]= 0;
	++argv; --argc;
	while (argc > 0 && left > 0)
	{
		arg = *argv;
		if (strncmp(*argv, "--password", strlen("--password")) == 0) {
			arg = "--password=...";
		}
		if (strncmp(*argv, "--encrypt-key",
				strlen("--encrypt-key")) == 0) {
			arg = "--encrypt-key=...";
		}
		if (strncmp(*argv, "--encrypt_key",
				strlen("--encrypt_key")) == 0) {
			arg = "--encrypt_key=...";
		}
		left-= ut_snprintf(buf + len - left, left,
			"%s%c", arg, argc > 1 ? ' ' : 0);
		++argv; --argc;
	}

	return buf;
}

void
ibx_capture_tool_command(int argc, char **argv)
{
	/* capture tool name tool args */
	tool_name = strrchr(argv[0], '/');
	tool_name = tool_name ? tool_name + 1 : argv[0];

	make_argv(tool_args, sizeof(tool_args), argc, argv);
}

bool
ibx_handle_options(int *argc, char ***argv)
{
	char backup_config_path[FN_REFLEN];
	const char *groups[] = {"mysqld", NULL};
	int i;

	if (handle_options(argc, argv, ibx_long_options, ibx_get_one_option)) {
		return(false);
	}

	if (opt_ibx_apply_log) {
		ibx_mode = IBX_MODE_APPLY_LOG;
	} else if (opt_ibx_copy_back) {
		ibx_mode = IBX_MODE_COPY_BACK;
	} else if (opt_ibx_move_back) {
		ibx_mode = IBX_MODE_MOVE_BACK;
	} else if (opt_ibx_decrypt || opt_ibx_decompress) {
		ibx_mode = IBX_MODE_DECRYPT_DECOMPRESS;
	} else {
		ibx_mode = IBX_MODE_BACKUP;
	}

	/* find and save position argument */
	i = 0;
	while (i < *argc) {
		char *opt = (*argv)[i];

		if (strncmp(opt, "--", 2) != 0
		    && !(strlen(opt) == 2 && opt[0] == '-')) {
			if (ibx_position_arg != NULL
				&& ibx_position_arg != opt) {
				ibx_msg("Error: extra argument found %s\n",
					opt);
			}
			ibx_position_arg = opt;
			--(*argc);
			continue;
		}
		++i;
	}

	if (ibx_position_arg == NULL) {
		ibx_msg("Missing argument\n");
		return(false);
	}

	if (ibx_mode == IBX_MODE_APPLY_LOG
	    || ibx_mode == IBX_MODE_COPY_BACK
	    || ibx_mode == IBX_MODE_MOVE_BACK) {
		ut_snprintf(backup_config_path, sizeof(backup_config_path),
			"%s/backup_my.cnf", ibx_position_arg);
		load_defaults(backup_config_path, groups, argc, argv);
	}

	/* set argv[0] to be the program name */
	--(*argv);
	++(*argc);

	return(true);
}

/*********************************************************************//**
Parse command-line options, connect to MySQL server,
detect server capabilities, etc.
@return	true on success. */
bool
ibx_init()
{
	const char *run;
	const char *mixed_options[4] = {NULL, NULL, NULL, NULL};
	int n_mixed_options;

	/* setup xtrabackup options */
	xb_close_files = ibx_xb_close_files;
	xtrabackup_compact = ibx_xtrabackup_compact;
	xtrabackup_compress_alg = ibx_xtrabackup_compress_alg;
	xtrabackup_compress_threads = ibx_xtrabackup_compress_threads;
	xtrabackup_compress_chunk_size = ibx_xtrabackup_compress_chunk_size;
	xtrabackup_encrypt_algo = ibx_xtrabackup_encrypt_algo;
	xtrabackup_encrypt_key = ibx_xtrabackup_encrypt_key;
	xtrabackup_encrypt_key_file = ibx_xtrabackup_encrypt_key_file;
	xtrabackup_encrypt_threads = ibx_xtrabackup_encrypt_threads;
	xtrabackup_encrypt_chunk_size = ibx_xtrabackup_encrypt_chunk_size;
	xtrabackup_export = ibx_xtrabackup_export;
	xtrabackup_extra_lsndir = ibx_xtrabackup_extra_lsndir;
	xtrabackup_incremental_basedir = ibx_xtrabackup_incremental_basedir;
	xtrabackup_incremental_dir = ibx_xtrabackup_incremental_dir;
	xtrabackup_incremental_force_scan =
					ibx_xtrabackup_incremental_force_scan;
	xtrabackup_log_copy_interval = ibx_xtrabackup_log_copy_interval;
	xtrabackup_incremental = ibx_xtrabackup_incremental;
	xtrabackup_parallel = ibx_xtrabackup_parallel;
	xtrabackup_rebuild_indexes = ibx_xtrabackup_rebuild_indexes;
	xtrabackup_rebuild_threads = ibx_xtrabackup_rebuild_threads;
	xtrabackup_stream_str = ibx_xtrabackup_stream_str;
	xtrabackup_tables_file = ibx_xtrabackup_tables_file;
	xtrabackup_throttle = ibx_xtrabackup_throttle;
	opt_mysql_tmpdir = ibx_opt_mysql_tmpdir;
	xtrabackup_use_memory = ibx_xtrabackup_use_memory;


	/* sanity checks */
	if (!opt_ibx_incremental
	    && (xtrabackup_incremental
	    	|| xtrabackup_incremental_basedir
	    	|| opt_ibx_incremental_history_name
	    	|| opt_ibx_incremental_history_uuid)) {
		ibx_msg("Error: --incremental-lsn, --incremental-basedir, "
			"--incremental-history-name and "
			"--incremental-history-uuid require the "
			"--incremental option.\n");
		return(false);
	}

	if (opt_ibx_slave_info
		&& opt_ibx_no_lock
		&& !opt_ibx_safe_slave_backup) {
		ibx_msg("Error: --slave-info is used with --no-lock but "
			"without --safe-slave-backup. The binlog position "
			"cannot be consistent with the backup data.\n");
		return(false);
	}

	if (opt_ibx_rsync && xtrabackup_stream_fmt) {
		ibx_msg("Error: --rsync doesn't work with --stream\n");
		return(false);
	}

	n_mixed_options = 0;

	if (opt_ibx_decompress) {
		mixed_options[n_mixed_options++] = "--decompress";
	} else if (opt_ibx_decrypt) {
		mixed_options[n_mixed_options++] = "--decrypt";
	}

	if (opt_ibx_copy_back) {
		mixed_options[n_mixed_options++] = "--copy-back";
	}

	if (opt_ibx_move_back) {
		mixed_options[n_mixed_options++] = "--move-back";
	}

	if (opt_ibx_apply_log) {
		mixed_options[n_mixed_options++] = "--apply-log";
	}

	if (n_mixed_options > 1) {
		ibx_msg("Error: %s and %s are mutually exclusive\n",
			mixed_options[0], mixed_options[1]);
		return(false);
	}

	if (opt_ibx_databases != NULL) {
		if (is_path_separator(*opt_ibx_databases)) {
			xtrabackup_databases_file = opt_ibx_databases;
		} else {
			xtrabackup_databases = opt_ibx_databases;
		}
	}

	/* --tables and --tables-file options are xtrabackup only */
	ibx_partial_backup = (opt_ibx_include || opt_ibx_databases);

	if (ibx_mode == IBX_MODE_BACKUP) {

		if ((mysql_connection = ibx_mysql_connect()) == NULL) {
			return(false);
		}

		if (!get_mysql_vars(mysql_connection)) {
			return(false);
		}

		if (!detect_mysql_capabilities_for_backup()) {
			return(false);
		}

		if (!make_backup_dir()) {
			return(false);
		}
	}

	switch (ibx_mode) {
	case IBX_MODE_APPLY_LOG:
		xtrabackup_prepare = TRUE;
		if (opt_ibx_redo_only) {
			xtrabackup_apply_log_only = TRUE;
		}
		xtrabackup_target_dir = ibx_position_arg;
		run = "apply-log";
		break;
	case IBX_MODE_BACKUP:
		xtrabackup_backup = TRUE;
		xtrabackup_target_dir = ibx_backup_directory;
		if (opt_ibx_include != NULL) {
			xtrabackup_tables = opt_ibx_include;
		}
		history_start_time = time(NULL);
		run = "backup";
		break;
	case IBX_MODE_COPY_BACK:
		xtrabackup_copy_back = TRUE;
		xtrabackup_target_dir = ibx_position_arg;
		run = "copy-back";
		break;
	case IBX_MODE_MOVE_BACK:
		xtrabackup_move_back = TRUE;
		xtrabackup_target_dir = ibx_position_arg;
		run = "move-back";
		break;
	case IBX_MODE_DECRYPT_DECOMPRESS:
		xtrabackup_decrypt_decompress = TRUE;
		xtrabackup_target_dir = ibx_position_arg;
		run = "decrypt and decompress";
		break;
	default:
		ut_error;
	}

	ibx_msg("Starting the %s operation\n\n"
		"IMPORTANT: Please check that the %s run completes "
		"successfully.\n"
		"           At the end of a successful %s run innobackupex\n"
		"           prints \"completed OK!\".\n\n", run, run, run);


	return(true);
}

/*********************************************************************//**
Deallocate memory, disconnect from MySQL server, etc.
@return	true on success. */
void
ibx_cleanup()
{
	free(mysql_slave_position);
	free(mysql_binlog_position);
	free(buffer_pool_filename);
	free(ibx_backup_directory);

	free(innobase_data_file_path_alloc);

	if (mysql_connection) {
		mysql_close(mysql_connection);
	}
}


bool
ibx_select_history()
{
	if (opt_ibx_incremental && !xtrabackup_incremental) {
		if (!select_incremental_lsn_from_history(
			&incremental_lsn)) {
			return(false);
		}
	}
	return(true);
}

bool
ibx_flush_changed_page_bitmaps()
{
	if (opt_ibx_incremental && have_changed_page_bitmaps &&
	    !xtrabackup_incremental_force_scan) {
		ibx_mysql_query(mysql_connection,
			"FLUSH NO_WRITE_TO_BINLOG CHANGED_PAGE_BITMAPS", false);
	}
	return(true);
}

void
version_check()
{
	if (opt_ibx_password != NULL) {
		setenv("option_mysql_password", opt_ibx_password, 1);
	}
	if (opt_ibx_user != NULL) {
		setenv("option_mysql_user", opt_ibx_user, 1);
	}
	if (opt_ibx_host != NULL) {
		setenv("option_mysql_host", opt_ibx_host, 1);
	}
	if (opt_ibx_socket != NULL) {
		setenv("option_mysql_socket", opt_ibx_socket, 1);
	}
	if (opt_ibx_port != 0) {
		char port[20];
		snprintf(port, sizeof(port), "%u", opt_ibx_port);
		setenv("option_mysql_port", port, 1);
	}

	FILE *pipe = popen("perl", "w");
	if (pipe == NULL) {
		return;
	}

	fputs((const char *)version_check_pl, pipe);

	pclose(pipe);
}

bool
ibx_backup_start()
{
	if (!opt_ibx_noversioncheck) {
		version_check();
	}

	if (!opt_ibx_no_lock) {
		if (opt_ibx_safe_slave_backup) {
			if (!wait_for_safe_slave(mysql_connection)) {
				return(false);
			}
		}

		if (!backup_files(fil_path_to_mysql_datadir, true)) {
			return(false);
		}

		history_lock_time = time(NULL);

		if (!ibx_lock_tables(mysql_connection)) {
			return(false);
		}
	}

	if (!backup_files(fil_path_to_mysql_datadir, false)) {
		return(false);
	}

	// There is no need to stop slave thread before coping non-Innodb data when
	// --no-lock option is used because --no-lock option requires that no DDL or
	// DML to non-transaction tables can occur.
	if (opt_ibx_no_lock) {
		if (opt_ibx_safe_slave_backup) {
			if (!wait_for_safe_slave(mysql_connection)) {
				return(false);
			}
		}
	} else {
		ibx_lock_binlog(mysql_connection);
	}

	if (opt_ibx_slave_info) {
		if (!write_slave_info(mysql_connection)) {
			return(false);
		}
	}

	/* The only reason why Galera/binlog info is written before
	wait_for_ibbackup_log_copy_finish() is that after that call the xtrabackup
	binary will start streamig a temporary copy of REDO log to stdout and
	thus, any streaming from innobackupex would interfere. The only way to
	avoid that is to have a single process, i.e. merge innobackupex and
	xtrabackup. */
	if (opt_ibx_galera_info) {
		if (!write_galera_info(mysql_connection)) {
			return(false);
		}
		write_current_binlog_file(mysql_connection);
	}

	write_binlog_info(mysql_connection);

	if (have_flush_engine_logs) {
		ibx_msg("Executing FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS...\n");
		ibx_mysql_query(mysql_connection,
			"FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS", false);
	}

	return(true);
}


bool
ibx_backup_finish()
{
	/* release all locks */
	if (!opt_ibx_no_lock) {
		ibx_unlock_all(mysql_connection);
		history_lock_time = 0;
	} else {
		history_lock_time = time(NULL) - history_lock_time;
	}

	if (opt_ibx_safe_slave_backup && sql_thread_started) {
		ibx_msg("Starting slave SQL thread\n");
		ibx_mysql_query(mysql_connection,
				"START SLAVE SQL_THREAD", false);
	}

	/* Copy buffer pool dump or LRU dump */
	if (!opt_ibx_rsync) {
		if (buffer_pool_filename && file_exists(buffer_pool_filename)) {
			const char *dst_name;

			dst_name = trim_dotslash(buffer_pool_filename);
			copy_file(buffer_pool_filename, dst_name, 0);
		}
		if (file_exists("ib_lru_dump")) {
			copy_file("ib_lru_dump", "ib_lru_dump", 0);
		}
	}

	ibx_msg("Backup created in directory '%s'\n", xtrabackup_target_dir);
	if (mysql_binlog_position != NULL) {
		ibx_msg("MySQL binlog position: %s\n", mysql_binlog_position);
	}
	if (mysql_slave_position && opt_ibx_slave_info) {
		ibx_msg("MySQL slave binlog position: %s\n",
			mysql_slave_position);
	}

	if (!write_backup_config_file()) {
		return(false);
	}

	if (!write_xtrabackup_info(mysql_connection)) {
		return(false);
	}



	return(true);
}

bool
ibx_copy_incremental_over_full()
{
	const char *ext_list[] = {"frm", "isl", "MYD", "MYI", "MAD", "MAI",
		"MRG", "TRG", "TRN", "ARM", "ARZ", "CSM", "CSV", "opt", "par",
		NULL};
	datadir_iter_t *it = NULL;
	datadir_node_t node;
	bool ret = true;
	char path[FN_REFLEN];

	datadir_node_init(&node);

	/* If we were applying an incremental change set, we need to make
	sure non-InnoDB files and xtrabackup_* metainfo files are copied
	to the full backup directory. */

	if (xtrabackup_incremental) {

		ds_data = ds_create(xtrabackup_target_dir, DS_TYPE_LOCAL);

		it = datadir_iter_new(xtrabackup_incremental_dir);

		while (datadir_iter_next(it, &node)) {

			/* copy only non-innodb files */

			if (node.is_empty_dir
			    || !filename_matches(node.filepath, ext_list)) {
				continue;
			}

			if (file_exists(node.filepath_rel)) {
				unlink(node.filepath_rel);
			}

			if (!(ret = copy_file(node.filepath,
						node.filepath_rel, 1))) {
				ibx_msg("Failed to copy file %s\n",
					node.filepath);
				goto cleanup;
			}
		}

		/* copy buffer pool dump */
		if (innobase_buffer_pool_filename) {
			const char *src_name;

			src_name = trim_dotslash(innobase_buffer_pool_filename);

			snprintf(path, sizeof(path), "%s/%s",
				xtrabackup_incremental_dir,
				src_name);

			if (file_exists(path)) {
				copy_file(path, innobase_buffer_pool_filename,
						0);
			}
		}

		snprintf(path, sizeof(path), "%s/%s",
			xtrabackup_incremental_dir,
			"ib_lru_dump");

		if (file_exists(path)) {
			copy_file(path, "ib_lru_dump", 0);
		}

	}

cleanup:
	if (it != NULL) {
		datadir_iter_free(it);
	}

	if (ds_data != NULL) {
		ds_destroy(ds_data);
	}

	datadir_node_free(&node);

	return(ret);
}

bool
ibx_cleanup_full_backup()
{
	const char *ext_list[] = {"delta", "meta", "ibd", NULL};
	datadir_iter_t *it = NULL;
	datadir_node_t node;
	bool ret = true;

	datadir_node_init(&node);

	/* If we are applying an incremental change set, we need to make
	sure non-InnoDB files are cleaned up from full backup dir before
	we copy files from incremental dir. */

	if (xtrabackup_incremental) {

		it = datadir_iter_new(xtrabackup_target_dir);

		while (datadir_iter_next(it, &node)) {

			if (node.is_empty_dir
			    || filename_matches(node.filepath, ext_list)) {
				continue;
			}

			unlink(node.filepath);
		}
	}

	if (it != NULL) {
		datadir_iter_free(it);
	}

	datadir_node_free(&node);

	return(ret);
}

bool
ibx_apply_log_finish()
{
	if (!ibx_cleanup_full_backup()
		|| !ibx_copy_incremental_over_full()) {
		return(false);
	}

	return(true);
}

bool
ibx_copy_back()
{
	char *innobase_data_file_path_copy;
	ulint i;
	bool ret;
	datadir_iter_t *it = NULL;
	datadir_node_t node;

	if (!opt_ibx_force_non_empty_dirs) {
		if (!directory_exists_and_empty(mysql_data_home,
							"Original data")) {
			return(false);
		}
	} else {
		if (!directory_exists(mysql_data_home, true)) {
			return(false);
		}
	}
	if (srv_undo_dir && *srv_undo_dir
		&& !directory_exists(srv_undo_dir, true)) {
			return(false);
	}
	if (innobase_data_home_dir && *innobase_data_home_dir
		&& !directory_exists(innobase_data_home_dir, true)) {
			return(false);
	}
	if (srv_log_group_home_dir && *srv_log_group_home_dir
		&& !directory_exists(srv_log_group_home_dir, true)) {
			return(false);
	}

	/* cd to backup directory */
	if (my_setwd(xtrabackup_target_dir, MYF(MY_WME)))
	{
		ibx_msg("cannot my_setwd %s\n", xtrabackup_target_dir);
		return(false);
	}

	/* parse data file path */

	if (!innobase_data_file_path) {
  		innobase_data_file_path = (char*) "ibdata1:10M:autoextend";
	}
	innobase_data_file_path_copy = strdup(innobase_data_file_path);

	if (!(ret = srv_parse_data_file_paths_and_sizes(
					innobase_data_file_path_copy))) {
		ibx_msg("syntax error in innodb_data_file_path\n");
		return(false);
	}

	srv_max_n_threads = 1000;
	os_sync_mutex = NULL;
	ut_mem_init();
	/* temporally dummy value to avoid crash */
	srv_page_size_shift = 14;
	srv_page_size = (1 << srv_page_size_shift);
	os_sync_init();
	sync_init();
	os_io_init_simple();
	mem_init(srv_mem_pool_size);
	ut_crc32_init();

	/* copy undo tablespaces */
	if (srv_undo_tablespaces > 0) {

		ds_data = ds_create((srv_undo_dir && *srv_undo_dir)
					? srv_undo_dir : mysql_data_home,
				DS_TYPE_LOCAL);

		for (i = 1; i <= srv_undo_tablespaces; i++) {
			char filename[20];
			sprintf(filename, "undo%03lu", i);
			if (!(ret = copy_or_move_file(filename, filename, 1))) {
				goto cleanup;
			}
		}

		ds_destroy(ds_data);
		ds_data = NULL;
	}

	/* copy redo logs */

	ds_data = ds_create((srv_log_group_home_dir && *srv_log_group_home_dir)
				? srv_log_group_home_dir : mysql_data_home,
			DS_TYPE_LOCAL);

	for (i = 0; i < (ulong)innobase_log_files_in_group; i++) {
		char filename[20];
		sprintf(filename, "ib_logfile%lu", i);

		if (!file_exists(filename)) {
			continue;
		}

		if (!(ret = copy_or_move_file(filename, filename, 1))) {
			goto cleanup;
		}
	}

	ds_destroy(ds_data);
	ds_data = NULL;

	/* copy innodb system tablespace(s) */

	ds_data = ds_create((innobase_data_home_dir && *innobase_data_home_dir)
				? innobase_data_home_dir : mysql_data_home,
			DS_TYPE_LOCAL);

	for (i = 0; i < srv_n_data_files; i++) {
		const char *filename = get_filename(srv_data_file_names[i]);

		if (!(ret = copy_or_move_file(filename,
						srv_data_file_names[i], 1))) {
			goto cleanup;
		}
	}

	ds_destroy(ds_data);
	ds_data = NULL;

	/* copy the rest of tablespaces */
	ds_data = ds_create(mysql_data_home, DS_TYPE_LOCAL);

	it = datadir_iter_new(".", false);

	datadir_node_init(&node);

	while (datadir_iter_next(it, &node)) {
		const char *ext_list[] = {"backup-my.cnf", "xtrabackup_logfile",
			"xtrabackup_binary", "xtrabackup_binlog_info",
			"xtrabackup_checkpoints", ".qp", ".pmap", ".tmp",
			".xbcrypt", NULL};
		const char *filename;
		char c_tmp;
		int i_tmp;
		bool is_ibdata_file;

		/* create empty directories */
		if (node.is_empty_dir) {
			char path[FN_REFLEN];

			snprintf(path, sizeof(path), "%s/%s",
				mysql_data_home, node.filepath_rel);

			ibx_msg("[%02u] Creating directory %s\n", 1,
				path);

			if (mkdirp(path, 0777, MYF(0)) < 0) {
				char errbuf[MYSYS_STRERROR_SIZE];

				ibx_msg("Can not create directory %s: %s\n",
					path, my_strerror(errbuf,
						sizeof(errbuf), my_errno));
				ret = false;

				goto cleanup;

			}

			ibx_msg("[%02u] ...done.", 1);

			continue;
		}

		filename = get_filename(node.filepath);

		/* skip .qp and .xbcrypt files */
		if (filename_matches(filename, ext_list)) {
			continue;
		}

		/* skip undo tablespaces */
		if (sscanf(filename, "undo%d%c", &i_tmp, &c_tmp) == 1) {
			continue;
		}

		/* skip redo logs */
		if (sscanf(filename, "ib_logfile%d%c", &i_tmp, &c_tmp) == 1) {
			continue;
		}

		/* skip innodb data files */
		is_ibdata_file = false;
		for (i = 0; i < srv_n_data_files; i++) {
			const char *ibfile;

			ibfile = get_filename(srv_data_file_names[i]);

			if (strcmp(ibfile, filename) == 0) {
				is_ibdata_file = true;
				continue;
			}
		}
		if (is_ibdata_file) {
			continue;
		}

		if (!(ret = copy_or_move_file(node.filepath,
						node.filepath_rel, 1))) {
			goto cleanup;
		}
	}

	/* copy buufer pool dump */

	if (innobase_buffer_pool_filename) {
		const char *src_name;
		char path[FN_REFLEN];

		src_name = trim_dotslash(innobase_buffer_pool_filename);

		snprintf(path, sizeof(path), "%s/%s",
			mysql_data_home,
			src_name);

		/* could be already copied with other files
		from data directory */
		if (file_exists(src_name) &&
			!file_exists(innobase_buffer_pool_filename)) {
			copy_file(src_name, innobase_buffer_pool_filename, 0);
		}
	}

cleanup:
	if (it != NULL) {
		datadir_iter_free(it);
	}

	datadir_node_free(&node);

	free(innobase_data_file_path_copy);

	if (ds_data != NULL) {
		ds_destroy(ds_data);
	}

	ds_data = NULL;

	sync_close();
	sync_initialized = FALSE;
	os_sync_free();
	mem_close();
	os_sync_mutex = NULL;
	ut_free_all_mem();

	return(ret);
}

bool
decrypt_decompress_file(const char *filepath, uint thread_n)
{
	std::stringstream cmd;
	char *dest_filepath = strdup(filepath);
	bool needs_action = false;

	cmd << "cat " << filepath;

 	if (ends_with(filepath, ".xbcrypt") && opt_ibx_decrypt) {
 		cmd << " | xbcrypt --decrypt --encrypt-algo="
 		    << xtrabackup_encrypt_algo_names[opt_ibx_decrypt_algo];
 		if (xtrabackup_encrypt_key) {
 			cmd << " --encrypt-key=" << xtrabackup_encrypt_key;
 		} else {
 			cmd << " --encrypt-key-file="
 			    << xtrabackup_encrypt_key_file;
 		}
 		dest_filepath[strlen(dest_filepath) - 8] = 0;
 		needs_action = true;
 	}

 	if (opt_ibx_decompress
 	    && (ends_with(filepath, ".qp")
		|| (ends_with(filepath, ".qp.xbcrypt")
		    && opt_ibx_decrypt))) {
 		cmd << " | qpress -dio ";
 		dest_filepath[strlen(dest_filepath) - 3] = 0;
 		needs_action = true;
 	}

 	cmd << " > " << dest_filepath;

 	free(dest_filepath);

 	if (needs_action) {

	 	ibx_msg("[%02u] %s\n", thread_n, cmd.str().c_str());

	 	if (system(cmd.str().c_str()) != 0) {
	 		return(false);
	 	}
	 }

 	return(true);
}

static
os_thread_ret_t
ibx_decrypt_decompress_thread_func(void *arg)
{
	bool ret = true;
	datadir_node_t node;
	datadir_thread_ctxt_t *ctxt = (datadir_thread_ctxt_t *)(arg);

	datadir_node_init(&node);

	while (datadir_iter_next(ctxt->it, &node)) {

		/* skip empty directories in backup */
		if (node.is_empty_dir) {
			continue;
		}

		if (!ends_with(node.filepath, ".qp")
		    && !ends_with(node.filepath, ".xbcrypt")) {
			continue;
		}

		if (!(ret = decrypt_decompress_file(node.filepath,
							ctxt->n_thread))) {
			goto cleanup;
		}
	}

cleanup:

	datadir_node_free(&node);

	os_mutex_enter(ctxt->count_mutex);
	--(*ctxt->count);
	os_mutex_exit(ctxt->count_mutex);

	ctxt->ret = ret;

	os_thread_exit(NULL);
	OS_THREAD_DUMMY_RETURN;
}

bool
ibx_decrypt_decompress()
{
	bool ret;
	datadir_iter_t *it = NULL;

	srv_max_n_threads = 1000;
	os_sync_mutex = NULL;
	ut_mem_init();
	os_sync_init();
	sync_init();

	/* cd to backup directory */
	if (my_setwd(xtrabackup_target_dir, MYF(MY_WME)))
	{
		ibx_msg("cannot my_setwd %s\n", xtrabackup_target_dir);
		return(false);
	}

	/* copy the rest of tablespaces */
	ds_data = ds_create(".", DS_TYPE_LOCAL);

	it = datadir_iter_new(".", false);

	ut_a(xtrabackup_parallel >= 0);

	ret = run_data_threads(it, ibx_decrypt_decompress_thread_func,
		xtrabackup_parallel ? xtrabackup_parallel : 1);

	if (it != NULL) {
		datadir_iter_free(it);
	}

	if (ds_data != NULL) {
		ds_destroy(ds_data);
	}

	ds_data = NULL;

	sync_close();
	sync_initialized = FALSE;
	os_sync_free();
	os_sync_mutex = NULL;
	ut_free_all_mem();

	return(ret);
}

void
ibx_completed_ok()
{
	ibx_msg("completed OK!\n");
}
