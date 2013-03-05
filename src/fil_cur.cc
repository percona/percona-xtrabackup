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

/* Source file cursor implementation */

#include <my_base.h>
#include "innodb_int.h"
#include "fil_cur.h"
#include "common.h"
#include "xtrabackup.h"

/* Size of read buffer in pages */
#define XB_FIL_CUR_PAGES 64

/************************************************************************
Open a source file cursor.

@return XB_FIL_CUR_SUCCESS on success, XB_FIL_CUR_SKIP if the source file must
be skipped and XB_FIL_CUR_ERROR on error. */
xb_fil_cur_result_t
xb_fil_cur_open(
/*============*/
	xb_fil_cur_t*	cursor,		/*!< out: source file cursor */
	fil_node_t*	node,		/*!< in: source tablespace node */
	uint		thread_n)	/*!< thread number for diagnostics */
{
	ulint	page_size;
	ulint	page_size_shift;
	ulint	zip_size;

	/* Initialize these first so xb_fil_cur_close() handles them correctly
	in case of error */
	cursor->orig_buf = NULL;
	cursor->file = XB_FILE_UNDEFINED;

	cursor->space_id = node->space->id;
	cursor->is_system = trx_sys_sys_space(node->space->id);

	/* Make the file path relative to the backup root,
	i.e. "ibdata1" for system tablespace or database/table.ibd for
	per-table spaces. */
	if (cursor->is_system) {
		char *next, *p;

		p = node->name;
		while ((next = strstr(p, SRV_PATH_SEPARATOR_STR)) != NULL) {
			p = next + 1;
		}
		strncpy(cursor->path, p, sizeof(cursor->path));
	} else {
		/* file per table style "./database/table.ibd" */
		strncpy(cursor->path, node->name, sizeof(cursor->path));
	}

	/* Open the file */

	if (my_stat(node->name, &cursor->statinfo, MYF(MY_WME)) == NULL) {
		msg("[%02u] xtrabackup: Warning: cannot stat %s\n",
		    thread_n, node->name);
		return(XB_FIL_CUR_SKIP);
	}

	ibool success;

	cursor->file =
		xb_file_create_no_error_handling(node->name,
						 OS_FILE_OPEN,
						 OS_FILE_READ_ONLY,
						 &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		msg("[%02u] xtrabackup: Warning: cannot open %s\n"
		    "[%02u] xtrabackup: Warning: We assume the "
		    "table was dropped or renamed during "
		    "xtrabackup execution and ignore the file.\n",
		    thread_n, node->name, thread_n);
		return(XB_FIL_CUR_SKIP);
	}

	xb_file_set_nocache(cursor->file, node->name, "OPEN");
	posix_fadvise(cursor->file, 0, 0, POSIX_FADV_SEQUENTIAL);

	/* Determine the page size */
	zip_size = xb_get_zip_size(cursor->file);
	if (zip_size == ULINT_UNDEFINED) {
		os_file_close(cursor->file);
		return(XB_FIL_CUR_SKIP);
	} else if (zip_size) {
		page_size = zip_size;
		page_size_shift = get_bit_shift(page_size);
		msg("[%02u] %s is compressed with page size = "
		    "%lu bytes\n", thread_n, node->name, page_size);
		if (page_size_shift < 10 || page_size_shift > 14) {
			msg("[%02u] xtrabackup: Error: Invalid "
			    "page size: %lu.\n", thread_n, page_size);
			ut_error;
		}
	} else {
		page_size = UNIV_PAGE_SIZE;
		page_size_shift = UNIV_PAGE_SIZE_SHIFT;
	}
	cursor->page_size = page_size;
	cursor->page_size_shift = page_size_shift;
	cursor->zip_size = zip_size;

	/* Allocate read buffer */
	cursor->buf_size = XB_FIL_CUR_PAGES * page_size;
	cursor->orig_buf = static_cast<byte *>
		(ut_malloc(cursor->buf_size + UNIV_PAGE_SIZE));
	cursor->buf = static_cast<byte *>
		(ut_align(cursor->orig_buf, UNIV_PAGE_SIZE));

	cursor->offset = 0;
	cursor->buf_read = 0;
	cursor->buf_npages = 0;
	cursor->buf_offset = 0;
	cursor->buf_page_no = 0;
	cursor->thread_n = thread_n;

	cursor->space_size = cursor->statinfo.st_size / page_size;

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
	ibool			success;
	ulint			page_size;
	ulint			offset_high;
	ulint			offset_low;
	byte*			page;
	ulint			i;
	ulint			npages;
	ulint			retry_count;
	xb_fil_cur_result_t	ret;
	IB_INT64		to_read;

	page_size = cursor->page_size;

	offset_high = (ulint) (cursor->offset >> 32);
	offset_low = (ulint) (cursor->offset & 0xFFFFFFFFUL);

	to_read = (IB_INT64) cursor->statinfo.st_size - cursor->offset;

	if (to_read == 0LL) {
		return(XB_FIL_CUR_EOF);
	}

	if (to_read > (IB_INT64) cursor->buf_size) {
		to_read = (IB_INT64) cursor->buf_size;
	}
	ut_a(to_read > 0 && to_read <= 0xFFFFFFFFLL);
	ut_a(to_read % page_size == 0);

	npages = (ulint) (to_read >> cursor->page_size_shift);

	retry_count = 10;
	ret = XB_FIL_CUR_SUCCESS;

read_retry:
	xtrabackup_io_throttling();

	cursor->buf_read = 0;
	cursor->buf_npages = 0;
	cursor->buf_offset = cursor->offset;
	cursor->buf_page_no = (ulint) (cursor->offset >>
				       cursor->page_size_shift);

	success = os_file_read(cursor->file, cursor->buf,
			       offset_low, offset_high, to_read);
	if (!success) {
		return(XB_FIL_CUR_ERROR);
	}

	/* check pages for corruption and re-read if necessary. i.e. in case of
	partially written pages */
	for (page = cursor->buf, i = 0; i < npages; page += page_size, i++) {
		if (xb_buf_page_is_corrupted(page, cursor->zip_size))
		{
			ulint page_no = cursor->buf_page_no + i;

			if (cursor->is_system &&
			    page_no >= FSP_EXTENT_SIZE &&
			    page_no < FSP_EXTENT_SIZE * 3) {
				/* skip doublewrite buffer pages */
				ut_a(page_size == UNIV_PAGE_SIZE);
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
					    cursor->path);
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
		cursor->buf_read += page_size;
		cursor->buf_npages++;
	}

	posix_fadvise(cursor->file, 0, 0, POSIX_FADV_DONTNEED);

	cursor->offset += page_size * i;

	return(ret);
}

/************************************************************************
Close the source file cursor opened with xb_fil_cur_open(). */
void
xb_fil_cur_close(
/*=============*/
	xb_fil_cur_t *cursor)	/*!< in/out: source file cursor */
{
	if (cursor->orig_buf != NULL) {
		ut_free(cursor->orig_buf);
	}
	if (cursor->file != XB_FILE_UNDEFINED) {
		os_file_close(cursor->file);
	}
}
