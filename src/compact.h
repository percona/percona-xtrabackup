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

#ifndef XB_COMPACT_H
#define XB_COMPACT_H

#include "write_filt.h"

/* Compact page filter context */
typedef struct {
	my_bool		 skip;
	ds_ctxt_t	*ds_buffer;
	ds_file_t	*buffer;
	INDEX_ID_T	 clustered_index;
	my_bool		 clustered_index_found;
	my_bool		 inside_skipped_range;
	ulint		 free_limit;
} xb_wf_compact_ctxt_t;

/******************************************************************************
Expand the data files according to the skipped pages maps created by --compact.
@return TRUE on success, FALSE on failure. */
my_bool xb_expand_datafiles(void);

#endif
