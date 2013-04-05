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

#include <my_base.h>
#include <mysql_com.h>
#include "innodb_int.h"
#include "ds_tmpfile.h"

#include "common.h"

extern long innobase_lock_wait_timeout;

#if MYSQL_VERSION_ID < 50600
char *opt_mysql_tmpdir = NULL;
#endif

#if MYSQL_VERSION_ID >= 50600
const char*	xb_dict_prefix	= "";
#else
const char*	xb_dict_prefix	= "./";
#endif

#if MYSQL_VERSION_ID < 50600
char*	innobase_log_group_home_dir		= NULL;
#endif

long innobase_mirrored_log_groups = 1;

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
	ibool*		success)/*!< out: TRUE if succeed, FALSE if error */
{
#if MYSQL_VERSION_ID > 50500
	return os_file_create_simple_no_error_handling(
		0, /* innodb_file_data_key */
		name, create_mode, access_type, success);
#else
	return os_file_create_simple_no_error_handling(
		name, create_mode, access_type, success);
#endif
}

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
	ibool*		success)/*!< out: TRUE if succeed, FALSE if error */
{
	os_file_t	result;
#if MYSQL_VERSION_ID >= 50600
	ibool	old_srv_read_only_mode = srv_read_only_mode;

	srv_read_only_mode = FALSE;
#endif
#if MYSQL_VERSION_ID > 50500
	result = os_file_create(0 /* innodb_file_data_key */,
				name, create_mode, purpose, type, success);
#else
	result=  os_file_create(name, create_mode, purpose, type, success);
#endif
#if MYSQL_VERSION_ID >= 50600
	srv_read_only_mode = old_srv_read_only_mode;
#endif
	return result;
}

/***********************************************************************//**
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@return	TRUE if success */
ibool
xb_file_rename(
/*===========*/
	const char*	oldpath,/*!< in: old file path as a null-terminated
				string */
	const char*	newpath)/*!< in: new file path */
{
#if MYSQL_VERSION_ID > 50500
	return os_file_rename(
		0 /* innodb_file_data_key */, oldpath, newpath);
#else
	return os_file_rename(oldpath, newpath);
#endif
}

void
xb_file_set_nocache(
/*================*/
	os_file_t	fd,		/* in: file descriptor to alter */
	const char*	file_name,	/* in: used in the diagnostic message */
	const char*	operation_name) /* in: used in the diagnostic message,
					we call os_file_set_nocache()
					immediately after opening or creating
					a file, so this is either "open" or
					"create" */
{
#ifndef __WIN__
	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(fd, file_name, operation_name);
	}
#endif
}

/***********************************************************************//**
Compatibility wrapper around os_file_flush().
@return	TRUE if success */
ibool
xb_file_flush(
/*==========*/
	os_file_t	file)	/*!< in, own: handle to a file */
{
#ifdef XTRADB_BASED
	return os_file_flush(file, TRUE);
#else
	return os_file_flush(file);
#endif
}
/*******************************************************************//**
Returns the table space by a given id, NULL if not found. */
fil_space_t*
xb_space_get_by_id(
/*================*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;

	ut_ad(mutex_own(&fil_system->mutex));

	HASH_SEARCH(hash, fil_system->spaces, id,
		    fil_space_t*, space,
		    ut_ad(space->magic_n == FIL_SPACE_MAGIC_N),
		    space->id == id);

	return(space);
}

/*******************************************************************//**
Returns the table space by a given name, NULL if not found. */
fil_space_t*
xb_space_get_by_name(
/*==================*/
	const char*	name)	/*!< in: space name */
{
	fil_space_t*	space;
	ulint		fold;

	ut_ad(mutex_own(&fil_system->mutex));

	fold = ut_fold_string(name);
	HASH_SEARCH(name_hash, fil_system->name_hash, fold,
		    fil_space_t*, space,
		    ut_ad(space->magic_n == FIL_SPACE_MAGIC_N),
		    !strcmp(name, space->name));

	return(space);
}

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
	os_file_t*	file)		/*!<out: file handle */
{
	ibool		ret;
	byte*		buf;
	byte*		page;

	*file = xb_file_create_no_error_handling(path, OS_FILE_CREATE,
						 OS_FILE_READ_WRITE, &ret);
	if (!ret) {
		msg("xtrabackup: cannot create file %s\n", path);
		return ret;
	}

	ret = xb_os_file_set_size(path, *file,
				  FIL_IBD_FILE_INITIAL_SIZE * UNIV_PAGE_SIZE);
	if (!ret) {
		msg("xtrabackup: cannot set size for file %s\n", path);
		os_file_close(*file);
		os_file_delete(path);
		return ret;
	}

	buf = static_cast<byte *>(ut_malloc(3 * UNIV_PAGE_SIZE));
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = static_cast<byte *>(ut_align(buf, UNIV_PAGE_SIZE));

	memset(page, '\0', UNIV_PAGE_SIZE);

	fsp_header_init_fields(page, space_id, flags);
	mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

	if (!fsp_flags_is_compressed(flags)) {
		buf_flush_init_for_writing(page, NULL, 0);

		ret = xb_os_file_write(path, *file, page, 0, UNIV_PAGE_SIZE);
	}
	else {
		page_zip_des_t	page_zip;
		ulint		zip_size;

		zip_size = fsp_flags_get_zip_size(flags);
		page_zip_set_size(&page_zip, zip_size);
		page_zip.data = page + UNIV_PAGE_SIZE;
		fprintf(stderr, "zip_size = %lu\n", zip_size);

#ifdef UNIV_DEBUG
		page_zip.m_start =
#endif /* UNIV_DEBUG */
			page_zip.m_end = page_zip.m_nonempty =
			page_zip.n_blobs = 0;

		buf_flush_init_for_writing(page, &page_zip, 0);

		ret = xb_os_file_write(path, *file, page_zip.data, 0,
				       zip_size);
	}

	ut_free(buf);

	if (!ret) {
		msg("xtrabackup: could not write the first page to %s\n",
		    path);
		os_file_close(*file);
		os_file_delete(path);
		return ret;
	}

	return TRUE;
}

