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

/* Page filter interface */

#ifndef XB_PAGE_FILT_H
#define XB_PAGE_FILT_H

#include "fil_cur.h"
#include "datasink.h"

/* Incremental page filter context */
typedef struct {
	byte		*delta_buf_base;
	byte		*delta_buf;
	ulint		 npages;
} xb_pf_incremental_ctxt_t;

/* Compact page filter context */
typedef struct {
	my_bool	skip;
} xb_pf_compact_ctxt_t;

/* Page filter context used as an opaque structure by callers */
typedef struct {
	xb_fil_cur_t	*cursor;
	union {
		xb_pf_incremental_ctxt_t	pf_incremental_ctxt;
		xb_pf_compact_ctxt_t		pf_compact_ctxt;
	} u;
} xb_page_filt_ctxt_t;


typedef struct {
	my_bool	(*init)(xb_page_filt_ctxt_t *ctxt, char *dst_name,
			xb_fil_cur_t *cursor);
	my_bool	(*process)(xb_page_filt_ctxt_t *ctxt, ds_file_t *dstfile);
	my_bool	(*finalize)(xb_page_filt_ctxt_t *, ds_file_t *dstfile);
	void (*deinit)(xb_page_filt_ctxt_t *);
} xb_page_filt_t;

extern xb_page_filt_t pf_write_through;
extern xb_page_filt_t pf_incremental;
extern xb_page_filt_t pf_compact;

#endif /* XB_PAGE_FILT_H */
