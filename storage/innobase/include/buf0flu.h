/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/buf0flu.h
 The database buffer pool flush algorithm

 Created 11/5/1995 Heikki Tuuri
 *******************************************************/

#ifndef buf0flu_h
#define buf0flu_h

#include "buf0types.h"
#include "log0types.h"
#include "univ.i"
#include "ut0byte.h"
#include "ut0cpu_cache.h"
#include "ut0link_buf.h"

#include <memory>
#ifndef UNIV_HOTBACKUP
/** Checks if the page_cleaner is in active state. */
bool buf_flush_page_cleaner_is_active();

#ifdef UNIV_DEBUG

/** Value of MySQL global variable used to disable page cleaner. */
extern bool innodb_page_cleaner_disabled_debug;

#endif /* UNIV_DEBUG */

/** Event to synchronise with the flushing. */
extern os_event_t buf_flush_event;

/** Event to wait for one flushing step */
extern os_event_t buf_flush_tick_event;

class Alter_stage;

/** Remove a block from the flush list of modified blocks.
@param[in]      bpage   pointer to the block in question */
void buf_flush_remove(buf_page_t *bpage);

/** Relocates a buffer control block on the flush_list.
 Note that it is assumed that the contents of bpage has already been
 copied to dpage. */
void buf_flush_relocate_on_flush_list(
    buf_page_t *bpage,  /*!< in/out: control block being moved */
    buf_page_t *dpage); /*!< in/out: destination block */

/** Updates the flush system data structures when a write is completed.
@param[in]      bpage   pointer to the block in question */
void buf_flush_write_complete(buf_page_t *bpage);

#endif /* !UNIV_HOTBACKUP */

/** Check if page type is uncompressed.
@param[in]      page    page frame
@return true if uncompressed page type. */
bool page_is_uncompressed_type(const byte *page);

/** Initialize a page for writing to the tablespace.
@param[in]      block           buffer block; NULL if bypassing the buffer pool
@param[in,out]  page            page frame
@param[in,out]  page_zip_       compressed page, or NULL if uncompressed
@param[in]      newest_lsn      newest modification LSN to the page
@param[in]      skip_checksum   whether to disable the page checksum
@param[in]      skip_lsn_check  true to skip check for LSN (in DEBUG) */
void buf_flush_init_for_writing(const buf_block_t *block, byte *page,
                                void *page_zip_, lsn_t newest_lsn,
                                bool skip_checksum, bool skip_lsn_check);

#ifndef UNIV_HOTBACKUP
#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: block and LRU list mutexes must be held upon entering this function, and
they will be released by this function after flushing. This is loosely based on
buf_flush_batch() and buf_flush_page().
@param[in,out]  buf_pool        buffer pool instance
@param[in,out]  block           buffer control block
@return true if the page was flushed and the mutex released */
[[nodiscard]] bool buf_flush_page_try(buf_pool_t *buf_pool, buf_block_t *block);
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** Do flushing batch of a given type.
NOTE: The calling thread is not allowed to own any latches on pages!
@param[in,out]  buf_pool        buffer pool instance
@param[in]      type            flush type
@param[in]      min_n           wished minimum number of blocks flushed
(it is not guaranteed that the actual number is that big, though)
@param[in]      lsn_limit       in the case BUF_FLUSH_LIST all blocks whose
oldest_modification is smaller than this should be flushed (if their number
does not exceed min_n), otherwise ignored
@param[out]     n_processed     the number of pages which were processed is
passed back to caller. Ignored if NULL
@retval true    if a batch was queued successfully.
@retval false   if another batch of same type was already running. */
bool buf_flush_do_batch(buf_pool_t *buf_pool, buf_flush_t type, ulint min_n,
                        lsn_t lsn_limit, ulint *n_processed);

/** This utility flushes dirty blocks from the end of the flush list of all
buffer pool instances.
NOTE: The calling thread is not allowed to own any latches on pages!
@param[in]      min_n           wished minimum number of blocks flushed (it is
not guaranteed that the actual number is that big, though)
@param[in]      lsn_limit       in the case BUF_FLUSH_LIST all blocks whose
oldest_modification is smaller than this should be flushed (if their number
does not exceed min_n), otherwise ignored
@param[out]     n_processed     the number of pages which were processed is
passed back to caller. Ignored if NULL.
@return true if a batch was queued successfully for each buffer pool
instance. false if another batch of same type was already running in
at least one of the buffer pool instance */
bool buf_flush_lists(ulint min_n, lsn_t lsn_limit, ulint *n_processed);

