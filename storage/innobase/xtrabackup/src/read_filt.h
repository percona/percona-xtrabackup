/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009, 2021 Percona Inc.
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

/* Data file read filter interface */

#ifndef XB_READ_FILT_H
#define XB_READ_FILT_H

#include "changed_page_tracking.h"

struct xb_fil_cur_t;

/* The read filter context */
struct xb_read_filt_ctxt_t {
  uint64_t offset;             /*!< current file offset */
  uint64_t data_file_size;     /*!< data file size */
  uint64_t buffer_capacity;    /*!< read buffer capacity */
  ulint space_id;              /*!< space id */
  ulint page_size;                    /*!< page size */
  ulint filter_batch_end;             /*!< the ending page id of the
                                      current changed page block in
                                      the page tracking */
};

/* The read filter */
struct xb_read_filt_t {
  void (*init)(xb_read_filt_ctxt_t *ctxt, const xb_fil_cur_t *cursor,
               ulint space_id);
  void (*get_next_batch)(xb_fil_cur_t *ctxt, uint64_t *read_batch_start,
                         uint64_t *read_batch_len);
  void (*deinit)(xb_read_filt_ctxt_t *ctxt);
  void (*update)(xb_read_filt_ctxt_t *ctxt, uint64_t len,
                 const xb_fil_cur_t *cursor);
};

extern xb_read_filt_t rf_pass_through;
extern xb_read_filt_t rf_page_tracking;
#ifdef UNIV_DEBUG
void verify_skipped_pages(ulint next_page_id, xb_fil_cur_t *cursor);
#endif  // UNIV_DEBUG
#endif
