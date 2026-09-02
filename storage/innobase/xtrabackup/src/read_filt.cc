/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009-2021 Percona Inc.
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

/* Data file read filter implementation */

#include "read_filt.h"

#include <algorithm>
#include <iomanip>
#include "common.h"
#include "dict0dict.h"
#include "fil_cur.h"
#include "utils.h"
#include "xb0xb.h"
#include "xb_io_probe.h"
#include "xtrabackup.h"

/****************************************************************/ /**
 Perform read filter context initialization that is common to all read
 filters.  */
static void common_init(
    /*========*/
    xb_read_filt_ctxt_t *ctxt,  /*!<in/out: read filter context */
    const xb_fil_cur_t *cursor) /*!<in: file cursor */
{
  ctxt->offset = 0;
  ctxt->data_file_size = cursor->statinfo.st_size;
  ctxt->buffer_capacity = cursor->buf_size;
  ctxt->page_size = cursor->page_size;
  ctxt->space_id = cursor->space_id;
  ctxt->merge_gap = 0;
  ctxt->stat_batches = 0;
  ctxt->stat_total_changed_pages = 0;
  ctxt->stat_groups = 0;
  ctxt->stat_combined_gaps = 0;
  ctxt->stat_filler_pages = 0;
  ctxt->stat_skipped_pages = 0;
  ctxt->log_stats = false;
}

/****************************************************************/ /**
 Update the filter with the actual batch size asfter it has
 been read. */
static void common_update(
    /*========*/
    xb_read_filt_ctxt_t *ctxt,  /*!<in/out: read filter context */
    uint64_t len,               /*!in: length in bytes of the batch has
                                   been read */
    const xb_fil_cur_t *cursor) /*!<in: file cursor */
{
  ctxt->data_file_size = cursor->statinfo.st_size;
  ctxt->offset += len;
}

/****************************************************************/ /**
 Initialize the pass-through read filter. */
static void rf_pass_through_init(
    /*=================*/
    xb_read_filt_ctxt_t *ctxt,  /*!<in/out: read filter context */
    const xb_fil_cur_t *cursor, /*!<in: file cursor */
    ulint space_id __attribute__((unused)))
/*!<in: space id we are reading */
{
  common_init(ctxt, cursor);
}

/****************************************************************/ /**
 Get the next batch of pages for the pass-through read filter.  */
static void rf_pass_through_get_next_batch(
    /*===========================*/
    xb_fil_cur_t *cursor,       /*!< in/out: source file cursor */
    uint64_t *read_batch_start, /*!<out: starting read
                                   offset in bytes for the
                                   next batch of pages */
    uint64_t *read_batch_len)   /*!<out: length in
                                   bytes of the next batch
                                   of pages */
{
  xb_read_filt_ctxt_t *ctxt = &cursor->read_filter_ctxt;
  *read_batch_start = ctxt->offset;
  if (ctxt->offset >= ctxt->data_file_size) {
    *read_batch_len = 0;
    return;
  }
  *read_batch_len = ctxt->data_file_size - ctxt->offset;

  if (*read_batch_len > ctxt->buffer_capacity) {
    *read_batch_len = ctxt->buffer_capacity;
  }
}

/****************************************************************/ /**
 Deinitialize the pass-through read filter.  */
static void rf_pass_through_deinit(
    /*===================*/
    xb_fil_cur_t *cursor __attribute__((unused)))
/*!<in: file cursor being closed */
{}

/** Initialize the page tracking based read filter.  Assumes that
the space_map is already set up in changed_page_bitmap.
@param[in/out] ctxt     read filter context
@param[in]     cursor   read cursor
@param[in]     space_id space id */
static void rf_page_tracking_init(xb_read_filt_ctxt_t *ctxt,
                                  const xb_fil_cur_t *cursor, ulint space_id) {
  common_init(ctxt, cursor);
  ctxt->filter_batch_end = 0;

  /* Full-scan spaces never consult merge_gap; spaces without changed pages
  are not read at all. */
  if (space_id == dict_sys_t::s_dict_space_id ||
      full_scan_tables.find(space_id) != full_scan_tables.end() ||
      changed_page_tracking == nullptr ||
      changed_page_tracking->count(space_id) == 0) {
    return;
  }

  /* A pinned --page-tracking-merge-gap is expressed in innodb_page_size
  pages; scale it by the physical page size so compressed tablespaces
  (zip size 1K-8K) combine across the same byte limit instead of a
  proportionally smaller one. In auto mode (the default) the limit is
  the read request cost - measured single-threaded at backup start, or
  the conservative fallback - converted to pages of this tablespace's
  physical page size: combine every gap cheaper than one saved read.

  The divisibility below holds for every InnoDB tablespace this filter
  can see: the page-tracking filter is only selected for spaces in the
  server's changed-page map (InnoDB by construction), and every valid
  physical page size - 1K-16K compressed, up to the 64K server page
  size uncompressed - is a power of two not larger than UNIV_PAGE_SIZE.
  Were it ever violated in a release build, the integer arithmetic
  degrades toward merge_gap = 0, i.e. the old strictly-consecutive reads:
  a performance fallback, never a correctness risk (the incremental
  write filter still gates every page by its LSN). */
  ut_ad(UNIV_PAGE_SIZE % ctxt->page_size == 0);
  ctxt->merge_gap =
      static_cast<ulint>(opt_page_tracking_merge_gap_auto
                             ? xb_read_request_cost / ctxt->page_size
                             : uint64_t{opt_page_tracking_merge_gap} *
                                   (UNIV_PAGE_SIZE / ctxt->page_size));

  /* grouping statistics accumulate while the file is read and are
  logged when it closes; below ~16MB of changed pages even fully
  scattered reads cost a fraction of a second, so such tables only
  add noise */
  constexpr ulint LOG_MIN_CHANGED_PAGES = 1000;
  ctxt->stat_total_changed_pages =
      changed_page_tracking->at(space_id).pages.size();
  ctxt->log_stats = (ctxt->stat_total_changed_pages >= LOG_MIN_CHANGED_PAGES);
}

