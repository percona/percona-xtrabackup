/******************************************************
Copyright (c) 2021-2023 Percona LLC and/or its affiliates.

Streaming implementation for XtraBackup.

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

#include "file_utils.h"
#include <mysql/service_mysql_alloc.h>
#ifdef __APPLE__
#include <sys/event.h>
#else
#include <sys/epoll.h>
#endif
#include <thread>
#include "common.h"
#include "ds_fifo.h"
#include "msg.h"
#include "my_dir.h"
#include "my_io.h"
#include "my_thread_local.h"

using page_type_t = size_t;
/** Return the value of the PAGE_SSIZE field */
constexpr uint32_t FSP_FLAGS_GET_PAGE_SSIZE(uint32_t flags) {
  return (flags & XB_FSP_FLAGS_MASK_PAGE_SSIZE) >> XB_FSP_FLAGS_POS_PAGE_SSIZE;
}

/** The following function is used to fetch data from 4 consecutive
bytes. The most significant byte is at the lowest address.
@param[in]      b       pointer to 4 bytes to read
@return 32 bit integer */
static inline uint32_t mach_read_from_4(const byte *b) {
#ifdef UNIV_DEBUG
  assert(b);
#endif  // UNIV_DEBUG
  return ((static_cast<uint32_t>(b[0]) << 24) |
          (static_cast<uint32_t>(b[1]) << 16) |
          (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]));
}

inline uint16_t mach_read_from_2(const byte *b) {
  return (((unsigned long)(b[0]) << 8) | (unsigned long)(b[1]));
}

/** Read the flags from the tablespace header page.
@param[in]      page    first page of a tablespace
@return the contents of FSP_SPACE_FLAGS */
inline uint32_t fsp_header_get_flags(const page_t *page) {
  return (mach_read_from_4(XB_FSP_HEADER_OFFSET + XB_FSP_SPACE_FLAGS + page));
}

/** Get the file page type.
@param[in]      page            File page
@return page type */
inline page_type_t fil_page_get_type(const byte *page) {
  return (static_cast<page_type_t>(mach_read_from_2(page + XB_FIL_PAGE_TYPE)));
}

/** Calculates the smallest multiple of m that is not smaller than n
 when m is a power of two.  In other words, rounds n up to m * k.
 @param n in: number to round up
 @param m in: alignment, must be a power of two
 @return n rounded up to the smallest possible integer multiple of m */
#define ut_calc_align(n, m) (((n) + ((m)-1)) & ~((m)-1))

#if defined _WIN32 || defined __CYGWIN__ || defined __EMX__ || \
    defined __MSDOS__ || defined __DJGPP__
#define IS_DRIVE_LETTER(C) \
  (((C) >= 'A' && (C) <= 'Z') || ((C) >= 'a' && (C) <= 'z'))
#define HAS_DEVICE(Filename) \
  (IS_DRIVE_LETTER((Filename)[0]) && (Filename)[1] == FN_DEVCHAR)
#define FILE_SYSTEM_PREFIX_LEN(Filename) (HAS_DEVICE(Filename) ? 2 : 0)
#else
#define FILE_SYSTEM_PREFIX_LEN(Filename) ((void)(Filename), 0)
#endif

typedef unsigned long int ulint;

const char *safer_name_suffix(char const *file_name, int *prefix_len_out) {
  char const *p;

  /* Skip file system prefixes, leading file name components that contain
     "..", and leading slashes.  */

  int prefix_len = FILE_SYSTEM_PREFIX_LEN(file_name);

  // Remove ../
  for (p = file_name + prefix_len; *p;) {
    if (p[0] == '.' && p[1] == '.' && (is_directory_separator(p[2]) || !p[2]))
      prefix_len = p + 2 - file_name;

    do {
      char c = *p++;
      if (is_directory_separator(c)) break;
    } while (*p);
  }

  // Remove leading /
  for (p = file_name + prefix_len; is_directory_separator(*p); p++) continue;
  prefix_len = p - file_name;

  if (prefix_len) {
    msg("%s: removing leading '%.*s'.\n", my_progname, prefix_len, file_name);
  }

  /* Unlike tar, file_name is always a regular file, so p can't be null */

  *prefix_len_out = prefix_len;
  return p;
}

/************************************************************************
Check to see if a file exists.
Takes name of the file to check.
@return true if file exists. */
bool file_exists(const char *filename) {
  MY_STAT stat_arg;

  if (!my_stat(filename, &stat_arg, MYF(0))) {
    return (false);
  }

  return (true);
}

/************************************************************************
Return true if character if file separator */
bool is_path_separator(char c) { return is_directory_separator(c); }

