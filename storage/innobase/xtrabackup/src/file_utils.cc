#include "file_utils.h"
#include <mysql/service_mysql_alloc.h>
#include "common.h"
#include "msg.h"
#include "my_dir.h"
#include "my_io.h"
#include "my_thread_local.h"
/* fsp_header_get_flags / FSP_FLAGS_GET_PAGE_SSIZE / fil_page_get_type
 * FIL_PAGE_COMPRESSED / FIL_PAGE_COMPRESSED_AND_ENCRYPTED
 * FIL_PAGE_COMPRESS_SIZE_V1 / FIL_PAGE_DATA */
#include <fsp0fsp.h>
#include "univ.i"

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
  xb_fil_cur_result_t res;
  size_t page_size = 0;

  if (!file_has_suffix("ibd", src_file_path)) return true;

  if (!datafile_open(src_file_path, &cursor, false, buffer_size)) {
    strcpy(error, "Cannot open file");
    return false;
  }

  while ((res = datafile_read(&cursor)) == XB_FIL_CUR_SUCCESS) {
    size_t offset = 0;
    if (cursor.buf_offset == cursor.buf_read) {
      const uint32_t flags = fsp_header_get_flags(cursor.buf);
      const ulint ssize = FSP_FLAGS_GET_PAGE_SSIZE(flags);
      if (ssize == 0) {
        page_size = UNIV_PAGE_SIZE_ORIG;
      } else {
        page_size = ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);
      }
    }

    for (ulint i = 0; i < cursor.buf_read / page_size; ++i) {
      const auto page = cursor.buf + offset;
      if (fil_page_get_type(page) == FIL_PAGE_COMPRESSED ||
          fil_page_get_type(page) == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
#ifdef UNIV_DEBUG
        assert(page_size % (size_t)cursor.statinfo.st_blksize == 0);
#endif
        size_t compressed_len = ut_calc_align(
            mach_read_from_2(page + FIL_PAGE_COMPRESS_SIZE_V1) + FIL_PAGE_DATA,
            cursor.statinfo.st_blksize);
        if (compressed_len < page_size) {
#ifdef HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
          int ret =
              fallocate(cursor.fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                        offset + compressed_len, page_size - compressed_len);
          if (ret != 0) {
            strcpy(error, "fallocate returned ");
            std::string err = std::to_string(errno);
            strcat(error, err.c_str());
            return false;
          }
#endif  // HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
        }
      }
      offset += page_size;
    }
  }
  datafile_close(&cursor);
  return true;
}