/*********************************************************************//**
Normalizes init parameter values to use units we use inside InnoDB.
@return	DB_SUCCESS or error code */
void
xb_normalize_init_values(void)
/*==========================*/
{
	ulint	i;

	for (i = 0; i < srv_n_data_files; i++) {
		srv_data_file_sizes[i] = srv_data_file_sizes[i]
					* ((1024 * 1024) / UNIV_PAGE_SIZE);
	}

	srv_last_file_size_max = srv_last_file_size_max
					* ((1024 * 1024) / UNIV_PAGE_SIZE);

	srv_log_file_size = srv_log_file_size / UNIV_PAGE_SIZE;

	srv_log_buffer_size = srv_log_buffer_size / UNIV_PAGE_SIZE;

	srv_lock_table_size = 5 * (srv_buf_pool_size / UNIV_PAGE_SIZE);
}

#if MYSQL_VERSION_ID < 50600

void
innobase_invalidate_query_cache(
	trx_t*	trx,
	const char*	full_name,
	ulint	full_name_len)
{
	(void)trx;
	(void)full_name;
	(void)full_name_len;
	/* do nothing */
}

#if MYSQL_VERSION_ID >= 50500

/**********************************************************************//**
It should be safe to use lower_case_table_names=0 for xtrabackup. If it causes
any problems, we can add the lower_case_table_names option to xtrabackup
later.
@return	0 */
ulint
innobase_get_lower_case_table_names(void)
/*=====================================*/
{
	return(0);
}

/******************************************************************//**
Strip dir name from a full path name and return only the file name
@return file name or "null" if no file name */
const char*
innobase_basename(
/*==============*/
	const char*	path_name)	/*!< in: full path name */
{
	const char*	name = base_name(path_name);

	return((name) ? name : "null");
}

#endif

