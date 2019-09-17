/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009-2013 Percona LLC and/or its affiliates.
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

*******************************************************/

/* Source file cursor implementation */

#include <my_base.h>

#include <univ.i>
#include <fil0fil.h>
#include <srv0start.h>
#include <trx0sys.h>
#include <fsp0sysspace.h>

#include "fil_cur.h"
#include "common.h"
#include "read_filt.h"
#include "xtrabackup.h"

/* Size of read buffer in pages (640 pages = 10M for 16K sized pages) */
#define XB_FIL_CUR_PAGES 640

/***********************************************************************
Extracts the relative path ("database/table.ibd") of a tablespace from a
specified possibly absolute path.

For user tablespaces both "./database/table.ibd" and
"/remote/dir/database/table.ibd" result in "database/table.ibd".

For system tablepsaces (i.e. When is_system is TRUE) both "/remote/dir/ibdata1"
and "./ibdata1" yield "ibdata1" in the output. */
const char *
xb_get_relative_path(
/*=================*/
	const char*	path,		/*!< in: tablespace path (either
			  		relative or absolute) */
	ibool		is_system)	/*!< in: TRUE for system tablespaces,
					i.e. when only the filename must be
					returned. */
{
	const char *next;
	const char *cur;
	const char *prev;

	prev = NULL;
	cur = path;

	while ((next = strchr(cur, OS_PATH_SEPARATOR)) != NULL) {

		prev = cur;
		cur = next + 1;
	}

	if (is_system) {

		return(cur);
	} else {

		return((prev == NULL) ? cur : prev);
	}

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
	ut_a(!node->being_extended);

	if (!node->is_open) {

		mutex_exit(&fil_system->mutex);

		return;
	}

	ret = os_file_close(node->handle);
	ut_a(ret);

	node->is_open = false;

	ut_a(fil_system->n_open > 0);
	fil_system->n_open--;
	fil_n_file_opened--;

	if (node->space->purpose == FIL_TYPE_TABLESPACE &&
	    fil_is_user_tablespace_id(node->space->id)) {

		ut_a(UT_LIST_GET_LEN(fil_system->LRU) > 0);

		/* The node is in the LRU list, remove it */
		UT_LIST_REMOVE(fil_system->LRU, node);
	}

	mutex_exit(&fil_system->mutex);
}