/** This function picks up a single page from the tail of the LRU
list, flushes it (if it is dirty), removes it from page_hash and LRU
list and puts it on the free list. It is called from user threads when
they are unable to find a replaceable page at the tail of the LRU
list i.e.: when the background LRU flushing in the page_cleaner thread
is not fast enough to keep pace with the workload.
@param[in,out]  buf_pool        buffer pool instance
@return true if success. */
bool buf_flush_single_page_from_LRU(buf_pool_t *buf_pool);

/** Waits until there's no flush of the given type from given BP instance.
Note that in case of BUF_FLUSH_LIST and BUF_FLUSH_LRU we also make sure there's
no ongoing batch initialization (which could lead to flushes).
The BUF_FLUSH_SINGLE_PAGE does not have batch initialization.
Note, that we return as soon as there is no flush, but in general a new one
could start right after we've returned (it's up to the caller to prevent this).
If buf_pool is nullptr, then it will await a moment with no flushes for each
BP instance in turn, which in general doesn't imply there was a single moment
when all instances were quiescent - it's up to the caller to ensure that.

@param[in] buf_pool
              The specific buffer pool instance to check.
              Can be null, if we want to wait for each buf_pool in turn.
@param[in] flush_type           Flush type. */
void buf_flush_await_no_flushing(buf_pool_t *buf_pool, buf_flush_t flush_type);

/** This function should be called at a mini-transaction commit, if a page was
modified in it. Puts the block to the list of modified blocks, if it not
already in it.
@param[in]      block           block which is modified
@param[in]      start_lsn       start lsn of the first mtr in a set of mtr's
@param[in]      end_lsn         end lsn of the last mtr in the set of mtr's
@param[in]      observer        flush observer */
static inline void buf_flush_note_modification(buf_block_t *block,
                                               lsn_t start_lsn, lsn_t end_lsn,
                                               Flush_observer *observer);

/** This function should be called when recovery has modified a buffer page.
@param[in]      block           block which is modified
@param[in]      start_lsn       start lsn of the first mtr in a set of mtr's
@param[in]      end_lsn         end lsn of the last mtr in the set of mtr's */
static inline void buf_flush_recv_note_modification(buf_block_t *block,
                                                    lsn_t start_lsn,
                                                    lsn_t end_lsn);

/** Returns true if the file page block is immediately suitable for replacement,
i.e., the transition FILE_PAGE => NOT_USED allowed. The caller must hold the
LRU list and block mutexes.
@param[in]      bpage   buffer control block, must be buf_page_in_file() and
                        in the LRU list
@return true if can replace immediately */
bool buf_flush_ready_for_replace(const buf_page_t *bpage);

#ifdef UNIV_DEBUG
struct SYS_VAR;

/** Disables page cleaner threads (coordinator and workers).
It's used by: SET GLOBAL innodb_page_cleaner_disabled_debug = 1 (0).
@param[in]      thd             thread handle
@param[in]      var             pointer to system variable
@param[out]     var_ptr         where the formal string goes
@param[in]      save            immediate result from check function */
void buf_flush_page_cleaner_disabled_debug_update(THD *thd, SYS_VAR *var,
                                                  void *var_ptr,
                                                  const void *save);
#endif /* UNIV_DEBUG */

/** Initialize page_cleaner.  */
void buf_flush_page_cleaner_init();

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Validates the flush list.
 @return true if ok */
bool buf_flush_validate(buf_pool_t *buf_pool);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/** Initialize the red-black tree to speed up insertions into the flush_list
 during recovery process. Should be called at the start of recovery
 process before any page has been read/written. */
void buf_flush_init_flush_rbt(void);

/** Frees up the red-black tree. */
void buf_flush_free_flush_rbt(void);

/** Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: 1. in simulated aio we must call os_aio_simulated_wake_handler_threads
after we have posted a batch of writes! 2. buf_page_get_mutex(bpage) must be
held upon entering this function. The LRU list mutex must be held if flush_type
== BUF_FLUSH_SINGLE_PAGE. Both mutexes will be released by this function if it
returns true.
@param[in]      buf_pool        buffer pool instance
@param[in]      bpage           buffer control block
@param[in]      flush_type      type of flush
@param[in]      sync            true if sync IO request
@return true if page was flushed */
bool buf_flush_page(buf_pool_t *buf_pool, buf_page_t *bpage,
                    buf_flush_t flush_type, bool sync);

