/******************************************************
Copyright (c) 2021-2023 Percona LLC and/or its affiliates.

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

#ifndef FILE_UTILS_H
#define FILE_UTILS_H
#include <my_dir.h>
#include <my_io.h>
#include <cstring>
#include <string>
#include "datasink.h"

typedef unsigned char byte;
// using page_type_t = size_t;

/** In tablespaces created by MySQL/InnoDB 5.1.7 or later, the contents of this
field is valid for all uncompressed pages. */
constexpr uint32_t XB_FIL_PAGE_TYPE = 24;

/** start of the data on the page */
constexpr uint32_t XB_FIL_PAGE_DATA = 38;

/** Offset of the space header within a file page */
constexpr uint32_t XB_FSP_HEADER_OFFSET = XB_FIL_PAGE_DATA;

/** fsp_space_t.flags, similar to dict_table_t::flags */
constexpr uint32_t XB_FSP_SPACE_FLAGS = 16;

constexpr uint32_t XB_FSP_FLAGS_POS_PAGE_SSIZE = 5 + 1;

/** Bit mask of the PAGE_SSIZE field */
constexpr uint32_t XB_FSP_FLAGS_MASK_PAGE_SSIZE = (~(~0U << 4U)) << 6;

/** Type of the index page */
typedef byte page_t;

/** Compressed page */
constexpr size_t XB_FIL_PAGE_COMPRESSED = 14;

/** Compressed and Encrypted page */
constexpr size_t XB_FIL_PAGE_COMPRESSED_AND_ENCRYPTED = 16;

/** Size after compression (u16) */
constexpr uint32_t XB_FIL_PAGE_COMPRESS_SIZE_V1 = 32U;

/** Original 16k page size for InnoDB tablespaces. */
constexpr uint32_t XB_UNIV_PAGE_SIZE_ORIG = 1 << 14U;

/** Smallest compressed page size */
constexpr uint32_t XB_UNIV_ZIP_SIZE_MIN = 1 << 10U;

/** Maximum page size InnoDB currently supports. */
constexpr size_t XB_UNIV_PAGE_SIZE_MAX = 1 << 16U;

/** Return a safer suffix of file_name, or "." if it has no safer
suffix. Check for fully specified file names and other atrocities.
Warn the user if we do not return file_name.
@param[in]  file_name            file_name to check
@param[out] prefix_len_out       the length of the unsafe prefix
@return ptr to a safer suffix of file_nam. */
const char *safer_name_suffix(char const *file_name, int *prefix_len_out);

bool file_exists(const char *filename);

bool is_path_separator(char c);

int mkdirp(const char *pathname, int Flags, myf MyFlags);

/**
  Gets relative path from a file.

  @param [in]     file       path to file

  @return relative path
*/
const char *get_relative_path(const char *path);

/** Check if the file has the specified suffix
@param[in]    sfx             suffix to look for
@param[in]    path            Filename to check
@return true if it has the ".ibd" suffix. */
bool file_has_suffix(const std::string &sfx, const std::string &path);

/** Check if the file has compression suffix
@param[in]    path            Filename to check
@return true if it has the any compression suffix. */
bool is_compressed_suffix(const std::string &path);

/** Check if the file has encryption suffix
@param[in]    path            Filename to check
@return true if it has the  encryption suffix. */
bool is_encrypted_suffix(const std::string &path);

/** Check if the file has encryption & compression suffix
@param[in]    path            Filename to check
@return true if it has the encryption & compression suffix. */
bool is_encrypted_and_compressed_suffix(const std::string &path);

/** Check if the file has qpress encryption
@param[in]    path            Filename to check
@return true if it has qpress encryption suffix. */
bool is_qpress_file(const std::string &path);

/* Holds the state of a data file read result */
typedef enum {
  XB_FIL_CUR_SUCCESS,
  XB_FIL_CUR_SKIP,
  XB_FIL_CUR_ERROR,
  XB_FIL_CUR_EOF,
  XB_FIL_CUR_CORRUPTED
} xb_fil_cur_result_t;

/* Holds the state needed to copy single data file. */
struct datafile_cur_t {
  File fd;
  char rel_path[FN_REFLEN];
  char abs_path[FN_REFLEN];
  MY_STAT statinfo;
  uint thread_n;
  byte *orig_buf;
  byte *buf;
  uint64_t buf_size;
  uint64_t buf_read;
  uint64_t buf_offset;
};

/**
  Opens a data file.

  @param [in]     file        path to file
  @param [in/out] cursor      cursor containing file metadata.
  @param [in]     read_only   open the file for read only
  @param [in]     buffer_size size of read buffer

  @return false in case of error, true otherwise
*/
bool datafile_open(const char *file, datafile_cur_t *cursor, bool read_only,
                   uint buffer_size);

/** Closes file description from datafile_cur_t. */
void datafile_close(datafile_cur_t *cursor);

/**
  Reads file content into cursor->buf.

  @param [in/out] cursor     cursor containing file metadata.

  @return false in case of error, true otherwise
*/
xb_fil_cur_result_t datafile_read(datafile_cur_t *cursor);

/**
  Restore sparseness of a file. This is used by xtrabackup & xbstream when
  --decompress options is used. This will restore sparseness (punch hole) on
  IBD files that have page compression (COMPRESSION="xxx")

  @param [in]       file          path to file
  @param [in]       buffer_size   size of read buffer
  @param [in/out]   error         error message in case of error

  @return false in case of error, true otherwise
*/
bool restore_sparseness(const char *src_file_path, uint buffer_size,
                        char error[512]);

/**
  Open FIFO file for writing. Wait up to timeout seconds for it to return a
  valid file descriptor. This is done by opening it on non-blocking mode which
  does not block if the file is not open for read. Then later changing FD mode
  to blocking mode.

  @param [in]       path      path to file
  @param [in]       timeout   timeout in seconds.

  @return file descriptor in case of success, -1 otherwise
*/
File open_fifo_for_write_with_timeout(const char *path, uint timeout);

/**
  Open FIFO file for reading. Wait up to timeout seconds for it to return a
  valid file descriptor. This is done by opening it on non-blocking mode waiting
  the FD to report EPOLLIN (FD ready for read).

  @param [in]       path      path to file
  @param [in]       timeout   timeout in seconds.

  @return file descriptor in case of success, -1 otherwise
*/
File open_fifo_for_read_with_timeout(const char *path, uint timeout);
#endif