/************************************************************************
Open a source file cursor and initialize the associated read filter.

@return XB_FIL_CUR_SUCCESS on success, XB_FIL_CUR_SKIP if the source file must
be skipped and XB_FIL_CUR_ERROR on error. */
xb_fil_cur_result_t
xb_fil_cur_open(
/*============*/
	xb_fil_cur_t*	cursor,		/*!< out: source file cursor */
	xb_read_filt_t*	read_filter,	/*!< in/out: the read filter */
	fil_node_t*	node,		/*!< in: source tablespace node */
	uint		thread_n)	/*!< thread number for diagnostics */
{
	page_size_t	page_size(0, 0, false);
	ulint		page_size_shift;
	bool		success;

	/* Initialize these first so xb_fil_cur_close() handles them correctly
	in case of error */
	cursor->orig_buf = NULL;
	cursor->node = NULL;

	cursor->space_id = node->space->id;
	cursor->space_flags = node->space->flags;
	cursor->is_system = !fil_is_user_tablespace_id(node->space->id);

	strncpy(cursor->abs_path, node->name, sizeof(cursor->abs_path));

	/* Get the relative path for the destination tablespace name, i.e. the
	one that can be appended to the backup root directory. Non-system
	tablespaces may have absolute paths for remote tablespaces in MySQL
	5.6+. We want to make "local" copies for the backup. */
	strncpy(cursor->rel_path,
		xb_get_relative_path(cursor->abs_path, cursor->is_system),
		sizeof(cursor->rel_path));

	/* In the backup mode we should already have a tablespace handle created
	by fil_load_single_table_tablespace() unless it is a system
	tablespace. Otherwise we open the file here. */
	if (cursor->is_system || !srv_backup_mode || srv_close_files) {
		node->handle =
			os_file_create_simple_no_error_handling(0, node->name,
								OS_FILE_OPEN,
								OS_FILE_READ_ONLY,
								srv_read_only_mode,
								&success);
		if (!success) {
			/* The following call prints an error message */
			os_file_get_last_error(TRUE);

			msg("[%02u] xtrabackup: error: cannot open "
			    "tablespace %s\n",
			    thread_n, cursor->abs_path);

			return(XB_FIL_CUR_ERROR);
		}
		mutex_enter(&fil_system->mutex);

		node->is_open = true;

		fil_system->n_open++;
		fil_n_file_opened++;

		if (node->space->purpose == FIL_TYPE_TABLESPACE &&
		    fil_is_user_tablespace_id(node->space->id)) {

			/* Put the node to the LRU list */
			UT_LIST_ADD_FIRST(fil_system->LRU, node);
		}

		mutex_exit(&fil_system->mutex);
	}

	ut_ad(node->is_open);

	cursor->node = node;
	cursor->file = node->handle;
	cursor->block_size = node->block_size;

	if (my_fstat(cursor->file.m_file, &cursor->statinfo, MYF(MY_WME))) {
		msg("[%02u] xtrabackup: error: cannot stat %s\n",
		    thread_n, cursor->abs_path);

		xb_fil_cur_close(cursor);

		return(XB_FIL_CUR_ERROR);
	}

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT
	    || srv_unix_file_flush_method == SRV_UNIX_O_DIRECT_NO_FSYNC) {

		os_file_set_nocache(cursor->file.m_file, node->name, "OPEN");
	}

	posix_fadvise(cursor->file.m_file, 0, 0, POSIX_FADV_SEQUENTIAL);

	/* Determine the page size */
	page_size.copy_from(xb_get_zip_size(cursor->file, &success));
	if (!success) {
		xb_fil_cur_close(cursor);
		return(XB_FIL_CUR_SKIP);
	} else if (page_size.is_compressed()) {
		page_size_shift = get_bit_shift(page_size.physical());
		msg("[%02u] %s is compressed with page size = "
		    "%lu bytes\n", thread_n, node->name, page_size.physical());
		if (page_size_shift < 10 || page_size_shift > 14) {
			msg("[%02u] xtrabackup: Error: Invalid "
			    "page size: %lu.\n", thread_n, page_size.physical());
			ut_error;
		}
	} else {
		page_size_shift = UNIV_PAGE_SIZE_SHIFT;
	}
	cursor->page_size = page_size.physical();
	cursor->page_size_shift = page_size_shift;
	cursor->zip_size = page_size.is_compressed() ? page_size.physical() : 0;

	ut_a(opt_read_buffer_size >= UNIV_PAGE_SIZE);
	/* Allocate read buffer */
	cursor->buf_size = opt_read_buffer_size;
	cursor->orig_buf = static_cast<byte *>
		(ut_malloc_nokey(cursor->buf_size + UNIV_PAGE_SIZE));
	cursor->buf = static_cast<byte *>
		(ut_align(cursor->orig_buf, UNIV_PAGE_SIZE));

	cursor->buf_read = 0;
	cursor->buf_npages = 0;
	cursor->buf_offset = 0;
	cursor->buf_page_no = 0;
	cursor->thread_n = thread_n;

	cursor->space_size = cursor->statinfo.st_size / page_size.physical();

	cursor->read_filter = read_filter;
	cursor->read_filter->init(&cursor->read_filter_ctxt, cursor,
				  node->space->id);

	cursor->scratch = static_cast<byte *>
		(ut_malloc_nokey(cursor->page_size * 2));
	cursor->decrypt = static_cast<byte *>
		(ut_malloc_nokey(cursor->page_size));

	memcpy(cursor->encryption_key, node->space->encryption_key,
	       sizeof(cursor->encryption_key));
	memcpy(cursor->encryption_iv, node->space->encryption_iv,
	       sizeof(cursor->encryption_iv));
	cursor->encryption_klen = node->space->encryption_klen;

	return(XB_FIL_CUR_SUCCESS);
}