/*****************************************************************//**
Convert an SQL identifier to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return	pointer to the end of buf */
static
char*
innobase_convert_identifier(
/*========================*/
	char*		buf,	/*!< out: buffer for converted identifier */
	ulint		buflen,	/*!< in: length of buf, in bytes */
	const char*	id,	/*!< in: identifier to convert */
	ulint		idlen,	/*!< in: length of id, in bytes */
	void*		thd __attribute__((unused)), 
						/*!< in: MySQL connection thread, or NULL */
	ibool		file_id __attribute__((unused)))
						/*!< in: TRUE=id is a table or database name;
						FALSE=id is an UTF-8 string */
{
	const char*	s	= id;
	int		q;

	/* See if the identifier needs to be quoted. */
	q = '"';

	if (q == EOF) {
		if (UNIV_UNLIKELY(idlen > buflen)) {
			idlen = buflen;
		}
		memcpy(buf, s, idlen);
		return(buf + idlen);
	}

	/* Quote the identifier. */
	if (buflen < 2) {
		return(buf);
	}

	*buf++ = q;
	buflen--;

	for (; idlen; idlen--) {
		int	c = *s++;
		if (UNIV_UNLIKELY(c == q)) {
			if (UNIV_UNLIKELY(buflen < 3)) {
				break;
			}

			*buf++ = c;
			*buf++ = c;
			buflen -= 2;
		} else {
			if (UNIV_UNLIKELY(buflen < 2)) {
				break;
			}

			*buf++ = c;
			buflen--;
		}
	}

	*buf++ = q;
	return(buf);
}

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
	ibool		table_id)/*!< in: TRUE=id is a table or database name;
				FALSE=id is an index name */
{
	char*		s	= buf;
	const char*	bufend	= buf + buflen;

	if (table_id) {
		const char*	slash = (const char*) memchr(id, '/', idlen);
		if (!slash) {

			goto no_db_name;
		}

		/* Print the database name and table name separately. */
		s = innobase_convert_identifier(s, bufend - s, id, slash - id,
						thd, TRUE);
		if (UNIV_LIKELY(s < bufend)) {
			*s++ = '.';
			s = innobase_convert_identifier(s, bufend - s,
							slash + 1, idlen
							- (slash - id) - 1,
							thd, TRUE);
		}
	} else if (UNIV_UNLIKELY(*id == TEMP_INDEX_PREFIX)) {
		/* Temporary index name (smart ALTER TABLE) */
		const char temp_index_suffix[]= "--temporary--";

		s = innobase_convert_identifier(buf, buflen, id + 1, idlen - 1,
						thd, FALSE);
		if (s - buf + (sizeof temp_index_suffix - 1) < buflen) {
			memcpy(s, temp_index_suffix,
			       sizeof temp_index_suffix - 1);
			s += sizeof temp_index_suffix - 1;
		}
	} else {
no_db_name:
		s = innobase_convert_identifier(buf, buflen, id, idlen,
						thd, table_id);
	}

	return(s);

}

