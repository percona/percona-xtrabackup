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

#include <my_base.h>
#include <mysql_com.h>
#include "innodb_int.h"
#include <srv0srv.h>
#include <ha_prototypes.h>
#include <trx0trx.h>
#include "common.h"

extern long innobase_lock_wait_timeout;

char *opt_mysql_tmpdir = NULL;
MY_TMPDIR mysql_tmpdir_list;


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
#if MYSQL_VERSION_ID > 50500
	return os_file_create(0 /* innodb_file_data_key */,
			      name, create_mode, purpose, type, success);
#else
	return os_file_create(name, create_mode, purpose, type, success);
#endif
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

	ret = os_file_set_size(path, *file,
			       FIL_IBD_FILE_INITIAL_SIZE * UNIV_PAGE_SIZE, 0);
	if (!ret) {
		msg("xtrabackup: cannot set size for file %s\n", path);
		os_file_close(*file);
		os_file_delete(path);
		return ret;
	}

	buf = ut_malloc(3 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf, UNIV_PAGE_SIZE);

	memset(page, '\0', UNIV_PAGE_SIZE);

	fsp_header_init_fields(page, space_id, flags);
	mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

	if (!(flags & DICT_TF_ZSSIZE_MASK)) {
		buf_flush_init_for_writing(page, NULL, 0);

		ret = os_file_write(path, *file, page, 0, 0, UNIV_PAGE_SIZE);
	}
	else {
		page_zip_des_t	page_zip;
		ulint		zip_size;

		zip_size = (PAGE_ZIP_MIN_SIZE >> 1)
			<< ((flags & DICT_TF_ZSSIZE_MASK)
			    >> DICT_TF_ZSSIZE_SHIFT);
		page_zip_set_size(&page_zip, zip_size);
		page_zip.data = page + UNIV_PAGE_SIZE;
		fprintf(stderr, "zip_size = %lu\n", zip_size);

#ifdef UNIV_DEBUG
		page_zip.m_start =
#endif /* UNIV_DEBUG */
			page_zip.m_end = page_zip.m_nonempty =
			page_zip.n_blobs = 0;

		buf_flush_init_for_writing(page, &page_zip, 0);

		ret = os_file_write(path, *file, page_zip.data, 0, 0,
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

int
mysql_get_identifier_quote_char(
	trx_t*		trx,
	const char*	name,
	ulint		namelen)
{
	(void)trx;
	(void)name;
	(void)namelen;
	return '"';
}

void
innobase_print_identifier(
	FILE*	f,
	trx_t*	trx __attribute__((unused)),
	ibool	table_id __attribute__((unused)),
	const char*	name,
	ulint	namelen)
{
	const char*	s	= name;
	const char*	e = s + namelen;
	int		q;

	q = '"';

	putc(q, f);
	while (s < e) {
		int	c = *s++;
		if (c == q) {
			putc(c, f);
		}
		putc(c, f);
	}
	putc(q, f);
}

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

#if MYSQL_VERSION_ID >= 50507
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

#endif /* MYSQL_VERSION_ID >= 50507 */