/** Check if the block is modified and ready for flushing.
Requires buf_page_get_mutex(bpage).
@param[in]      bpage           buffer control block, must be buf_page_in_file()
@param[in]      flush_type      type of flush
@return true if can flush immediately */
[[nodiscard]] bool buf_flush_ready_for_flush(buf_page_t *bpage,
                                             buf_flush_t flush_type);

/** Check if there are any dirty pages that belong to a space id in the flush
 list in a particular buffer pool.
 @return number of dirty pages present in a single buffer pool */
ulint buf_pool_get_dirty_pages_count(
    buf_pool_t *buf_pool,      /*!< in: buffer pool */
    space_id_t id,             /*!< in: space id to check */
    Flush_observer *observer); /*!< in: flush observer to check */

/** Executes fsync for all tablespaces, to fsync all pages written to disk. */
void buf_flush_fsync();

/** Synchronously flush dirty blocks from the end of the flush list of all
 buffer pool instances. NOTE: The calling thread is not allowed to own any
 latches on pages! */
void buf_flush_sync_all_buf_pools();

/** Checks if all flush lists are empty. It is supposed to be used in
single thread, during startup or shutdown. Hence it does not acquire
lock and it is caller's responsibility to guarantee that flush lists
are not changed in background.
@return true if all flush lists were empty. */
bool buf_are_flush_lists_empty_validate();

/** We use Flush_observer to track flushing of non-redo logged pages in bulk
create index(btr0load.cc).Since we disable redo logging during a index build,
we need to make sure that all dirty pages modified by the index build are
flushed to disk before any redo logged operations go to the index. */

class Flush_observer {
 public:
  /** Constructor
  @param[in] space_id   table space id
  @param[in] trx                trx instance
  @param[in,out] stage PFS progress monitoring instance, it's used by
  ALTER TABLE. It is passed to log_preflush_pool_modified_pages() for
  accounting. */
  Flush_observer(space_id_t space_id, trx_t *trx, Alter_stage *stage) noexcept;

  /** Destructor */
  ~Flush_observer() noexcept;

  /** Print information about the current object.
  @param[in,out]  out  output stream to be used.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const;

  /** Check pages have been flushed and removed from the flush list
  in a buffer pool instance.
  @param[in]    instance_no     buffer pool instance no
  @return true if the pages were removed from the flush list */
  bool is_complete(size_t instance_no) {
    ut_ad(m_flushed[instance_no].load() >= 0);
    ut_ad(m_removed[instance_no].load() >= 0);
    return m_interrupted ||
           (m_flushed[instance_no].load() == m_removed[instance_no].load());
  }

  /** Interrupt observer not to wait. */
  void interrupted() { m_interrupted = true; }

  /** Check whether trx is interrupted
  @return true if trx is interrupted */
  bool check_interrupted();

  /** Flush dirty pages. */
  void flush();

  /** Notify observer of flushing a page
  @param[in]    buf_pool        buffer pool instance
  @param[in]    bpage           buffer page to flush */
  void notify_flush(buf_pool_t *buf_pool, buf_page_t *bpage);

  /** Notify observer of removing a page from flush list
  @param[in]    buf_pool        buffer pool instance
  @param[in]    bpage           buffer page flushed */
  void notify_remove(buf_pool_t *buf_pool, buf_page_t *bpage);

 private:
  using Counter = std::atomic_int;
  using Counters = std::vector<Counter, ut::allocator<Counter>>;

#ifdef UNIV_DEBUG
  [[nodiscard]] bool validate() const noexcept;
#endif /* UNIV_DEBUG */

  /** Tablespace ID. */
  space_id_t m_space_id{};

  /** Trx instance */
  trx_t *m_trx{};

  /** Performance schema accounting object, used by ALTER TABLE.
  If not nullptr, then stage->begin_phase_flush() will be called initially,
  specifying the number of pages to be attempted to be flushed and
  subsequently, stage->inc() will be called for each page we attempt to
  flush. */
  Alter_stage *m_stage{};

  /** Flush request sent, per buffer pool. */
  Counters m_flushed{};

  /** Flush request finished, per buffer pool. */
  Counters m_removed{};

  /** Number of pages using this instance. */
  Counter m_n_ref_count{};