ibool
trx_is_interrupted(
	trx_t*	trx)
{
	(void)trx;
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
				msg("xtrabackup: InnoDB needs charset %lu for "
				    "doing a comparison, but MySQL cannot "
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

ulint
innobase_raw_format(
/*================*/
	const char*	data,		/*!< in: raw data */
	ulint		data_len,	/*!< in: raw data length
					in bytes */
	ulint		charset_coll,	/*!< in: charset collation */
	char*		buf,		/*!< out: output buffer */
	ulint		buf_size)	/*!< in: output buffer size
					in bytes */
{
	(void)data;
	(void)data_len;
	(void)charset_coll;
	(void)buf;
	(void)buf_size;

	msg("xtrabackup: innobase_raw_format() is called\n");
	return(0);
}

ulong
thd_lock_wait_timeout(
/*==================*/
	void*	thd)	/*!< in: thread handle (THD*), or NULL to query
			the global innodb_lock_wait_timeout */
{
	(void)thd;
	return(innobase_lock_wait_timeout);
}

ibool
thd_supports_xa(
/*============*/
	void*	thd)	/*!< in: thread handle (THD*), or NULL to query
			the global innodb_supports_xa */
{
	(void)thd;
	return(FALSE);
}

ibool
trx_is_strict(
/*==========*/
	trx_t*	trx)	/*!< in: transaction */
{
	(void)trx;
	return(FALSE);
}

#endif /* MYSQL_VERSION_ID < 50600 */

#ifdef XTRADB_BASED
trx_t*
innobase_get_trx()
{
	return(NULL);
}

ibool
innobase_get_slow_log()
{
	return(FALSE);
}
#endif

#if MYSQL_VERSION_ID < 50600

extern "C" {

ibool
thd_is_replication_slave_thread(
	void*	thd)
{
	(void)thd;
	msg("xtrabackup: thd_is_replication_slave_thread() is called\n");
	return(FALSE);
}

ibool
thd_has_edited_nontrans_tables(
	void*	thd)
{
	(void)thd;
	msg("xtrabackup: thd_has_edited_nontrans_tables() is called\n");
	return(FALSE);
}

ibool
thd_is_select(
	const void*	thd)
{
	(void)thd;
	msg("xtrabackup: thd_is_select() is called\n");
	return(FALSE);
}

void
innobase_mysql_print_thd(
	FILE*   f,
	void*   input_thd,
	uint	max_query_len)
{
	(void)f;
	(void)input_thd;
	(void)max_query_len;
	msg("xtrabackup: innobase_mysql_print_thd() is called\n");
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
	struct charset_info_st*	cs,
	char*	to,
	const char*	from,
	ulint	len)
{
	(void)cs;
	(void)to;
	(void)from;
	(void)len;

	msg("xtrabackup: innobase_convert_from_table_id() is called\n");
}

void
innobase_convert_from_id(
	struct charset_info_st*	cs,
	char*	to,
	const char*	from,
	ulint	len)
{
	(void)cs;
	(void)to;
	(void)from;
	(void)len;
	msg("xtrabackup: innobase_convert_from_id() is called\n");
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
	(void)mysql_thd;
	msg("xtrabackup: innobase_get_charset() is called\n");
	return(NULL);
}

const char*
innobase_get_stmt(
	void*	mysql_thd,
	size_t*	length)
{
	(void)mysql_thd;
	(void)length;
	msg("xtrabackup: innobase_get_stmt() is called\n");
	return("nothing");
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
#ifdef _WIN32
		/* Note that on Windows, the integer returned by mysql_tmpfile
		has no relation to C runtime file descriptor. Here, we need
		to call my_get_osfhandle to get the HANDLE and then convert it
		to C runtime filedescriptor. */
		{
			HANDLE hFile = my_get_osfhandle(fd);
			HANDLE hDup;
			BOOL bOK =
				DuplicateHandle(GetCurrentProcess(), hFile,
						GetCurrentProcess(), &hDup, 0,
						FALSE, DUPLICATE_SAME_ACCESS);
			if(bOK) {
				fd2 = _open_osfhandle((intptr_t)hDup,0);
			}
			else {
				my_osmaperr(GetLastError());
				fd2 = -1;
			}
		}
#else
		fd2 = dup(fd);
#endif
		if (fd2 < 0) {
			msg("xtrabackup: Got error %d on dup\n",fd2);
		}
		my_close(fd, MYF(MY_WME));
	}
	return(fd2);
}

/* The following is used by row0merge for error reporting. Define a stub so we
can use fast index creation from XtraBackup. */
void
innobase_rec_reset(
/*===============*/
	TABLE*			table __attribute__((unused)))
{
}

/* The following is used by row0merge for error reporting. Define a stub so we
can use fast index creation from XtraBackup. */
void
innobase_rec_to_mysql(
/*==================*/
	TABLE*			table __attribute__((unused)),
	const rec_t*		rec __attribute__((unused)),
	const dict_index_t*	index __attribute__((unused)),
	const ulint*		offsets __attribute__((unused)))
{
}

} /* extern "C" */

#endif /* MYSQL_VERSION_ID < 50600 */

#if (MYSQL_VERSION_ID >= 50507) && (MYSQL_VERSION_ID < 50600)
/*
   As of MySQL 5.5.7, InnoDB uses thd_wait plugin service.
   We have to provide mock functions to avoid linker errors.
*/
#include <mysql/plugin.h>
#include <mysql/service_thd_wait.h>

void thd_wait_begin(MYSQL_THD thd, int wait_type)
{
	(void)thd;
	(void)wait_type;
	return;
}

void thd_wait_end(MYSQL_THD thd)
{
	(void)thd;
	return;
}

#endif /* (MYSQL_VERSION_ID >= 50507) && (MYSQL_VERSION_ID < 50600) */

#ifdef XTRADB_BASED
/******************************************************************//*******
Stub for an XtraDB-specific function called from row_merge_build_indexes().*/
ibool
thd_expand_fast_index_creation(
/*================================*/
	void*	thd __attribute__((unused)))
{
	return(FALSE);
}
#endif /* XTRADB_BASED */

#if MYSQL_VERSION_ID >= 50600

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
	ulint	i)			/*!< in: log file number in group */
{
	ibool	ret;
	os_offset_t	size;
	char	name[10000];
	ulint	dirnamelen;

	UT_NOT_USED(create_new_db);
	UT_NOT_USED(log_file_has_been_opened);
	UT_NOT_USED(k);
	ut_ad(k == 0);

	*log_file_created = FALSE;

	srv_normalize_path_for_win(srv_log_group_home_dir);

	dirnamelen = strlen(srv_log_group_home_dir);
	ut_a(dirnamelen < (sizeof name) - 10 - sizeof "ib_logfile");
	memcpy(name, srv_log_group_home_dir, dirnamelen);

	/* Add a path separator if needed. */
	if (dirnamelen && name[dirnamelen - 1] != SRV_PATH_SEPARATOR) {
		name[dirnamelen++] = SRV_PATH_SEPARATOR;
	}

	sprintf(name + dirnamelen, "%s%lu", "ib_logfile", (ulong) i);

	files[i] = os_file_create(innodb_file_log_key, name,
				  OS_FILE_OPEN, OS_FILE_NORMAL,
				  OS_LOG_FILE, &ret);
	if (ret == FALSE) {
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

		fil_space_create(name,
				 2 * k + SRV_LOG_SPACE_FIRST_ID, 0, FIL_LOG);
	}

	ut_a(fil_validate());

	ut_a(fil_node_create(name, srv_log_file_size,
			     2 * k + SRV_LOG_SPACE_FIRST_ID, FALSE));
	if (i == 0) {
		log_group_init(k, srv_n_log_files,
			       srv_log_file_size * UNIV_PAGE_SIZE,
			       2 * k + SRV_LOG_SPACE_FIRST_ID,
			       SRV_LOG_SPACE_FIRST_ID + 1); /* dummy arch
							    space id */
	}

	return(DB_SUCCESS);
}

