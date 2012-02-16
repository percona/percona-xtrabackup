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

/* Page filters implementation */

#include <my_base.h>
#include <ut0mem.h>
#include "common.h"
#include "innodb_int.h"
#include "page_filt.h"
#include "fil_cur.h"

/* value of the --incremental option in xtrabackup.c */
extern LSN64 incremental_lsn;

/* defined in xtrabackup.c */
my_bool xb_write_delta_metadata(const char *filename, const xb_delta_info_t *
				info);

/************************************************************************
Write-through page filter. */
static my_bool pf_wt_init(xb_page_filt_ctxt_t *ctxt, char *dst_name,
			  xb_fil_cur_t *cursor);
static my_bool pf_wt_process(xb_page_filt_ctxt_t *ctxt, ds_file_t *dstfile);

xb_page_filt_t pf_write_through = {
	&pf_wt_init,
	&pf_wt_process,
	NULL,
	NULL
};

/************************************************************************
Incremental page filter. */
static my_bool pf_incremental_init(xb_page_filt_ctxt_t *ctxt, char *dst_name,
				   xb_fil_cur_t *cursor);
static my_bool pf_incremental_process(xb_page_filt_ctxt_t *ctxt,
				      ds_file_t *dstfile);
static my_bool pf_incremental_finalize(xb_page_filt_ctxt_t *ctxt,
				       ds_file_t *dstfile);
static void pf_incremental_deinit(xb_page_filt_ctxt_t *ctxt);

xb_page_filt_t pf_incremental = {
	&pf_incremental_init,
	&pf_incremental_process,
	&pf_incremental_finalize,
	&pf_incremental_deinit
};

/************************************************************************
Compact page filter. */
static my_bool pf_compact_init(xb_page_filt_ctxt_t *ctxt, char *dst_name,
				   xb_fil_cur_t *cursor);
static my_bool pf_compact_process(xb_page_filt_ctxt_t *ctxt,
				      ds_file_t *dstfile);
static my_bool pf_compact_finalize(xb_page_filt_ctxt_t *ctxt,
				       ds_file_t *dstfile);

xb_page_filt_t pf_compact = {
	&pf_compact_init,
	&pf_compact_process,
	&pf_compact_finalize,
	NULL
};

/************************************************************************
Initialize incremental page filter.

@return TRUE on success, FALSE on error. */
static my_bool
pf_incremental_init(xb_page_filt_ctxt_t *ctxt, char *dst_name,
		    xb_fil_cur_t *cursor)
{
	char		 		meta_name[FN_REFLEN];
	xb_delta_info_t	 		info;
	ulint				buf_size;
	xb_pf_incremental_ctxt_t	*cp =
		&(ctxt->u.pf_incremental_ctxt);

	ctxt->cursor = cursor;

	/* allocate buffer for incremental backup (4096 pages) */
	buf_size = (UNIV_PAGE_SIZE_MAX / 4 + 1) * UNIV_PAGE_SIZE_MAX;
	cp->delta_buf_base = ut_malloc(buf_size);
	bzero(cp->delta_buf_base, buf_size);
	cp->delta_buf = ut_align(cp->delta_buf_base, UNIV_PAGE_SIZE_MAX);

	/* write delta meta info */
	snprintf(meta_name, sizeof(meta_name), "%s%s", dst_name,
		 XB_DELTA_INFO_SUFFIX);
	info.page_size = cursor->page_size;
	if (!xb_write_delta_metadata(meta_name, &info)) {
		msg("[%02lu] xtrabackup: Error: "
		    "failed to write meta info for %s\n",
		    cursor->thread_n, cursor->path);
		return(FALSE);
	}

	/* change the target file name, since we are only going to write
	delta pages */
	strcat(dst_name, ".delta");

	mach_write_to_4(cp->delta_buf, 0x78747261UL); /*"xtra"*/
	cp->npages = 1;

	return(TRUE);
}

/************************************************************************
Run the next batch of pages through incremental page filter.

@return TRUE on success, FALSE on error. */
static my_bool
pf_incremental_process(xb_page_filt_ctxt_t *ctxt, ds_file_t *dstfile)
{
	ulint 				i;
	xb_fil_cur_t			*cursor = ctxt->cursor;
	ulint				page_size = cursor->page_size;
	byte				*page;
	byte 				*buf_end;
	xb_pf_incremental_ctxt_t	*cp = &(ctxt->u.pf_incremental_ctxt);

	buf_end = cursor->buf + cursor->buf_read;
	for (i = 0, page = cursor->buf; i < cursor->buf_npages;
	     i++, page += page_size) {
		if (ut_dulint_cmp(incremental_lsn,
				  MACH_READ_64(page + FIL_PAGE_LSN)) >= 0) {
			continue;
		}

		/* updated page */
		if (cp->npages == page_size / 4) {
			/* flush buffer */
			if (ds_write(dstfile, cp->delta_buf,
				     cp->npages * page_size)) {
				return(FALSE);
			}

			/* clear buffer */
			bzero(cp->delta_buf, page_size / 4 * page_size);
			mach_write_to_4(cp->delta_buf, 0x78747261UL); /*"xtra"*/
			cp->npages = 1;
		}

		mach_write_to_4(cp->delta_buf + cp->npages * 4,
				cursor->buf_page_no + i);
		memcpy(cp->delta_buf + cp->npages * page_size, page, page_size);

		cp->npages++;
	}

	return(TRUE);
}