  /** True if the operation was interrupted. */
  bool m_interrupted{};
};

lsn_t get_flush_sync_lsn() noexcept;

// Forward declaration.
class Buf_flush_list_added_lsns;

// Simplified type alias for the aligned unique_ptr
using Buf_flush_list_added_lsns_aligned_ptr =
    std::unique_ptr<Buf_flush_list_added_lsns,
                    ut::detail::Aligned_deleter<Buf_flush_list_added_lsns>>;

/** Tracks the concurrent executions of adding dirty pages to the flush lists.
It allows to relax order in which dirty pages have to be added to the flush
lists and helps in advancing the checkpoint LSN.
Note : Earlier it was known as recent_closed buffer. */
class Buf_flush_list_added_lsns {
 public:
  /** Factory method that creates a dynamically allocated instance that is
  aligned to the cache line boundary. This is needed since C++17 doesn't
  guarantee allocating aligned types dynamically */
  static Buf_flush_list_added_lsns_aligned_ptr create();

  /** Validates using assertions that the specified LSN range is not yet added
  to the flush lists.
  @param[in]    begin   start LSN of the range
  @param[in]    end     end LSN of the range*/
  void validate_not_added(lsn_t begin, lsn_t end);

  /** Assume that the start_lsn is the start lsn of the first mini-transaction,
  for which report_added(mtr.start_lsn, mtr.end_lsn) was not yet called.
  Before the call smallest_not_added_lsn() can't be larger than start_lsn.
  During the call smallest_not_added_lsn() becomes start_lsn.
  @param[in]   start_lsn   LSNs that are already flushed.*/
  void assume_added_up_to(lsn_t start_lsn);

  /** Used to report that we know that all changes in a specified range of lsns
  were already applied to pages in BP and all these pages were already added to
  corresponding flush lists. The committing mtr should use this function to
  report that it has added all the pages that it has dirtied to corresponding
  flush lists already.
  The newest_modification should be the mtr->commit_lsn().
  The oldest_modification should be the start_lsn assigned to the mtr
  (which equals commit_lsn() of the previous mtr).

  Information from these reports is used by smallest_not_added_lsn() to
  establish the smallest not yet reported lsn, which in turn can be used by page
  cleaners, or other mtrs which are currently waiting for their turn in
  wait_to_add().
  @param[in]    oldest_modification    start lsn of the range
  @param[in]    newest_modification    end lsn fo the range */
  void report_added(lsn_t oldest_modification, lsn_t newest_modification);

  /** The flush lists are not completely sorted, and the amount of disorder is
  limited by the order_lag() - which is controlled by immutable
  srv_buf_flush_list_added_size  sysvar - in the following way. If page A is
  added before page B in a flush_list, then it must hold that
  A.oldest_modification returned value < B.oldest_modification.
  @return the maximum lsn distance the subsequent elements of the flush list can
  lag behind the first element w.r.t. oldest_modification */
  uint64_t order_lag();

  /** Return lsn up to which we know that all dirty pages with smaller
  oldest_modification were added to the flush list.
  @return The smallest lsn for which there was no call to assume_added_up_to(y)
  with lsn<y nor to report_added(x,y) with x<=lsn<y yet */
  lsn_t smallest_not_added_lsn();

  /** Wait until it is safe to add dirty pages with a given oldest_modification
  lsn to flush list without exceeding the disorder limit imposed by older_lag().
  This function will block and wait until all the pages having
  page.oldest_modification smaller than oldest_modification - older_lag() are
  reported to be added to flush list by other threads.
  @param[in]  oldest_modification  Oldest LSN that does not violate the
              order_lag() */
  void wait_to_add(lsn_t oldest_modification);

 private:
  /** Ring buffer that keeps track of the ranges of LSNs added to
  flush_list(s) already.*/
  alignas(ut::INNODB_CACHE_LINE_SIZE) Link_buf<lsn_t> m_buf_added_lsns;

  /** Constructor. Clients must use factory method to create an instance */
  Buf_flush_list_added_lsns();

  template <typename T, typename... Args>
  friend T *ut::aligned_new_withkey(ut::PSI_memory_key_t key,
                                    std::size_t alignment, Args &&...args);
};

extern Buf_flush_list_added_lsns_aligned_ptr buf_flush_list_added;
#endif /* !UNIV_HOTBACKUP */

#include "buf0flu.ic"

#endif