/** Create directories recursively.
@return 0 if directories created successfully. */
int mkdirp(const char *pathname, int Flags, myf MyFlags) {
  char parent[PATH_MAX], *p;

  /* make a parent directory path */
  strncpy(parent, pathname, sizeof(parent));
  parent[sizeof(parent) - 1] = 0;

  for (p = parent + strlen(parent); !is_path_separator(*p) && p != parent; p--)
    ;

  *p = 0;

  /* try to create parent directory if it doesn't exist */
  if (!file_exists(parent)) {
    if (p != parent && mkdirp(parent, Flags, MyFlags) != 0) {
      return (-1);
    }
  }

  /* create this one if parent has been created */
  if (my_mkdir(pathname, Flags, MyFlags) == 0) {
    return (0);
  }

  /* if it already exists that is fine */
  if (my_errno() == EEXIST) {
    return (0);
  }

  return (-1);
}

const char *get_relative_path(const char *path) {
  if (test_if_hard_path(path) && is_prefix(path, DEFAULT_MYSQL_HOME) &&
      strcmp(DEFAULT_MYSQL_HOME, FN_ROOTDIR)) {
    path += strlen(DEFAULT_MYSQL_HOME);
    while (is_directory_separator(*path)) path++;
  }
  return path;
}

bool file_has_suffix(const std::string &sfx, const std::string &path) {
  return (path.size() >= sfx.size() &&
          path.compare(path.size() - sfx.size(), sfx.size(), sfx) == 0);
}

bool is_compressed_suffix(const std::string &path) {
  return file_has_suffix("qp", path) || file_has_suffix("lz4", path) ||
         file_has_suffix("zst", path);
}

bool is_encrypted_suffix(const std::string &path) {
  return file_has_suffix("xbcrypt", path);
}

bool is_encrypted_and_compressed_suffix(const std::string &path) {
  return file_has_suffix("qp.xbcrypt", path) ||
         file_has_suffix("lz4.xbcrypt", path) ||
         file_has_suffix("zst.xbcrypt", path);
}

bool is_qpress_file(const std::string &path) {
  return file_has_suffix("qp", path) || file_has_suffix("qp.xbcrypt", path);
}

void datafile_close(datafile_cur_t *cursor) {
  if (cursor->fd != -1) {
    my_close(cursor->fd, MYF(0));
  }
  my_free(cursor->buf);
}

bool datafile_open(const char *file, datafile_cur_t *cursor, bool read_only,
                   uint buffer_size) {
  memset(cursor, 0, sizeof(datafile_cur_t));

  strncpy(cursor->abs_path, file, sizeof(cursor->abs_path));

  /* Get the relative path for the destination tablespace name, i.e. the
  one that can be appended to the backup root directory. Non-system
  tablespaces may have absolute paths for remote tablespaces in MySQL
  5.6+. We want to make "local" copies for the backup. */
  strncpy(cursor->rel_path, get_relative_path(cursor->abs_path),
          sizeof(cursor->rel_path));

  cursor->fd =
      my_open(cursor->abs_path, (read_only) ? O_RDONLY : O_RDWR, MYF(MY_WME));

  if (cursor->fd == -1) {
    return (false);
  }

  if (my_fstat(cursor->fd, &cursor->statinfo)) {

    datafile_close(cursor);

    return (false);
  }

  posix_fadvise(cursor->fd, 0, 0, POSIX_FADV_SEQUENTIAL);

  cursor->buf_size = buffer_size;
  cursor->buf = static_cast<byte *>(
      my_malloc(PSI_NOT_INSTRUMENTED, cursor->buf_size, MYF(MY_FAE)));

  return (true);
}

xb_fil_cur_result_t datafile_read(datafile_cur_t *cursor) {
  ulint to_read;
  ulint count;


  to_read =
      std::min(cursor->statinfo.st_size - cursor->buf_offset, cursor->buf_size);

  if (to_read == 0) {
    return (XB_FIL_CUR_EOF);
  }

  count = my_read(cursor->fd, cursor->buf, to_read, MYF(0));
  if (count == MY_FILE_ERROR) {
    return (XB_FIL_CUR_ERROR);
  }

  posix_fadvise(cursor->fd, cursor->buf_offset, count, POSIX_FADV_DONTNEED);

  cursor->buf_read = count;
  cursor->buf_offset += count;

  return (XB_FIL_CUR_SUCCESS);
}