/** Get the next batch of pages for the page tracking based filter.
@param[in/out] cursor            source file cursor
@param[out]    read_batch_start  starting read offset for the next pages batch
@param[out]    read_batch_len    length in bytes of next batch of pages */
static void rf_page_tracking_get_next_batch(xb_fil_cur_t *cursor,
                                            uint64_t *read_batch_start,
                                            uint64_t *read_batch_len) {
  xb_read_filt_ctxt_t *ctxt = &cursor->read_filter_ctxt;

  ulint next_page_id;

#ifdef UNIV_DEBUG
  auto verify_skipped_pages = [&]() {
    /* Ensure skipped pages does not have modified pages between last
    backup lsn and checkpoint LSN */
    while (true) {
      uint64_t to_read_len = next_page_id * ctxt->page_size - ctxt->offset;

      if (to_read_len == 0) break;

      xb_fil_cur_read_from_offset(cursor, ctxt->offset, to_read_len);

      ulint page_size = cursor->page_size;
      byte *page;
      uint i = 0;
      for (i = 0, page = cursor->buf; i < cursor->buf_npages;
           i++, page += page_size) {
        lsn_t page_lsn = mach_read_from_8(page + FIL_PAGE_LSN);
        ut_ad(page_lsn <= incremental_lsn ||
              page_lsn >= incremental_start_checkpoint_lsn);
      }
    }
  };
#endif

  /* we need full scan of mysql tablespaces to identify modified pages from last
   * backup start lsn. This is done because we skip applying logical
   * redos (MLOG_TABLE_DYNAMIC_META) during the incremental prepare (except
   * the last prepare). These logical redos are converted to regular redo and
   * flushed to pages in mysql.ibd when the server process a checkpoint. So
   * we directly take the physical changes made to innodb_dynamic_metadata
   * since the last backup. Hence we copy all changes to mysql.ibd since last
   * backup start_lsn instead of last backup end_lsn.
   * We read all pages by setting read_batch_len to the size of file */

  /* if inplace DDLs that generated MLOG_INDEX_LOAD happened on the table after
  the checkpoint LSN or if tablepace encryption is changed after checkpoint LSN
  we do full scan full_scan_tables is populated during the first scan of redo */

  if (ctxt->space_id == dict_sys_t::s_dict_space_id ||
      full_scan_tables.find(ctxt->space_id) != full_scan_tables.end()) {
    ut_ad(full_scan_tables_count == full_scan_tables.size());
    *read_batch_start = ctxt->offset;
    if (ctxt->offset >= ctxt->data_file_size) {
      *read_batch_len = 0;
      return;
    }
    *read_batch_len = ctxt->data_file_size - ctxt->offset;
  } else {
    ut_ad(full_scan_tables_count == full_scan_tables.size());
    /* if no page changed for given space return */
    if (!changed_page_tracking->count(ctxt->space_id)) {
#ifdef UNIV_DEBUG
      next_page_id = ctxt->data_file_size / ctxt->page_size;
      verify_skipped_pages();
#endif
      *read_batch_len = 0;
      return;
    }

    ut_ad(ctxt->offset % ctxt->page_size == 0);

    ulint start_page_id;
    start_page_id = ctxt->offset / ctxt->page_size;

    /* check if we need to scan new block */
    if (start_page_id == ctxt->filter_batch_end) {
      auto space = &changed_page_tracking->at(ctxt->space_id);

      if (ctxt->offset == 0) {
        space->current_page_it = space->pages.begin();
      } else {
        /* find the page where we end the last block */
        ut_ad(space->current_page_it != space->pages.end());
        space->current_page_it++;
        if (space->current_page_it == space->pages.end()) {
          *read_batch_len = 0;
          return;
        }
      }
      next_page_id = *space->current_page_it;

#ifdef UNIV_DEBUG
      verify_skipped_pages();
#endif

      /* stats: one more read range. The gap between the previous
      range's end and this one (if any) is exactly a gap the walker
      refused (that refusal is what ended the previous range), so its
      pages were seeked past, never read: skipped. Gaps the walker
      combines lie inside a range and are counted as filler by the
      walker itself - each gap lands in exactly one of the two. */
      ctxt->stat_groups++;
      if (ctxt->filter_batch_end != 0) {
        ctxt->stat_skipped_pages += next_page_id - ctxt->filter_batch_end;
      }

      ctxt->offset = next_page_id * ctxt->page_size;
      /* Find the end of the current page tracking block; the walker
      adds the pages it merges across into ctxt's stat counters */
      pagetracking::range_get_next_page(space, ctxt);
      ut_ad(space->current_page_it != space->pages.end());

      ctxt->filter_batch_end = (*space->current_page_it) + 1;
      ut_ad(next_page_id <= ctxt->filter_batch_end);
    }

    *read_batch_start = ctxt->offset;

    if (ctxt->offset >= ctxt->data_file_size) {
      *read_batch_len = 0;
      return;
    }

    *read_batch_len = ctxt->filter_batch_end * ctxt->page_size - ctxt->offset;
  }
  /* If the page block is larger than the buffer capacity, limit it to
  buffer capacity.  The subsequent invocations will continue returning
  the current block in buffer-sized pieces until ctxt->filter_batch_end
  is reached, triggering the next pagetracking query */
  if (*read_batch_len > ctxt->buffer_capacity) {
    *read_batch_len = ctxt->buffer_capacity;
  }

  if (*read_batch_len > 0) {
    ctxt->stat_batches++;
  }

  ut_ad(ctxt->offset % ctxt->page_size == 0);
  ut_ad(*read_batch_start % ctxt->page_size == 0);
  ut_ad(*read_batch_len % ctxt->page_size == 0);
}