/************************************************************************
Reads and verifies the next block of pages from the source
file. Positions the cursor after the last read non-corrupted page.

@return XB_FIL_CUR_SUCCESS if some have been read successfully, XB_FIL_CUR_EOF
if there are no more pages to read and XB_FIL_CUR_ERROR on error. */
xb_fil_cur_result_t
xb_fil_cur_read(
/*============*/
	xb_fil_cur_t*	cursor)	/*!< in/out: source file cursor */
{
	dberr_t			err;
	byte*			page, *page_to_check;
	ulint			i;
	ulint			npages;
	ulint			retry_count;
	xb_fil_cur_result_t	ret;
	ib_uint64_t		offset;
	ib_uint64_t		to_read;
	ulong			n_read;
	page_size_t		page_size(cursor->zip_size != 0 ?
					  cursor->zip_size : cursor->page_size,
					  cursor->page_size,
					  cursor->zip_size != 0);
	IORequest		read_request(IORequest::READ |
					     IORequest::NO_COMPRESSION);

	read_request.encryption_algorithm(Encryption::AES);
	read_request.encryption_key(cursor->encryption_key,
				    cursor->encryption_klen,
				    cursor->encryption_iv);
	read_request.block_size(cursor->block_size);

	cursor->read_filter->get_next_batch(&cursor->read_filter_ctxt,
					    &offset, &to_read);

	if (to_read == 0LL) {
		return(XB_FIL_CUR_EOF);
	}

	if (to_read > (ib_uint64_t) cursor->buf_size) {
		to_read = (ib_uint64_t) cursor->buf_size;
	}

	xb_a(to_read > 0 && to_read <= 0xFFFFFFFFLL);

	if (to_read % cursor->page_size != 0 &&
	    offset + to_read == (ib_uint64_t) cursor->statinfo.st_size) {

		if (to_read < (ib_uint64_t) cursor->page_size) {
			msg("[%02u] xtrabackup: Warning: junk at the end of "
			    "%s:\n", cursor->thread_n, cursor->abs_path);
			msg("[%02u] xtrabackup: Warning: offset = %llu, "
			    "to_read = %llu\n",
			    cursor->thread_n,
			    (unsigned long long) offset,
			    (unsigned long long) to_read);

			return(XB_FIL_CUR_EOF);
		}

		to_read = (ib_uint64_t) (((ulint) to_read) &
					~(cursor->page_size - 1));
	}

	xb_a(to_read % cursor->page_size == 0);

	retry_count = 10;
	ret = XB_FIL_CUR_SUCCESS;

read_retry:
	xtrabackup_io_throttling();

	cursor->buf_read = 0;
	cursor->buf_npages = 0;
	cursor->buf_offset = offset;
	cursor->buf_page_no = (ulint) (offset >> cursor->page_size_shift);

	err = os_file_read_no_error_handling(read_request, cursor->file,
		cursor->buf, offset, to_read, &n_read);
	if (err != DB_SUCCESS) {
		if (err == DB_IO_ERROR) {
			/* If the file is truncated by MySQL, os_file_read will
			fail with DB_IO_ERROR, but XtraBackup must treat this
			error as non critical. */
			if (my_fstat(cursor->file.m_file, &cursor->statinfo,
				     MYF(MY_WME))) {
				msg("[%02u] xtrabackup: error: cannot stat "
				    "%s\n", cursor->thread_n, cursor->abs_path);
				return(XB_FIL_CUR_ERROR);
			}
			/* Check if we reached EOF */
			if ((ulonglong) cursor->statinfo.st_size >
			    offset + n_read) {
				return(XB_FIL_CUR_ERROR);
			}
		}
	}

	npages = n_read >> cursor->page_size_shift;

	/* check pages for corruption and re-read if necessary. i.e. in case of
	partially written pages */
	for (page = cursor->buf, i = 0; i < npages;
	     page += cursor->page_size, i++) {
		page_to_check = page;

		if (Encryption::is_encrypted_page(page)) {
			dberr_t		ret;
			Encryption	encryption(
				read_request.encryption_algorithm());

			page_to_check = cursor->decrypt;
			memcpy(cursor->decrypt, page, cursor->page_size);
			ret = encryption.decrypt(read_request, cursor->decrypt,
						 cursor->page_size,
						 cursor->scratch,
						 cursor->page_size);
			if (ret != DB_SUCCESS) {
				goto corruption;
			}

			if (Compression::is_compressed_page(cursor->decrypt)) {
				if (os_file_decompress_page(false,
				    cursor->decrypt, cursor->scratch,
				    cursor->page_size) != DB_SUCCESS) {
					goto corruption;
				}
			}
		}

		if (Compression::is_compressed_page(page)) {

			page_to_check = cursor->decrypt;
			memcpy(cursor->decrypt, page, cursor->page_size);
			if (os_file_decompress_page(false, cursor->decrypt,
			    cursor->scratch, cursor->page_size) != DB_SUCCESS) {
				goto corruption;
			}

		}

		if (buf_page_is_corrupted(TRUE, page_to_check, page_size,
					  false)) {

corruption:

			ulint page_no = cursor->buf_page_no + i;

			if (cursor->is_system &&
			    page_no >= FSP_EXTENT_SIZE &&
			    page_no < FSP_EXTENT_SIZE * 3) {
				/* skip doublewrite buffer pages */
				xb_a(cursor->page_size == UNIV_PAGE_SIZE);
				msg("[%02u] xtrabackup: "
				    "Page %lu is a doublewrite buffer page, "
				    "skipping.\n", cursor->thread_n, page_no);
			} else {
				retry_count--;
				if (retry_count == 0) {
					msg("[%02u] xtrabackup: "
					    "Error: failed to read page after "
					    "10 retries. File %s seems to be "
					    "corrupted.\n", cursor->thread_n,
					    cursor->abs_path);
					ret = XB_FIL_CUR_ERROR;
					break;
				}
				msg("[%02u] xtrabackup: "
				    "Database page corruption detected at page "
				    "%lu, retrying...\n", cursor->thread_n,
				    page_no);

				os_thread_sleep(100000);

				goto read_retry;
			}
		}
		cursor->buf_read += cursor->page_size;
		cursor->buf_npages++;
	}

	cursor->read_filter->update(&cursor->read_filter_ctxt, n_read, cursor);

	posix_fadvise(cursor->file.m_file, offset, to_read, POSIX_FADV_DONTNEED);

	return(ret);
}

/************************************************************************
Close the source file cursor opened with xb_fil_cur_open() and its
associated read filter. */
void
xb_fil_cur_close(
/*=============*/
	xb_fil_cur_t *cursor)	/*!< in/out: source file cursor */
{
	cursor->read_filter->deinit(&cursor->read_filter_ctxt);

	if (cursor->scratch != NULL) {
		ut_free(cursor->scratch);
	}
	if (cursor->decrypt != NULL) {
		ut_free(cursor->decrypt);
	}
	if (cursor->orig_buf != NULL) {
		ut_free(cursor->orig_buf);
	}
	if (cursor->node != NULL) {
		xb_fil_node_close_file(cursor->node);
		cursor->file = XB_FILE_UNDEFINED;
	}
}
