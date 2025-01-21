/******************************************************
XtraBackup: hot backup tool for InnoDB
(c) 2009, 2023 Percona LLC and/or its affiliates.
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

#include <fil0fil.h>
#include <fsp0sysspace.h>
#include <srv0start.h>
#include <trx0sys.h>
#include <univ.i>

#include "common.h"
#include "fil_cur.h"
#include "read_filt.h"
#include "xb0xb.h"
#include "xb_dict.h"
#include "xtrabackup.h"

/***********************************************************************
Reads the space flags from a given data file and returns the
page size and whether the file is compressable. */
static bool xb_get_zip_size(const char *file_name, pfs_os_file_t file,
                            byte *buf, page_size_t &page_size,
                            bool &is_encrypted) {
  IORequest read_request(IORequest::READ | IORequest::NO_COMPRESSION);
  const auto ret =
      os_file_read(read_request, file_name, file, buf, 0, UNIV_PAGE_SIZE_MIN);
  if (!ret) {
    xb::warn() << "Failed to read file from server directory " << file_name;
    return (false);
  }

  space_id_t space = mach_read_from_4(buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
  const auto flags = fsp_header_get_flags(buf);

  if (space == 0) {
    page_size.copy_from(univ_page_size);
  } else {
    page_size.copy_from(page_size_t(flags));
  }

  is_encrypted = FSP_FLAGS_GET_ENCRYPTION(flags) ? true : false;

  return (true);
}

/***********************************************************************
Extracts the relative path ("database/table.ibd") of a tablespace from a
specified possibly absolute path.

For user tablespaces both "./database/table.ibd" and
"/remote/dir/database/table.ibd" result in "database/table.ibd".

For system tablepsaces (i.e. When is_system is true) both "/remote/dir/ibdata1"
and "./ibdata1" yield "ibdata1" in the output. */
const char *xb_get_relative_path(
    /*=================*/
    fil_space_t *space, /*!< in: tablespace */
    const char *path)   /*!< in: tablespace path (either
                        relative or absolute) */
{
  const char *next;
  const char *cur;
  const char *prev;

  bool is_shared =
      space != nullptr ? FSP_FLAGS_GET_SHARED(space->flags) : false;
  bool is_system =
      space != nullptr ? fsp_is_system_or_temp_tablespace(space->id) : false;
  bool is_undo = space != nullptr ? fsp_is_undo_tablespace(space->id) : false;

  prev = NULL;
  cur = path;

  while ((next = strchr(cur, OS_PATH_SEPARATOR)) != NULL) {
    prev = cur;
    cur = next + 1;
  }

  if (is_system) {
    return (cur);
  } else {
    return ((prev == NULL || is_shared || is_undo) ? cur : prev);
  }
}

/************************************************************************
Open a source file cursor and initialize the associated read filter.

@return XB_FIL_CUR_SUCCESS on success, XB_FIL_CUR_SKIP if the source file must
be skipped and XB_FIL_CUR_ERROR on error. */
xb_fil_cur_result_t xb_fil_cur_open(
    /*============*/
    xb_fil_cur_t *cursor,        /*!< out: source file cursor */
    xb_read_filt_t *read_filter, /*!< in/out: the read filter */
    fil_node_t *node,            /*!< in: source tablespace node */
    uint thread_n)               /*!< thread number for diagnostics */
{
  page_size_t page_size(0, 0, false);
  ulint page_size_shift;

  /* Initialize these first so xb_fil_cur_close() handles them correctly
  in case of error */
  cursor->orig_buf = NULL;
  cursor->node = NULL;

  cursor->space_id = node->space->id;
  cursor->space_flags = node->space->flags;
  cursor->is_system = fsp_is_system_or_temp_tablespace(node->space->id);
  cursor->is_ibd = fsp_is_ibd_tablespace(node->space->id);

  strncpy(cursor->abs_path, node->name, sizeof(cursor->abs_path) - 1);

  /* Get the relative path for the destination tablespace name, i.e. the
  one that can be appended to the backup root directory. Non-system
  tablespaces may have absolute paths for remote tablespaces in MySQL
  5.6+. We want to make "local" copies for the backup. */
  strncpy(cursor->rel_path, xb_get_relative_path(node->space, cursor->abs_path),
          sizeof(cursor->rel_path) - 1);

  /* In the backup mode we should already have a tablespace handle created
  by fil_load_single_table_tablespace() unless it is a system
  tablespace or srv_close_files is true. Otherwise we open the file here.
  srv_close_files has an effect only on IBD tablespaces. */
  if (cursor->is_system || !srv_backup_mode ||
      (srv_close_files && cursor->is_ibd)) {
    if (!fil_node_open_file(node)) {
      /* The following call prints an error message */
      os_file_get_last_error(true);

      xb::error() << "cannot open tablespace " << cursor->abs_path;

      return (XB_FIL_CUR_ERROR);
    }
  }

  ut_ad(node->is_open);

  cursor->node = node;
  cursor->file = node->handle;

  if (my_fstat(cursor->file.m_file, &cursor->statinfo)) {
    xb::error() << "cannot stat " << cursor->abs_path;

    xb_fil_cur_close(cursor);

    return (XB_FIL_CUR_ERROR);
  }

  if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT ||
      srv_unix_file_flush_method == SRV_UNIX_O_DIRECT_NO_FSYNC) {
    os_file_set_nocache(cursor->file.m_file, node->name, "OPEN");
  }

  posix_fadvise(cursor->file.m_file, 0, 0, POSIX_FADV_SEQUENTIAL);

  /* Allocate read buffer */
  ut_a(opt_read_buffer_size >= UNIV_PAGE_SIZE);
  cursor->buf_size = opt_read_buffer_size;
  cursor->orig_buf = static_cast<byte *>(ut::malloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, cursor->buf_size + UNIV_PAGE_SIZE));
  cursor->buf = static_cast<byte *>(ut_align(cursor->orig_buf, UNIV_PAGE_SIZE));

  /* Determine the page size */
  if (!xb_get_zip_size(cursor->rel_path, cursor->file, cursor->buf, page_size,
                       cursor->is_encrypted)) {
    xb_fil_cur_close(cursor);
    return (XB_FIL_CUR_SKIP);
  } else if (page_size.is_compressed()) {
    page_size_shift = get_bit_shift(page_size.physical());
    xb::info() << node->name
               << " is compressed with page size = " << page_size.physical()
               << " bytes";
    if (page_size_shift < 10 || page_size_shift > 14) {
      xb::error() << "Invalid page size: " << page_size.physical();
      ut_error;
    }
  } else {
    page_size_shift = UNIV_PAGE_SIZE_SHIFT;
  }
  cursor->page_size = page_size.physical();
  cursor->page_size_shift = page_size_shift;
  cursor->zip_size = page_size.is_compressed() ? page_size.physical() : 0;

  cursor->buf_read = 0;
  cursor->buf_npages = 0;
  cursor->buf_offset = 0;
  cursor->buf_page_no = 0;
  cursor->thread_n = thread_n;

  cursor->space_size = cursor->statinfo.st_size / page_size.physical();
  cursor->block_size = node->block_size;

  cursor->read_filter = read_filter;
  cursor->read_filter->init(&cursor->read_filter_ctxt, cursor, node->space->id);

  cursor->scratch = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, cursor->page_size * 2));
  cursor->decrypt = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, cursor->page_size));

  memcpy(cursor->encryption_key, node->space->m_encryption_metadata.m_key,
         sizeof(cursor->encryption_key));
  memcpy(cursor->encryption_iv, node->space->m_encryption_metadata.m_iv,
         sizeof(cursor->encryption_iv));
  cursor->encryption_klen = node->space->m_encryption_metadata.m_key_len;
  cursor->block_size = node->block_size;

  return (XB_FIL_CUR_SUCCESS);
}

