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
  ulint merge_gap;                    /*!< largest changed-page gap to
                                    combine into one read, in pages of
                                    page_size (--page-tracking-merge-gap
                                    scaled for compressed tablespaces,
                                    or the read request cost in
                                    pages) */
  /* Statistics accumulated while the file is read, reported by the
  filter's deinit in one log line; they never influence any decision.

  Shared example used in the field comments below: changed pages

      1,2,3,4   7   20,21          (3 runs of consecutive pages)

  with merge_gap = 4. A gap counts the unchanged pages BETWEEN two
  changed pages (between 4 and 7 it is 2: pages 5,6 - not the id
  difference 3). The gap of 2 (pages 5,6) is combined across, so
  [1-7] becomes one read group; the gap of 12 (pages 8..19) is not,
  so [20-21] starts a second group. Every unchanged page between two
  changed pages ends up in exactly one of stat_filler_pages (its gap
  was combined: the page was read) or stat_skipped_pages (its gap was
  refused: the page was seeked past). */
  ulint stat_batches;             /*!< pread requests actually issued.
                                  Example: 2 - one per group; exceeds
                                  stat_groups only when a group is
                                  larger than --read-buffer-size and is
                                  read in buffer-sized sequential
                                  pieces */
  ulint stat_total_changed_pages; /*!< pages the server tracked as
                                  changed for this file: the work that
                                  must be copied regardless of
                                  grouping. Example: 7
                                  (1,2,3,4,7,20,21) */
  ulint stat_groups;              /*!< read groups formed = distinct
                                  disk locations read = seeks.
                                  Example: 2 ([1-7] and [20-21]). The
                                  log line's "ranges" = stat_groups +
                                  stat_combined_gaps is what merge-gap=0
                                  would have read: here 3, so combining
                                  saved one request */
  ulint stat_combined_gaps;       /*!< gaps of unchanged pages combined
                                  across (gap <= merge_gap), each joining
                                  two ranges into one read. Example: 1
                                  (the 2-page gap between 4 and 7) */
  ulint stat_filler_pages;        /*!< unchanged pages read only as
                                  filler inside combined gaps, then
                                  dropped by the incremental write
                                  filter's LSN check - they cost read
                                  volume only, never backup size.
                                  Example: 2 (pages 5,6). read
                                  amplification = (total_changed +
                                  filler) / total_changed */
  ulint stat_skipped_pages;       /*!< unchanged pages in refused gaps
                                  (gap > merge_gap): never read, seeked
                                  past. Example: 12 (pages 8..19).
                                  avg gap = (filler + skipped) /
                                  (ranges - 1) describes how scattered
                                  the changes are */
  bool log_stats;                 /*!< log grouping and batch stats
                                  for this file */
};

/* The read filter */
struct xb_read_filt_t {
  void (*init)(xb_read_filt_ctxt_t *ctxt, const xb_fil_cur_t *cursor,
               ulint space_id);
  void (*get_next_batch)(xb_fil_cur_t *ctxt, uint64_t *read_batch_start,
                         uint64_t *read_batch_len);
  /* called from xb_fil_cur_close with the owning cursor, so the
  filter can report per-file statistics with full cursor context */
  void (*deinit)(xb_fil_cur_t *cursor);
  void (*update)(xb_read_filt_ctxt_t *ctxt, uint64_t len,
                 const xb_fil_cur_t *cursor);
};

extern xb_read_filt_t rf_pass_through;
extern xb_read_filt_t rf_page_tracking;

#endif