#endif /* MYSQL_VERSION_ID >= 50600 */

/*****************************************************************//**
Parse, validate, and set the InnoDB variables from
innodb_log_group_home_dir and innodb_mirrored_log_group_groups
options.

@return TRUE if server variables set OK, FALSE otherwise */
ibool
xb_parse_log_group_home_dirs(void)
/*==============================*/
{
	ibool	ret = TRUE;
#if MYSQL_VERSION_ID < 50600
	ret = srv_parse_log_group_home_dirs(INNODB_LOG_DIR);

	if (ret == FALSE || innobase_mirrored_log_groups != 1) {

		msg("xtrabackup: syntax error in innodb_log_group_home_dir, "
		    "or a wrong number of mirrored log groups\n");
		ret = FALSE;
	}
	srv_n_log_groups = (ulint) innobase_mirrored_log_groups;
#else
	srv_normalize_path_for_win(srv_log_group_home_dir);

	if (strchr(srv_log_group_home_dir, ';')
	    || innobase_mirrored_log_groups != 1) {

		msg("syntax error in innodb_log_group_home_dir, "
		    "or a wrong number of mirrored log groups");
		ret = FALSE;
	}
#endif
	return ret;
}

/*****************************************************************//**
Set InnoDB to read-only mode. */
void
xb_set_innodb_read_only(void)
/*=========================*/
{
#if MYSQL_VERSION_ID >= 50600
	srv_read_only_mode = TRUE;
#else
	srv_read_only = TRUE;
#endif
}

/*****************************************************************//**
Adjust srv_fatal_semaphore_wait_threshold in a thread-safe manner.  */
void
xb_adjust_fatal_semaphore_wait_threshold(
/*=====================================*/
	ulint delta)	/*!<in: threshold adjustment delta in seconds */
{
#if MYSQL_VERSION_ID >= 50600
	os_increment_counter_by_amount(server_mutex,
				       srv_fatal_semaphore_wait_threshold,
				       delta);
#else
	mutex_enter(&kernel_mutex);
	srv_fatal_semaphore_wait_threshold += delta;
	mutex_exit(&kernel_mutex);
#endif
}

/*****************************************************************//**
Wrapper around around recv_check_cp_is_consistent() for handling
version differences.

@return TRUE if checkpoint info in the buffer is OK */
ibool
xb_recv_check_cp_is_consistent(
/*===========================*/
	const byte*	buf)	/*!<in: buffer containing checkpoint info */
{
#if MYSQL_VERSION_ID >= 50600
	return recv_check_cp_is_consistent(buf);
#else
	return recv_check_cp_is_consistent(const_cast<byte *>(buf));
#endif
}

/*****************************************************************//**
Initialize a index_field_t variable from a dict_field_t */
void
xb_dict_index_field_to_index_field(
/*===============================*/
	mem_heap_t*		heap __attribute__((unused)),/*!< in: heap for
						field name string allocation on
						5.5 or earlier version. */
	const dict_field_t*	dict_index_field,	/*!< in: field */
	index_field_t*		index_field)		/*!< out: field */
{
#if MYSQL_VERSION_ID >= 50600
	index_field->col_no = dict_col_get_no(dict_index_field->col);
#else
	index_field->field_name = mem_heap_strdup(heap,
						  dict_index_field->name);
#endif
	index_field->prefix_len = dict_index_field->prefix_len;
}
