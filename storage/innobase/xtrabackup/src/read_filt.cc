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
#include "common.h"
#include "dict0dict.h"
#include "fil_cur.h"
#include "xb0xb.h"
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
    xb_read_filt_ctxt_t *ctxt __attribute__((unused)))
/*!<in: read filter context */
{}

/****************************************************************/ /**
 Initialize the changed page bitmap-based read filter.  Assumes that
 the bitmap is already set up in changed_page_bitmap.  */
static void rf_bitmap_init(
    /*===========*/
    xb_read_filt_ctxt_t *ctxt,  /*!<in/out: read filter
                                context */
    const xb_fil_cur_t *cursor, /*!<in: read cursor */
    ulint space_id)             /*!<in: space id  */
{
  common_init(ctxt, cursor);
  ctxt->bitmap_range = xb_page_bitmap_range_init(changed_page_bitmap, space_id);
  ctxt->filter_batch_end = 0;
}

/****************************************************************/ /**
 Get the next batch of pages for the bitmap read filter.  */
static void rf_bitmap_get_next_batch(
    /*=====================*/
    xb_fil_cur_t *cursor,       /*!< in/out: source file cursor */
    uint64_t *read_batch_start, /*!<out: starting read
                                   offset in bytes for the
                                   next batch of pages */
    uint64_t *read_batch_len)   /*!<out: length in
                                   bytes of the next batch
                                   of pages */
{
  xb_read_filt_ctxt_t *ctxt = &cursor->read_filter_ctxt;
  ulint start_page_id = ctxt->offset / ctxt->page_size;

  xb_a(ctxt->offset % ctxt->page_size == 0);

  if (start_page_id == ctxt->filter_batch_end) {
    /* Used up all the previous bitmap range, get some more */
    ulint next_page_id;

    /* Find the next changed page using the bitmap */
    next_page_id = xb_page_bitmap_range_get_next_bit(ctxt->bitmap_range, true);

    if (next_page_id == ULINT_UNDEFINED) {
      *read_batch_len = 0;
      return;
    }

    ctxt->offset = next_page_id * ctxt->page_size;

    /* Find the end of the current changed page block by searching
    for the next cleared bitmap bit */
    ctxt->filter_batch_end =
        xb_page_bitmap_range_get_next_bit(ctxt->bitmap_range, false);
    xb_a(next_page_id < ctxt->filter_batch_end);
  }

  *read_batch_start = ctxt->offset;
  if (ctxt->offset >= ctxt->data_file_size) {
    *read_batch_len = 0;
    return;
  }
  if (ctxt->filter_batch_end == ULINT_UNDEFINED) {
    /* No more cleared bits in the bitmap, need to copy all the
    remaining pages.  */
    *read_batch_len = ctxt->data_file_size - ctxt->offset;
  } else {
    *read_batch_len = ctxt->filter_batch_end * ctxt->page_size - ctxt->offset;
  }

  /* If the page block is larger than the buffer capacity, limit it to
  buffer capacity.  The subsequent invocations will continue returning
  the current block in buffer-sized pieces until ctxt->filter_batch_end
  is reached, trigerring the next bitmap query.  */
  if (*read_batch_len > ctxt->buffer_capacity) {
    *read_batch_len = ctxt->buffer_capacity;
  }

  xb_a(ctxt->offset % ctxt->page_size == 0);
  xb_a(*read_batch_start % ctxt->page_size == 0);
  xb_a(*read_batch_len % ctxt->page_size == 0);
}

/****************************************************************/ /**
 Deinitialize the changed page bitmap-based read filter.  */
static void rf_bitmap_deinit(
    /*=============*/
    xb_read_filt_ctxt_t *ctxt) /*!<in/out: read filter context */
{
  xb_page_bitmap_range_deinit(ctxt->bitmap_range);
}

/** Initialize the page tracking based read filter.  Assumes that
the space_map is already set up in changed_page_bitmap.
@param[in/out] ctxt     read filter context
@param[in]     cursor   read cursor
@param[in]     space_id space id */
static void rf_page_tracking_init(xb_read_filt_ctxt_t *ctxt,
                                  const xb_fil_cur_t *cursor, ulint space_id) {
  common_init(ctxt, cursor);
  ctxt->filter_batch_end = 0;
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
      full_scan_tables[ctxt->space_id] == true) {
    *read_batch_start = ctxt->offset;
    if (ctxt->offset >= ctxt->data_file_size) {
      *read_batch_len = 0;
      return;
    }
    *read_batch_len = ctxt->data_file_size - ctxt->offset;
  } else {
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

      ctxt->offset = next_page_id * ctxt->page_size;
      /* Find the end of the current page tracking block */
      pagetracking::range_get_next_page(space);
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

  ut_ad(ctxt->offset % ctxt->page_size == 0);
  ut_ad(*read_batch_start % ctxt->page_size == 0);
  ut_ad(*read_batch_len % ctxt->page_size == 0);
}

/** Deinitialize the page tracking based read filter.
@param[in] ctxt   read filtr context */
static void rf_page_tracking_deinit(xb_read_filt_ctxt_t *ctxt
                                    __attribute__((unused))) {}

/* The pass-through read filter */
xb_read_filt_t rf_pass_through = {&rf_pass_through_init,
                                  &rf_pass_through_get_next_batch,
                                  &rf_pass_through_deinit, &common_update};

/* The changed page bitmap-based read filter */
xb_read_filt_t rf_bitmap = {&rf_bitmap_init, &rf_bitmap_get_next_batch,
                            &rf_bitmap_deinit, &common_update};

/* The page tracking based read filter */
xb_read_filt_t rf_page_tracking = {&rf_page_tracking_init,
                                   &rf_page_tracking_get_next_batch,
                                   &rf_page_tracking_deinit, &common_update};