/** Deinitialize the page tracking based read filter: log the grouping
statistics accumulated while the file was read. Everything reported
here describes what actually happened - no prediction. */
static void rf_page_tracking_deinit(xb_fil_cur_t *cursor) {
  const xb_read_filt_ctxt_t *ctxt = &cursor->read_filter_ctxt;
  if (!ctxt->log_stats || ctxt->stat_groups == 0) {
    return;
  }

  /* ranges = runs of consecutive changed pages: every combined gap
  joined two of them into one read */
  const ulint ranges = ctxt->stat_groups + ctxt->stat_combined_gaps;
  const ulint boundaries = ranges - 1;
  const double avg_gap = boundaries == 0
                             ? 0.0
                             : static_cast<double>(ctxt->stat_filler_pages +
                                                   ctxt->stat_skipped_pages) /
                                   static_cast<double>(boundaries);
  /* the benefit and its price, as parallel ratios: how many times fewer
  read requests, bought at how many times the changed read volume */
  const double reduction =
      static_cast<double>(ranges) / static_cast<double>(ctxt->stat_groups);
  const double amplification =
      static_cast<double>(ctxt->stat_total_changed_pages +
                          ctxt->stat_filler_pages) /
      static_cast<double>(ctxt->stat_total_changed_pages);
  xb::info() << std::fixed << "pagetracking: " << cursor->rel_path << ": "
             << ctxt->stat_total_changed_pages << " changed pages in " << ranges
             << " ranges (avg gap " << std::setprecision(1) << avg_gap
             << " pages); merge-gap=" << ctxt->merge_gap
             << (opt_page_tracking_merge_gap_auto ? " (auto)" : "")
             << " combined them into " << ctxt->stat_groups
             << " reads: request reduction " << reduction
             << "x, read amplification " << std::setprecision(2)
             << amplification << "x; issued " << ctxt->stat_batches
             << " read batches";

  /* Make an ineffective auto decision self-explanatory: when the
  typical gap costs more than one read request, combining achieves
  little and the reads stay individual. Only when the gap is within
  reach of a plausible cost: past the clamp ceiling no calibration
  could ever combine across it - that is genuinely sparse data. */
  const uint64_t avg_gap_bytes =
      static_cast<uint64_t>(avg_gap * static_cast<double>(ctxt->page_size));
  if (opt_page_tracking_merge_gap_auto && reduction < 1.5 &&
      avg_gap_bytes > xb_read_request_cost &&
      avg_gap_bytes <= pagetracking::READ_REQUEST_COST_MAX_BYTES) {
    xb::info() << std::fixed << std::setprecision(1)
               << "pagetracking: " << cursor->rel_path << ": typical gap "
               << avg_gap << " pages ("
               << xtrabackup::utils::human_readable(avg_gap_bytes)
               << ") costs more than one read request ("
               << xtrabackup::utils::human_readable(xb_read_request_cost)
               << "); reads stay individual - if sequential read "
                  "throughput is high, --page-tracking-merge-gap="
               << static_cast<uint64_t>(avg_gap + 1.0) << " may be faster";
  }
}

/* The pass-through read filter */
xb_read_filt_t rf_pass_through = {&rf_pass_through_init,
                                  &rf_pass_through_get_next_batch,
                                  &rf_pass_through_deinit, &common_update};

/* The page tracking based read filter */
xb_read_filt_t rf_page_tracking = {&rf_page_tracking_init,
                                   &rf_page_tracking_get_next_batch,
                                   &rf_page_tracking_deinit, &common_update};