/************************************************************************
Flush the incremental page filter's buffer.

@return TRUE on success, FALSE on error. */
static my_bool
pf_incremental_finalize(xb_page_filt_ctxt_t *ctxt, ds_file_t *dstfile)
{
	xb_fil_cur_t			*cursor = ctxt->cursor;
	ulint				page_size = cursor->page_size;
	xb_pf_incremental_ctxt_t	*cp = &(ctxt->u.pf_incremental_ctxt);

	if (cp->npages != page_size / 4) {
		mach_write_to_4(cp->delta_buf + cp->npages * 4, 0xFFFFFFFFUL);
	}

	/* Mark the final block */
	mach_write_to_4(cp->delta_buf, 0x58545241UL); /*"XTRA"*/

	/* flush buffer */
	if (ds_write(dstfile, cp->delta_buf, cp->npages * page_size)) {
		return(FALSE);
	}

	return(TRUE);
}

/************************************************************************
Free the incremental page filter's buffer. */
static void
pf_incremental_deinit(xb_page_filt_ctxt_t *ctxt)
{
	xb_pf_incremental_ctxt_t	*cp = &(ctxt->u.pf_incremental_ctxt);

	if (cp->delta_buf_base != NULL) {
		ut_free(cp->delta_buf_base);
	}
}

/************************************************************************
Initialize the write-through page filter.

@return TRUE on success, FALSE on error. */
static my_bool
pf_wt_init(xb_page_filt_ctxt_t *ctxt, char *dst_name __attribute__((unused)),
	   xb_fil_cur_t *cursor)
{
	ctxt->cursor = cursor;

	return(TRUE);
}

/************************************************************************
Write the next batch of pages to the destination datasinkr.

@return TRUE on success, FALSE on error. */
static my_bool
pf_wt_process(xb_page_filt_ctxt_t *ctxt, ds_file_t *dstfile)
{
	xb_fil_cur_t			*cursor = ctxt->cursor;

	if (ds_write(dstfile, cursor->buf, cursor->buf_read)) {
		return(FALSE);
	}

	return(TRUE);
}

/************************************************************************
Initialize the compact page filter.

@return TRUE on success, FALSE on error. */
static my_bool
pf_compact_init(xb_page_filt_ctxt_t *ctxt,
		char *dst_name __attribute__((unused)), xb_fil_cur_t *cursor)
{
	xb_pf_compact_ctxt_t		*cp = &(ctxt->u.pf_compact_ctxt);

	/* Don't compact the system table space */
	cp->skip = cursor->is_system;

	return(TRUE);
}

static my_bool
check_if_skip_page(byte *page __attribute__((unused)))
{
	return(FALSE);
}

/************************************************************************
Run the next batch of pages through the compact page filter.

@return TRUE on success, FALSE on error. */
static my_bool
pf_compact_process(xb_page_filt_ctxt_t *ctxt, ds_file_t *dstfile)
{
	xb_fil_cur_t		*cursor = ctxt->cursor;
	ulint			 page_size = cursor->page_size;
	byte			*page;
	byte 			*buf_end;
	byte			*write_from;
	xb_pf_compact_ctxt_t	*cp = &(ctxt->u.pf_compact_ctxt);

	if (cp->skip) {
		return(!ds_write(dstfile, cursor->buf, cursor->buf_read));
	}

	write_from = NULL;
	buf_end = cursor->buf + cursor->buf_read;
	for (page = cursor->buf; page < buf_end; page += page_size) {
		if (!check_if_skip_page(page)) {
			if (write_from == NULL) {
				write_from = page;
			}
			continue;
		}
	}

	/* Write the remaining pages in the buffer, if any */
	if (write_from != NULL &&
	    ds_write(dstfile, write_from, buf_end - write_from)) {
		return(FALSE);
	}

	return(TRUE);
}

/************************************************************************
Close the compact filter's page map stream.

@return TRUE on success, FALSE on error. */
static my_bool
pf_compact_finalize(xb_page_filt_ctxt_t *ctxt, ds_file_t *dstfile)
{
	xb_fil_cur_t			*cursor = ctxt->cursor;
	ulint				page_size = cursor->page_size;
	xb_pf_incremental_ctxt_t	*cp = &(ctxt->u.pf_incremental_ctxt);

	if (cp->npages != page_size / 4) {
		mach_write_to_4(cp->delta_buf + cp->npages * 4, 0xFFFFFFFFUL);
	}

	mach_write_to_4(cp->delta_buf, 0x58545241UL); /*"XTRA"*/

	/* flush buffer */
	if (ds_write(dstfile, cp->delta_buf, cp->npages * page_size)) {
		return(FALSE);
	}

	return(TRUE);
}