bool restore_sparseness(const char *src_file_path, uint buffer_size,
                        char error[512]) {
  datafile_cur_t cursor;
  size_t page_size = 0;
  size_t seek = 0;
  if (!file_has_suffix("ibd", src_file_path)) return true;

  if (!datafile_open(src_file_path, &cursor, false, buffer_size)) {
    strcpy(error, "Cannot open file");
    return false;
  }
  auto punch_hole_func = [&](const auto page) {
    if (fil_page_get_type(page) == XB_FIL_PAGE_COMPRESSED) {
#ifdef UNIV_DEBUG
      assert(page_size % (size_t)cursor.statinfo.st_blksize == 0);
#endif
      size_t compressed_len =
          ut_calc_align(mach_read_from_2(page + XB_FIL_PAGE_COMPRESS_SIZE_V1) +
                            XB_FIL_PAGE_DATA,
                        cursor.statinfo.st_blksize);
      if (compressed_len < page_size) {
#ifdef HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
        int ret =
            fallocate(cursor.fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                      seek + compressed_len, page_size - compressed_len);
        if (ret != 0) {
          strcpy(error, "fallocate returned ");
          std::string err = std::to_string(errno);
          strcat(error, err.c_str());
          return false;
        }
#endif  // HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
      }
    }
    return true;
  };

  while (datafile_read(&cursor) == XB_FIL_CUR_SUCCESS) {
    size_t buf_offset = 0;
    if (cursor.buf_offset == cursor.buf_read) {
      const uint32_t flags = fsp_header_get_flags(cursor.buf);
      const ulint ssize = FSP_FLAGS_GET_PAGE_SSIZE(flags);
      if (ssize == 0) {
        page_size = XB_UNIV_PAGE_SIZE_ORIG;
      } else {
        page_size = ((XB_UNIV_ZIP_SIZE_MIN >> 1) << ssize);
      }
#ifdef UNIV_DEBUG
      /* Check this is a valid page size. We might get compressed min page size
       * and uncompressed max page size here */
      assert(page_size >= XB_UNIV_ZIP_SIZE_MIN &&
             page_size <= XB_UNIV_PAGE_SIZE_MAX);
#endif
    }

    for (ulint i = 0; i < cursor.buf_read / page_size; ++i) {
      const auto page = cursor.buf + buf_offset;
      if (!punch_hole_func(page)) return false;

      buf_offset += page_size;
      seek += page_size;
    }
  }
  datafile_close(&cursor);
  return true;
}

File open_fifo_for_write_with_timeout(const char *path, uint timeout) {
  File fd;
  uint attempt = 0;
  do {
    fd = my_open(path, O_WRONLY | O_NONBLOCK, MYF(0));
    if (fd < 0) {
      attempt++;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

  } while (fd < 0 && attempt < timeout);
  if (fd < 0) {
    return -1;
  }
  /* Adjust file flags to blocking - we are in sync mode now. */
  if (fcntl(fd, F_SETFL, O_WRONLY) == -1) {
    my_close(fd, MYF(0));
    return -1;
  }

  /*
   * Signals the read side by writing DS_FIFO_CONTROL_CHAR
   * This triggers EVFILT_READ/EPOLLIN. This is necessary because
   * the read side is also opened in non-blocking mode. We change
   * back to blocking node once kqueue/epoll is notified. The way
   * to notify that data is available to read is to write some data
   * to it.
   */
  my_write(fd, (uchar *)DS_FIFO_CONTROL_CHAR, 1, MYF(0));
  return fd;
}

File open_fifo_for_read_with_timeout(const char *path, uint timeout) {
  int fd;
  uint attempt = 0;
  do {
    fd = my_open(path, O_RDONLY | O_NONBLOCK, MYF(0));
    if (fd < 0) {
      attempt++;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } while (fd < 0 && attempt < timeout);

  if (fd < 0) {
    return -1;
  }

  /* File was open, lets check its open on the other side */
#ifdef __APPLE__
  struct timespec tm = {timeout, 0};
  int kqueue_fd = kqueue();
  if (kqueue_fd < 0) {
    return -1;
  }
  struct kevent ev;
  EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
  if (kevent(kqueue_fd, &ev, 1, NULL, 0, NULL) < 0) {
    return -1;
  }
  if (kevent(kqueue_fd, NULL, 0, &ev, 1, &tm) <= 0) {
    return -1;
  }
  close(kqueue_fd);
#else
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    return -1;
  }
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
    return -1;
  }
  if (epoll_wait(epoll_fd, &ev, 1, timeout * 1000) <= 0) {
    return -1;
  }
  close(epoll_fd);
#endif  // __APPLE__

  if (fcntl(fd, F_SETFL, O_RDONLY) == -1) {
    return -1;
  }

  /* Read the control char to make sure the other side is in sync. */
  char buf[1];
  if (my_read(fd, (uchar *)buf, 1, MYF(0)) <= 0) {
    return -1;
  }
  if (memcmp(buf, DS_FIFO_CONTROL_CHAR, 1)) {
    return -1;
  }

  return fd;
}