static bool is_page_corrupted(bool check_lsn, const byte *read_buf,
                              const page_size_t &page_size,
                              bool skip_checksum) {
  BlockReporter reporter =
      BlockReporter(check_lsn, read_buf, page_size, skip_checksum);
  return reporter.is_corrupted();
}

/** Reads and verifies the next block of pages from the source
file. Positions the cursor after the last read non-corrupted page.
@param[in/out]	cursor	 	source file cursor
@param[in]	offset	 	offset of file to read from
@param[in]	to_read		bytest to read
@return XB_FIL_CUR_SUCCESS if some have been read successfully, XB_FIL_CUR_EOF
if there are no more pages to read and XB_FIL_CUR_ERROR on error */
xb_fil_cur_result_t xb_fil_cur_read_from_offset(xb_fil_cur_t *cursor,
                                                uint64_t offset,
                                                uint64_t to_read) {
  dberr_t err;
  byte *page, *page_to_check;
  ulint i;
  ulint npages;
  ulint retry_count;
  xb_fil_cur_result_t ret;
  ulong n_read;
  page_size_t page_size(
      cursor->zip_size != 0 ? cursor->zip_size : cursor->page_size,
      cursor->page_size, cursor->zip_size != 0);
  IORequest read_request(IORequest::READ | IORequest::NO_COMPRESSION);

  if (to_read == 0LL) {
    return (XB_FIL_CUR_EOF);
  }

  if (to_read > (uint64_t)cursor->buf_size) {
    to_read = (uint64_t)cursor->buf_size;
  }

  xb_a(to_read > 0 && to_read <= 0xFFFFFFFFLL);

  if (to_read % cursor->page_size != 0 &&
      offset + to_read == (uint64_t)cursor->statinfo.st_size) {
    if (to_read < (uint64_t)cursor->page_size) {
      xb::warn() << "junk at the end of " << cursor->abs_path;
      xb::warn() << "offset = " << offset << ", to_read = " << to_read;
      return (XB_FIL_CUR_EOF);
    }

    to_read = (uint64_t)(((ulint)to_read) & ~(cursor->page_size - 1));
  }

  xb_a(to_read % cursor->page_size == 0);

  retry_count = 10;
  ret = XB_FIL_CUR_SUCCESS;

read_retry:
  xtrabackup_io_throttling();

  cursor->buf_read = 0;
  cursor->buf_npages = 0;
  cursor->buf_offset = offset;
  cursor->buf_page_no = (ulint)(offset >> cursor->page_size_shift);

  err = os_file_read_no_error_handling(read_request, cursor->rel_path,
                                       cursor->file, cursor->buf, offset,
                                       to_read, &n_read);
  if (err != DB_SUCCESS) {
    if (err == DB_IO_ERROR) {
      /* If the file is truncated by MySQL, os_file_read will
      fail with DB_IO_ERROR, but XtraBackup must treat this
      error as non critical. */
      if (my_fstat(cursor->file.m_file, &cursor->statinfo)) {
        xb::error() << "cannot stat " << cursor->abs_path;
        return (XB_FIL_CUR_ERROR);
      }
      /* Check if we reached EOF */
      if ((ulonglong)cursor->statinfo.st_size > offset + n_read) {
        return (XB_FIL_CUR_ERROR);
      }
    }
    return (XB_FIL_CUR_ERROR);
  }
  Encryption_metadata encryption_metadata;

  Encryption::set_or_generate(Encryption::AES, cursor->encryption_key,
                              cursor->encryption_iv, encryption_metadata);

  read_request.get_encryption_info().set(encryption_metadata);

  read_request.block_size(cursor->block_size);

  npages = n_read >> cursor->page_size_shift;

  /* check pages for corruption and re-read if necessary. i.e. in case of
  partially written pages */
  for (page = cursor->buf, i = 0; i < npages; page += cursor->page_size, i++) {
    page_to_check = page;
    if (Encryption::is_encrypted_page(page)) {
      Encryption encryption(read_request.encryption_algorithm());

      page_to_check = cursor->decrypt;
      memcpy(cursor->decrypt, page, cursor->page_size);

      const auto ret =
          encryption.decrypt(read_request, cursor->decrypt, cursor->page_size,
                             cursor->scratch, cursor->page_size);
      if (ret != DB_SUCCESS) {
        goto corruption;
      }

      if (Compression::is_compressed_page(cursor->decrypt)) {
        if (os_file_decompress_page(false, cursor->decrypt, cursor->scratch,
                                    cursor->page_size) != DB_SUCCESS) {
          goto corruption;
        }
      }
    }

    if (Compression::is_compressed_page(page)) {
      page_to_check = cursor->decrypt;
      memcpy(cursor->decrypt, page, cursor->page_size);
      if (os_file_decompress_page(false, cursor->decrypt, cursor->scratch,
                                  cursor->page_size) != DB_SUCCESS) {
        goto corruption;
      }
    }

    if (is_page_corrupted(true, page_to_check, page_size, false)) {
    corruption:

      ulint page_no = cursor->buf_page_no + i;

      if (cursor->is_system && page_no >= FSP_EXTENT_SIZE &&
          page_no < FSP_EXTENT_SIZE * 3) {
        /* skip doublewrite buffer pages */
        xb_a(cursor->page_size == UNIV_PAGE_SIZE);
        xb::info() << "Page " << page_no
                   << " is a doublewrite buffer page, skipping.";
      } else {
        retry_count--;
        if (retry_count == 0) {
          xb::error() << "failed to read page after 10 retries. File "
                      << cursor->abs_path << " seems to be corrupted.";
          ret = XB_FIL_CUR_CORRUPTED;
          break;
        }
        xb::info() << "Database page corruption detected at page " << page_no
                   << ", retrying...";

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        goto read_retry;
      }
    }
    cursor->buf_read += cursor->page_size;
    cursor->buf_npages++;
  }

  cursor->read_filter->update(&cursor->read_filter_ctxt, n_read, cursor);

  posix_fadvise(cursor->file.m_file, offset, to_read, POSIX_FADV_DONTNEED);

  return (ret);
}

/** Reads and verifies the next block of pages from the source
file. Positions the cursor after the last read non-corrupted page.
@param[in/out] cursor	source file cursor
@return XB_FIL_CUR_SUCCESS if some have been read successfully, XB_FIL_CUR_EOF
if there are no more pages to read and XB_FIL_CUR_ERROR on error. */
xb_fil_cur_result_t xb_fil_cur_read(xb_fil_cur_t *cursor) {
  uint64_t offset;
  uint64_t to_read;
  cursor->read_filter->get_next_batch(cursor, &offset, &to_read);
  return xb_fil_cur_read_from_offset(cursor, offset, to_read);
}

/************************************************************************
Close the source file cursor opened with xb_fil_cur_open() and its
associated read filter. */
void xb_fil_cur_close(
    /*=============*/
    xb_fil_cur_t *cursor) /*!< in/out: source file cursor */
{
  cursor->read_filter->deinit(&cursor->read_filter_ctxt);

  ut::free(cursor->scratch);
  ut::free(cursor->decrypt);
  ut::free(cursor->orig_buf);
  if (cursor->node != NULL) {
    fil_node_close_file(cursor->node);
    cursor->file = XB_FILE_UNDEFINED;
  }
}
