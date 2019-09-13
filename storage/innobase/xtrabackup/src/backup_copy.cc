/******************************************************
hot backup tool for InnoDB
(c) 2009-2015 Percona LLC and/or its affiliates
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

*******************************************************

This file incorporates work covered by the following copyright and
permission notice:

Copyright (c) 2000, 2011, MySQL AB & Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#include <fil0fil.h>
#include <fsp0sysspace.h>
#include <my_default.h>
#include <my_dir.h>
#include <my_sys.h>
#include <my_systime.h>
#include <mysql/psi/mysql_cond.h>
#include <mysqld.h>
#include <page0page.h>
#include <srv0srv.h>
#include <srv0start.h>
#include <ut0mem.h>
#include <ut0new.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include "common.h"
#include "fil_cur.h"
#include "space_map.h"
#include "xb0xb.h"

#include "backup_copy.h"
#include "backup_mysql.h"
#include "keyring_plugins.h"
#include "xtrabackup.h"
#include "xtrabackup_config.h"
#include "xtrabackup_version.h"
#ifdef HAVE_VERSION_CHECK
#include <version_check_pl.h>
#endif

using std::min;

/** Possible values for system variable "innodb_checksum_algorithm". */
extern const char *innodb_checksum_algorithm_names[];

/** Used to define an enumerate type of the system variable
innodb_checksum_algorithm. */
extern TYPELIB innodb_checksum_algorithm_typelib;

/** Names of allowed values of innodb_flush_method */
extern const char *innodb_flush_method_names[];

/** Enumeration of innodb_flush_method */
extern TYPELIB innodb_flush_method_typelib;

/* list of files to sync for --rsync mode */
static std::set<std::string> rsync_list;
static std::mutex rsync_mutex;

/* skip these files on copy-back */
static std::set<std::string> skip_copy_back_list;

class datadir_queue {
  std::queue<datadir_entry_t> queue;
  mysql_mutex_t mutex;
  mysql_cond_t cond;
  bool final;

 public:
  datadir_queue() {
    mysql_mutex_init(0, &mutex, MY_MUTEX_INIT_FAST);
    mysql_cond_init(0, &cond);
    final = false;
  }
  ~datadir_queue() {
    mysql_cond_destroy(&cond);
    mysql_mutex_destroy(&mutex);
  }
  void push(const datadir_entry_t &entry) {
    mysql_mutex_lock(&mutex);
    queue.push(entry);
    mysql_mutex_unlock(&mutex);
    mysql_cond_broadcast(&cond);
  }
  void complete() {
    mysql_mutex_lock(&mutex);
    final = true;
    mysql_mutex_unlock(&mutex);
    mysql_cond_broadcast(&cond);
  }
  bool pop(datadir_entry_t &entry) {
    mysql_mutex_lock(&mutex);
    while (queue.empty() && !final) {
      mysql_cond_wait(&cond, &mutex);
    }
    if (final && queue.empty()) {
      mysql_mutex_unlock(&mutex);
      return false;
    }
    entry = queue.front();
    queue.pop();
    mysql_mutex_unlock(&mutex);
    return true;
  }
  void wake_all() { mysql_cond_broadcast(&cond); }
};

struct datadir_thread_ctxt_t;

/************************************************************************
Represents the context of the thread processing MySQL data directory. */
struct datadir_thread_ctxt_t {
  datadir_queue *queue;
  uint n_thread;
  uint *count;
  ib_mutex_t *count_mutex;
  os_thread_id_t id;
  bool ret;
};

/************************************************************************
Retirn true if character if file separator */
bool is_path_separator(char c) { return is_directory_separator(c); }

/************************************************************************
Holds the state needed to copy single data file. */
struct datafile_cur_t {
  File fd;
  char rel_path[FN_REFLEN];
  char abs_path[FN_REFLEN];
  MY_STAT statinfo;
  uint thread_n;
  byte *orig_buf;
  byte *buf;
  ib_uint64_t buf_size;
  ib_uint64_t buf_read;
  ib_uint64_t buf_offset;
};

static void datafile_close(datafile_cur_t *cursor) {
  if (cursor->fd != -1) {
    my_close(cursor->fd, MYF(0));
  }
  ut_free(cursor->buf);
}

static bool datafile_open(const char *file, datafile_cur_t *cursor,
                          uint thread_n) {
  memset(cursor, 0, sizeof(datafile_cur_t));

  strncpy(cursor->abs_path, file, sizeof(cursor->abs_path));

  /* Get the relative path for the destination tablespace name, i.e. the
  one that can be appended to the backup root directory. Non-system
  tablespaces may have absolute paths for remote tablespaces in MySQL
  5.6+. We want to make "local" copies for the backup. */
  strncpy(cursor->rel_path, xb_get_relative_path(nullptr, cursor->abs_path),
          sizeof(cursor->rel_path));

  cursor->fd = my_open(cursor->abs_path, O_RDONLY, MYF(MY_WME));

  if (cursor->fd == -1) {
    /* The following call prints an error message */
    os_file_get_last_error(TRUE);

    msg("[%02u] error: cannot open file %s\n", thread_n, cursor->abs_path);

    return (false);
  }

  if (my_fstat(cursor->fd, &cursor->statinfo)) {
    msg("[%02u] error: cannot stat %s\n", thread_n, cursor->abs_path);

    datafile_close(cursor);

    return (false);
  }

  posix_fadvise(cursor->fd, 0, 0, POSIX_FADV_SEQUENTIAL);

  ut_a(opt_read_buffer_size >= UNIV_PAGE_SIZE);
  cursor->buf_size = opt_read_buffer_size;
  cursor->buf = static_cast<byte *>(ut_malloc_nokey(cursor->buf_size));

  return (true);
}

static xb_fil_cur_result_t datafile_read(datafile_cur_t *cursor) {
  ulint to_read;
  ulint count;

  xtrabackup_io_throttling();

  to_read =
      min(cursor->statinfo.st_size - cursor->buf_offset, cursor->buf_size);

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

/************************************************************************
Check to see if a file exists.
Takes name of the file to check.
@return true if file exists. */
static bool file_exists(const char *filename) {
  MY_STAT stat_arg;

  if (!my_stat(filename, &stat_arg, MYF(0))) {
    return (false);
  }

  return (true);
}

/************************************************************************
Trim leading slashes from absolute path so it becomes relative */
static const char *trim_dotslash(const char *path) {
  while (*path) {
    if (is_path_separator(*path)) {
      ++path;
      continue;
    }
    if (*path == '.' && is_path_separator(path[1])) {
      path += 2;
      continue;
    }
    break;
  }

  return (path);
}

/** Check if string ends with given suffix.
@return true if string ends with given suffix. */
static bool ends_with(const char *str, const char *suffix) {
  size_t suffix_len = strlen(suffix);
  size_t str_len = strlen(str);
  return (str_len >= suffix_len &&
          strcmp(str + str_len - suffix_len, suffix) == 0);
}

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
  if (errno == EEXIST) {
    return (0);
  }

  return (-1);
}

/************************************************************************
Return true if first and second arguments are the same path. */
bool equal_paths(const char *first, const char *second) {
  char real_first[PATH_MAX];
  char real_second[PATH_MAX];

  if (realpath(first, real_first) == NULL) {
    return false;
  }
  if (realpath(second, real_second) == NULL) {
    return false;
  }

  return (strcmp(real_first, real_second) == 0);
}

/************************************************************************
Check if directory exists. Optionally create directory if doesn't
exist.
@return true if directory exists and if it was created successfully. */
bool directory_exists(const char *dir, bool create) {
  MY_STAT stat_arg;
  char errbuf[MYSYS_STRERROR_SIZE];

  if (my_stat(dir, &stat_arg, MYF(0)) == NULL) {
    if (!create) {
      return (false);
    }

    if (mkdirp(dir, 0777, MYF(0)) < 0) {
      msg("Can not create directory %s: %s\n", dir,
          my_strerror(errbuf, sizeof(errbuf), my_errno()));

      return (false);
    }
  }

  /* could be symlink */
  DIR *directory = opendir(dir);

  if (directory == nullptr) {
    msg("Can not open directory %s: %s\n", dir,
        my_strerror(errbuf, sizeof(errbuf), my_errno()));

    return (false);
  }

  closedir(directory);

  return (true);
}

/************************************************************************
Check that directory exists and it is empty. */
static bool directory_exists_and_empty(const char *dir, const char *comment) {
  if (!directory_exists(dir, true)) {
    return (false);
  }

  DIR *directory = opendir(dir);

  if (directory == nullptr) {
    msg("%s can not open directory %s\n", comment, dir);
    return (false);
  }

  bool empty = true;
  struct dirent *dp;

  while ((dp = readdir(directory)) != nullptr) {
    if (strcmp(dp->d_name, ".") == 0) {
      continue;
    }
    if (strcmp(dp->d_name, "..") == 0) {
      continue;
    }
    empty = false;
    break;
  }

  closedir(directory);

  if (!empty) {
    msg("%s directory %s is not empty!\n", comment, dir);
  }

  return (empty);
}

/************************************************************************
Check if file name ends with given set of suffixes.
@return true if it does. */
static bool filename_matches(const char *filename, const char **ext_list) {
  const char **ext;

  for (ext = ext_list; *ext; ext++) {
    if (ends_with(filename, *ext)) {
      return (true);
    }
  }

  return (false);
}

/************************************************************************
Copy data file for backup. Also check if it is allowed to copy by
comparing its name to the list of known data file types and checking
if passes the rules for partial backup.
@return true if file backed up or skipped successfully. */
static bool datafile_copy_backup(const char *filepath, uint thread_n) {
  const char *ext_list[] = {"MYD", "MYI", "MAD", "MAI", "MRG", "ARM",
                            "ARZ", "CSM", "CSV", "opt", "sdi", NULL};

  /* Get the name and the path for the tablespace. node->name always
  contains the path (which may be absolute for remote tablespaces in
  5.6+). space->name contains the tablespace name in the form
  "./database/table.ibd" (in 5.5-) or "database/table" (in 5.6+). For a
  multi-node shared tablespace, space->name contains the name of the first
  node, but that's irrelevant, since we only need node_name to match them
  against filters, and the shared tablespace is always copied regardless
  of the filters value. */

  if (check_if_skip_table(filepath)) {
    msg_ts("[%02u] Skipping %s.\n", thread_n, filepath);
    return (true);
  }

  if (filename_matches(filepath, ext_list)) {
    return copy_file(ds_data, filepath, filepath, thread_n, FILE_PURPOSE_OTHER);
  }

  return (true);
}

/************************************************************************
Same as datafile_copy_backup, but put file name into the list for
rsync command. */
static bool datafile_rsync_backup(const char *filepath, bool save_to_list,
                                  FILE *f) {
  const char *ext_list[] = {"MYD", "MYI", "MAD", "MAI", "MRG", "ARM",
                            "ARZ", "CSM", "CSV", "opt", "sdi", NULL};

  /* Get the name and the path for the tablespace. node->name always
  contains the path (which may be absolute for remote tablespaces in
  5.6+). space->name contains the tablespace name in the form
  "./database/table.ibd" (in 5.5-) or "database/table" (in 5.6+). For a
  multi-node shared tablespace, space->name contains the name of the first
  node, but that's irrelevant, since we only need node_name to match them
  against filters, and the shared tablespace is always copied regardless
  of the filters value. */

  if (check_if_skip_table(filepath)) {
    return (true);
  }

  if (filename_matches(filepath, ext_list)) {
    std::lock_guard<std::mutex> guard(rsync_mutex);
    fprintf(f, "%s\n", filepath);
    if (save_to_list) {
      rsync_list.insert(filepath);
    }
  }

  return (true);
}

static bool backup_ds_print(ds_file_t *dstfile, const char *message, int len) {
  const char *action = xb_get_copy_action("Writing");
  msg_ts("[%02u] %s %s\n", 0, action, dstfile->path);

  if (ds_write(dstfile, message, len)) {
    goto error;
  }

  msg_ts("[%02u]        ...done\n", 0);
  return true;

error:
  msg("[%02u] Error: %s backup file failed.\n", 0, action);
  return false;
}

bool backup_file_print(const char *filename, const char *message, int len) {
  ds_file_t *dstfile = NULL;
  MY_STAT stat;

  memset(&stat, 0, sizeof(stat));
  stat.st_mtime = my_time(0);
  stat.st_size = len;

  dstfile = ds_open(ds_data, filename, &stat);
  if (dstfile == NULL) {
    msg("[%02u] error: "
        "cannot open the destination stream for %s\n",
        0, filename);
    goto error;
  }

  if (!backup_ds_print(dstfile, message, len)) {
    goto error;
  }

  if (ds_close(dstfile)) {
    goto error_close;
  }

  return (true);

error:
  if (dstfile != NULL) {
    ds_close(dstfile);
  }

error_close:
  return (false); /*ERROR*/
}

bool backup_file_printf(const char *filename, const char *fmt, ...) {
  bool result = false;
  char *buf = 0;
  int buf_len = 0;
  va_list ap;

  va_start(ap, fmt);
  buf_len = vasprintf(&buf, fmt, ap);
  va_end(ap);

  if (buf_len == -1) {
    return false;
  }

  result = backup_file_print(filename, buf, buf_len);

  free(buf);
  return (result);
}

template <typename F>
static bool run_data_threads(const char *dir, F func, uint n,
                             const char *thread_description) {
  datadir_thread_ctxt_t *data_threads;
  uint i, count;
  ib_mutex_t count_mutex;
  bool ret;

  ut_a(thread_description);
  data_threads = (datadir_thread_ctxt_t *)(ut_malloc_nokey(
      sizeof(datadir_thread_ctxt_t) * n));

  mutex_create(LATCH_ID_XTRA_COUNT_MUTEX, &count_mutex);
  count = n;

  datadir_queue queue;

  for (i = 0; i < n; i++) {
    data_threads[i].n_thread = i + 1;
    data_threads[i].count = &count;
    data_threads[i].count_mutex = &count_mutex;
    data_threads[i].queue = &queue;
    os_thread_create(PFS_NOT_INSTRUMENTED, func, &data_threads[i]).start();
  }

  xb_process_datadir(
      dir, "",
      [&](const datadir_entry_t &entry, void *arg) mutable -> bool {
        queue.push(entry);
        return true;
      },
      nullptr);

  queue.complete();

  /* Wait for threads to exit */
  while (1) {
    os_thread_sleep(100000);
    mutex_enter(&count_mutex);
    if (count == 0) {
      mutex_exit(&count_mutex);
      break;
    }
    mutex_exit(&count_mutex);
  }

  mutex_free(&count_mutex);

  ret = true;
  for (i = 0; i < n; i++) {
    ret = data_threads[i].ret && ret;
    if (!data_threads[i].ret) {
      msg("Error: %s thread %u failed.\n", thread_description, i);
    }
  }

  ut_free(data_threads);

  return (ret);
}

/************************************************************************
Write buffer into .ibd file and preserve it's sparsiness. */
bool write_ibd_buffer(ds_file_t *file, unsigned char *buf, size_t buf_len,
                      size_t page_size, size_t block_size) {
  ut_a(buf_len % page_size == 0);

  if (ds_is_sparse_write_supported(file) && page_size > block_size) {
    std::vector<ds_sparse_chunk_t> sparse_map;
    size_t skip = 0, len = 0;
    for (ulint i = 0, page_offs = 0; i < buf_len / page_size;
         ++i, page_offs += page_size) {
      const auto page = buf + page_offs;

      if (Compression::is_compressed_page(page) ||
          fil_page_get_type(page) == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
        ut_ad(page_size % block_size == 0);
        size_t compressed_len =
            mach_read_from_2(page + FIL_PAGE_COMPRESS_SIZE_V1) + FIL_PAGE_DATA;
        if (fil_page_get_type(page) == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
          compressed_len = ut_calc_align(compressed_len, block_size);
        }
        sparse_map.push_back(ds_sparse_chunk_t{skip, len + compressed_len});
        skip = page_size - compressed_len;
        len = 0;
      } else {
        len += page_size;
      }
    }
    sparse_map.push_back(ds_sparse_chunk_t{skip, len});

    if (sparse_map.size() > 1) {
      size_t src_pos = 0, dst_pos = 0;
      for (size_t i = 0; i < sparse_map.size(); ++i) {
        src_pos += sparse_map[i].skip;
        memmove(buf + dst_pos, buf + src_pos, sparse_map[i].len);
        src_pos += sparse_map[i].len;
        dst_pos += sparse_map[i].len;
      }
      if (ds_write_sparse(file, buf, dst_pos, sparse_map.size(),
                          &sparse_map[0])) {
        return (false);
      }
      return (true);
    }
  }

  if (ds_write(file, buf, buf_len)) {
    return (false);
  }
  return (true);
}

/************************************************************************
Copy file for backup/restore.
@return true in case of success. */
bool copy_file(ds_ctxt_t *datasink, const char *src_file_path,
               const char *dst_file_path, uint thread_n,
               file_purpose_t file_purpose, ssize_t pos) {
  char dst_name[FN_REFLEN];
  ds_file_t *dstfile = NULL;
  datafile_cur_t cursor;
  xb_fil_cur_result_t res;
  const char *action;
  page_size_t page_size{0, 0, false};

  if (!datafile_open(src_file_path, &cursor, thread_n)) {
    goto error;
  }

  if (pos >= 0) {
    if ((ssize_t)cursor.statinfo.st_size < pos) {
      msg("xtrabackup: Error file '%s' is smaller than %zd\n", src_file_path,
          pos);
      goto error;
    }
    cursor.statinfo.st_size = pos;
  }

  strncpy(dst_name, cursor.rel_path, sizeof(dst_name));

  dstfile = ds_open(datasink, trim_dotslash(dst_file_path), &cursor.statinfo);
  if (dstfile == NULL) {
    msg("[%02u] error: cannot open the destination stream for %s\n", thread_n,
        dst_name);
    goto error;
  }

  action = xb_get_copy_action();
  if (pos >= 0) {
    msg_ts("[%02u] %s %s to %s up to position %zd\n", thread_n, action,
           src_file_path, dstfile->path, pos);
  } else {
    msg_ts("[%02u] %s %s to %s\n", thread_n, action, src_file_path,
           dstfile->path);
  }

  /* The main copy loop */
  while ((res = datafile_read(&cursor)) == XB_FIL_CUR_SUCCESS) {
    if (file_purpose == FILE_PURPOSE_DATAFILE) {
      if (cursor.buf_offset == cursor.buf_read)
        page_size.copy_from(fsp_header_get_page_size(cursor.buf));
      if (!write_ibd_buffer(dstfile, cursor.buf, cursor.buf_read,
                            page_size.physical(), cursor.statinfo.st_blksize))
        goto error;
    } else {
      if (ds_write(dstfile, cursor.buf, cursor.buf_read)) goto error;
    }
  }

  if (res == XB_FIL_CUR_ERROR) {
    goto error;
  }

  /* close */
  msg_ts("[%02u]        ...done\n", thread_n);
  datafile_close(&cursor);
  if (ds_close(dstfile)) {
    goto error_close;
  }
  return (true);

error:
  datafile_close(&cursor);
  if (dstfile != NULL) {
    ds_close(dstfile);
  }

error_close:
  msg("[%02u] Error: copy_file() failed.\n", thread_n);
  return (false); /*ERROR*/
}

/************************************************************************
Try to move file by renaming it. If source and destination are on
different devices fall back to copy and unlink.
@return true in case of success. */
static bool move_file(ds_ctxt_t *datasink, const char *src_file_path,
                      const char *dst_file_path, const char *dst_dir,
                      uint thread_n, file_purpose_t file_purpose) {
  char errbuf[MYSYS_STRERROR_SIZE];
  char dst_file_path_abs[FN_REFLEN];
  char dst_dir_abs[FN_REFLEN];
  size_t dirname_length;

  snprintf(dst_file_path_abs, sizeof(dst_file_path_abs), "%s/%s", dst_dir,
           dst_file_path);

  dirname_part(dst_dir_abs, dst_file_path_abs, &dirname_length);

  if (!directory_exists(dst_dir_abs, true)) {
    return (false);
  }

  if (file_exists(dst_file_path_abs)) {
    msg("Error: Move file %s to %s failed: Destination "
        "file exists\n",
        src_file_path, dst_file_path_abs);
    return (false);
  }

  msg_ts("[%02u] Moving %s to %s\n", thread_n, src_file_path,
         dst_file_path_abs);

  if (my_rename(src_file_path, dst_file_path_abs, MYF(0)) != 0) {
    if (my_errno() == EXDEV) {
      bool ret;
      ret = copy_file(datasink, src_file_path, dst_file_path, thread_n,
                      file_purpose);
      msg_ts("[%02u] Removing %s\n", thread_n, src_file_path);
      if (unlink(src_file_path) != 0) {
        msg("Error: unlink %s failed: %s\n", src_file_path,
            my_strerror(errbuf, sizeof(errbuf), errno));
      }
      return (ret);
    }
    msg("Can not move file %s to %s: %s\n", src_file_path, dst_file_path_abs,
        my_strerror(errbuf, sizeof(errbuf), my_errno()));
    return (false);
  }

  msg_ts("[%02u]        ...done\n", thread_n);

  return (true);
}

/************************************************************************
Fix InnoDB page checksum after modifying it. */
static void page_checksum_fix(byte *page, const page_size_t &page_size) {
  ib_uint32_t checksum = BUF_NO_CHECKSUM_MAGIC;

  BlockReporter reporter = BlockReporter(false, page, page_size, false);

  if (page_size.is_compressed()) {
    const uint32_t checksum = reporter.calc_zip_checksum(
        page, page_size.physical(),
        static_cast<srv_checksum_algorithm_t>(srv_checksum_algorithm));

    mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
  } else {
    switch ((srv_checksum_algorithm_t)srv_checksum_algorithm) {
      case SRV_CHECKSUM_ALGORITHM_CRC32:
      case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
        checksum = buf_calc_page_crc32(page);
        mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
        break;
      case SRV_CHECKSUM_ALGORITHM_INNODB:
      case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
        checksum = (ib_uint32_t)buf_calc_page_new_checksum(page);
        mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
        checksum = (ib_uint32_t)buf_calc_page_old_checksum(page);
        break;
      case SRV_CHECKSUM_ALGORITHM_NONE:
      case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
        mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
        break;
        /* no default so the compiler will emit a warning if
        new enum is added and not handled here */
    }

    mach_write_to_4(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
                    checksum);
  }

  ut_a(!reporter.is_corrupted());
}

/************************************************************************
Reencrypt datafile header with new master key for copy-back.
@return true in case of success. */
static bool reencrypt_datafile_header(const char *dir, const char *filepath,
                                      uint thread_n) {
  char fullpath[FN_REFLEN];
  byte buf[UNIV_PAGE_SIZE_MAX * 2];
  byte encrypt_info[ENCRYPTION_INFO_SIZE];
  fil_space_t space;

  fn_format(fullpath, filepath, dir, "", MYF(MY_RELATIVE_PATH));

  byte *page = static_cast<byte *>(ut_align(buf, UNIV_PAGE_SIZE_MAX));

  File fd = my_open(fullpath, O_RDWR, MYF(MY_FAE));

  my_seek(fd, 0L, SEEK_SET, MYF(MY_WME));

  size_t len = my_read(fd, page, UNIV_PAGE_SIZE_MAX, MYF(MY_WME));

  if (len < UNIV_PAGE_SIZE_MIN) {
    my_close(fd, MYF(MY_FAE));
    return (false);
  }

  ulint flags = fsp_header_get_flags(page);

  if (!FSP_FLAGS_GET_ENCRYPTION(flags)) {
    my_close(fd, MYF(MY_FAE));
    return (true);
  }

  msg_ts("[%02u] Encrypting %s tablespace header with new master key.\n",
         thread_n, fullpath);

  memset(encrypt_info, 0, ENCRYPTION_INFO_SIZE);

  space.id = page_get_space_id(page);
  bool found = xb_fetch_tablespace_key(space.id, space.encryption_key,
                                       space.encryption_iv);
  ut_a(found);
  space.encryption_type = Encryption::AES;
  space.encryption_klen = ENCRYPTION_KEY_LEN;

  const page_size_t page_size(fsp_header_get_page_size(page));

  if (!Encryption::fill_encryption_info(space.encryption_key,
                                        space.encryption_iv, encrypt_info,
                                        false, true)) {
    my_close(fd, MYF(MY_FAE));
    return (false);
  }

  ulint offset = fsp_header_get_encryption_offset(page_size);

  memcpy(page + offset, encrypt_info, ENCRYPTION_INFO_SIZE);

  page_checksum_fix(page, page_size);

  my_seek(fd, 0L, SEEK_SET, MYF(MY_WME));
  my_write(fd, page, len, MYF(MY_FAE | MY_NABP));

  my_close(fd, MYF(MY_FAE));

  return (true);
}

/************************************************************************
Copy or move file depending on current mode.
@return true in case of success. */
static bool copy_or_move_file(const char *src_file_path,
                              const char *dst_file_path, const char *dst_dir,
                              uint thread_n, file_purpose_t file_purpose) {
  ds_ctxt_t *datasink = ds_data; /* copy to datadir by default */
  bool ret;

  /* File is located outsude of the datadir */
  char external_dir[FN_REFLEN];

  if (Fil_path::type_of_path(dst_file_path) == Fil_path::absolute) {
    /* Make sure that destination directory exists */
    size_t dirname_length;

    dirname_part(external_dir, dst_file_path, &dirname_length);

    if (!directory_exists(external_dir, true)) {
      return (false);
    }

    datasink = ds_create(external_dir, DS_TYPE_LOCAL);

    dst_file_path = dst_file_path + dirname_length;
    dst_dir = external_dir;
  }

  ret =
      (xtrabackup_copy_back ? copy_file(datasink, src_file_path, dst_file_path,
                                        thread_n, file_purpose)
                            : move_file(datasink, src_file_path, dst_file_path,
                                        dst_dir, thread_n, file_purpose));

  if (opt_generate_new_master_key && (file_purpose == FILE_PURPOSE_DATAFILE ||
                                      file_purpose == FILE_PURPOSE_UNDO_LOG)) {
    reencrypt_datafile_header(dst_dir, dst_file_path, thread_n);
  }

  if (datasink != ds_data) {
    ds_destroy(datasink);
  }

  return (ret);
}

static void backup_thread_func(datadir_thread_ctxt_t *ctx, bool prep_mode,
                               FILE *rsync_tmpfile) {
  bool ret = true;
  datadir_entry_t entry;

  if (my_thread_init()) {
    ret = false;
    goto cleanup;
  }

  while (ctx->queue->pop(entry)) {
    char name[FN_REFLEN];
    char path[FN_REFLEN];

    if (!entry.db_name.empty()) {
      snprintf(name, FN_REFLEN, "%s/%s", entry.db_name.c_str(),
               entry.file_name.c_str());
    } else {
      snprintf(name, FN_REFLEN, "%s", entry.file_name.c_str());
    }

    fn_format(path, name, entry.datadir.c_str(), "",
              MY_UNPACK_FILENAME | MY_SAFE_PATH);

    if (!entry.is_empty_dir) {
      if (opt_rsync) {
        ret = datafile_rsync_backup(path, !prep_mode, rsync_tmpfile);
      } else {
        ret = datafile_copy_backup(path, ctx->n_thread);
      }
      if (!ret) {
        msg("Failed to copy file %s\n", path);
        ret = false;
        goto cleanup;
      }
    } else if (!prep_mode) {
      /* backup fake file into empty directory */
      char opath[FN_REFLEN + 10];
      snprintf(opath, sizeof(opath), "%s/db.opt", path);
      if (!(ret = backup_file_printf(trim_dotslash(opath), "%s", ""))) {
        msg("Failed to create file %s\n", opath);
        ret = false;
        goto cleanup;
      }
    }
  }

cleanup:
  my_thread_end();

  mutex_enter(ctx->count_mutex);
  --(*ctx->count);
  mutex_exit(ctx->count_mutex);

  ctx->ret = ret;
}

bool backup_files(const char *from, bool prep_mode) {
  char rsync_tmpfile_name[FN_REFLEN];
  FILE *rsync_tmpfile = NULL;
  bool ret = true;

  if (prep_mode && !opt_rsync) {
    return (true);
  }

  if (opt_rsync) {
    snprintf(rsync_tmpfile_name, sizeof(rsync_tmpfile_name), "%s/%s%d",
             opt_mysql_tmpdir, "xtrabackup_rsyncfiles_pass", prep_mode ? 1 : 2);
    rsync_tmpfile = fopen(rsync_tmpfile_name, "w");
    if (rsync_tmpfile == NULL) {
      msg("Error: can't create file %s\n", rsync_tmpfile_name);
      return (false);
    }
  }

  msg_ts("Starting %s non-InnoDB tables and files\n",
         prep_mode ? "prep copy of" : "to backup");

  run_data_threads(from,
                   std::bind(backup_thread_func, std::placeholders::_1,
                             prep_mode, rsync_tmpfile),
                   xtrabackup_parallel, "backup");

  if (opt_rsync) {
    std::stringstream cmd;
    int err;

    if (buffer_pool_filename && file_exists(buffer_pool_filename)) {
      /* Check if dump of buffer pool has completed
      and potentially wait for it to complete
      is only executed before FTWRL - prep_mode */
      if (prep_mode && opt_dump_innodb_buffer_pool) {
        check_dump_innodb_buffer_pool(mysql_connection);
      }
      fprintf(rsync_tmpfile, "%s\n", buffer_pool_filename);
      rsync_list.insert(buffer_pool_filename);
    }
    if (file_exists("ib_lru_dump")) {
      fprintf(rsync_tmpfile, "%s\n", "ib_lru_dump");
      rsync_list.insert("ib_lru_dump");
    }

    fclose(rsync_tmpfile);
    rsync_tmpfile = NULL;

    cmd << "rsync -t . --files-from=" << rsync_tmpfile_name << " "
        << xtrabackup_target_dir;

    msg_ts("Starting rsync as: %s\n", cmd.str().c_str());
    if ((err = system(cmd.str().c_str()) && !prep_mode) != 0) {
      msg_ts("Error: rsync failed with error code %d\n", err);
      ret = false;
      goto out;
    }
    msg_ts("rsync finished successfully.\n");

    if (!prep_mode && !opt_no_lock) {
      char path[FN_REFLEN];
      char dst_path[FN_REFLEN * 2 + 1];
      char *newline;

      /* Remove files that have been removed between first and
      second passes. Cannot use "rsync --delete" because it
      does not work with --files-from. */
      snprintf(rsync_tmpfile_name, sizeof(rsync_tmpfile_name), "%s/%s",
               opt_mysql_tmpdir, "xtrabackup_rsyncfiles_pass1");

      rsync_tmpfile = fopen(rsync_tmpfile_name, "r");
      if (rsync_tmpfile == NULL) {
        msg("Error: can't open file %s\n", rsync_tmpfile_name);
        return (false);
      }

      while (fgets(path, sizeof(path), rsync_tmpfile)) {
        newline = strchr(path, '\n');
        if (newline) {
          *newline = 0;
        }
        if (rsync_list.count(path) < 1) {
          snprintf(dst_path, sizeof(dst_path), "%s/%s", xtrabackup_target_dir,
                   path);
          msg_ts("Removing %s\n", dst_path);
          unlink(dst_path);
        }
      }

      fclose(rsync_tmpfile);
      rsync_tmpfile = NULL;
    }
  }

  msg_ts("Finished %s non-InnoDB tables and files\n",
         prep_mode ? "a prep copy of" : "backing up");

out:
  if (rsync_tmpfile != NULL) {
    fclose(rsync_tmpfile);
  }

  return (ret);
}

void Myrocks_datadir::scan_dir(const std::string &dir,
                               const char *dest_data_dir,
                               const char *dest_wal_dir, scan_type_t scan_type,
                               file_list &result) const {
  os_file_scan_directory(
      dir.c_str(),
      [&](const char *path, const char *name) mutable -> void {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
          return;
        }
        char buf[FN_REFLEN];
        fn_format(buf, name, path, "", MY_UNPACK_FILENAME | MY_SAFE_PATH);
        if (!ends_with(name, ".log")) {
          if (scan_type != SCAN_WAL) {
            result.push_back(
                datadir_entry_t("", buf, dest_data_dir, name, false));
          }
        } else {
          if (scan_type != SCAN_DATA) {
            result.push_back(
                datadir_entry_t("", buf, dest_wal_dir, name, false));
          }
        }
      },
      false);
}

Myrocks_datadir::file_list Myrocks_datadir::files(
    const char *dest_data_dir, const char *dest_wal_dir) const {
  file_list result;

  scan_dir(rocksdb_datadir, dest_data_dir, dest_wal_dir, SCAN_ALL, result);
  if (!rocksdb_wal_dir.empty()) {
    scan_dir(rocksdb_wal_dir, NULL, dest_wal_dir, SCAN_WAL, result);
  }

  return result;
}

Myrocks_datadir::file_list Myrocks_datadir::data_files(
    const char *dest_datadir) const {
  file_list result;

  scan_dir(rocksdb_datadir, dest_datadir, NULL, SCAN_DATA, result);

  return result;
}

Myrocks_datadir::file_list Myrocks_datadir::wal_files(
    const char *dest_wal_dir) const {
  file_list result;

  scan_dir(rocksdb_datadir, NULL, dest_wal_dir, SCAN_WAL, result);
  if (!rocksdb_wal_dir.empty()) {
    scan_dir(rocksdb_wal_dir, NULL, dest_wal_dir, SCAN_WAL, result);
  }

  return result;
}

void Myrocks_checkpoint::create(MYSQL *con) {
  msg_ts("xtrabackup: Creating RocksDB checkpoint\n");

  xb_mysql_query(con, "SET SESSION rocksdb_disable_file_deletions = TRUE",
                 false);

  constexpr auto checkpoint_basename = ".xtrabackup_rocksdb_checkpoint";
  using namespace std::chrono;
  std::stringstream dirname_s;
  dirname_s << checkpoint_basename << "_"
            << duration_cast<milliseconds>(
                   system_clock::now().time_since_epoch())
                   .count();
  std::stringstream query;
  query << "SET SESSION rocksdb_create_temporary_checkpoint = '"
        << dirname_s.str() << "'";

  xb_mysql_query(con, query.str().c_str(), false);

  checkpoint_dir = dirname_s.str();
  rocksdb_datadir = opt_rocksdb_datadir;
  rocksdb_wal_dir = opt_rocksdb_wal_dir != nullptr ? opt_rocksdb_wal_dir : "";

  this->con = con;
}

void Myrocks_checkpoint::enable_file_deletions() const {
  xb_mysql_query(con, "SET SESSION rocksdb_disable_file_deletions = FALSE",
                 false);
}

void Myrocks_checkpoint::remove() const {
  msg_ts("xtrabackup: Removing RocksDB checkpoint\n");

  xb_mysql_query(con, "SET SESSION rocksdb_create_temporary_checkpoint = NULL",
                 false);
}

Myrocks_checkpoint::file_list Myrocks_checkpoint::wal_files(
    const log_status_t &log_status) const {
  file_list wal_files;

  for (const auto &f : log_status.rocksdb_wal_files) {
    char path[FN_REFLEN];
    fn_format(path, f.path_name.c_str() + 1,
              rocksdb_wal_dir.empty() ? rocksdb_datadir.c_str()
                                      : rocksdb_wal_dir.c_str(),
              "", MY_UNPACK_FILENAME | MY_SAFE_PATH);
    wal_files.push_back(datadir_entry_t("", path, ".rocksdb",
                                        f.path_name.c_str() + 1, false,
                                        f.file_size_bytes));
  }
  return wal_files;
}

Myrocks_checkpoint::file_list Myrocks_checkpoint::checkpoint_files(
    const log_status_t &log_status) const {
  std::set<std::string> live_wal_set;
  for (const auto &f : log_status.rocksdb_wal_files) {
    live_wal_set.insert(f.path_name.substr(1));
  }

  file_list checkpoint_files = Myrocks_datadir(checkpoint_dir).files();

  checkpoint_files.erase(
      std::remove_if(checkpoint_files.begin(), checkpoint_files.end(),
                     [&live_wal_set](const datadir_entry_t &x) {
                       return (live_wal_set.count(x.file_name) > 0);
                     }),
      checkpoint_files.end());

  return checkpoint_files;
}

static void par_copy_rocksdb_files(const Myrocks_datadir::const_iterator &start,
                                   const Myrocks_datadir::const_iterator &end,
                                   size_t thread_n, ds_ctxt_t *ds,
                                   bool *result) {
  for (auto it = start; it != end; it++) {
    if (ends_with(it->path.c_str(), ".qp") ||
        ends_with(it->path.c_str(), ".lz4") ||
        ends_with(it->path.c_str(), ".xbcrypt")) {
      continue;
    }
    if (!copy_file(ds, it->path.c_str(), it->rel_path.c_str(), thread_n,
                   FILE_PURPOSE_OTHER, it->file_size)) {
      *result = false;
    }
    if (!*result) {
      break;
    }
  }
}

static void backup_rocksdb_files(const Myrocks_datadir::const_iterator &start,
                                 const Myrocks_datadir::const_iterator &end,
                                 size_t thread_n, bool *result) {
  for (auto it = start; it != end; it++) {
    if (!copy_file(ds_uncompressed_data, it->path.c_str(), it->rel_path.c_str(),
                   thread_n, FILE_PURPOSE_OTHER, it->file_size)) {
      *result = false;
    }
    if (!*result) {
      break;
    }
  }
}

static bool backup_rocksdb_wal(const Myrocks_checkpoint &checkpoint,
                               const log_status_t &log_status) {
  bool result = true;

  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;

  std::function<void(const Myrocks_datadir::const_iterator &,
                     const Myrocks_datadir::const_iterator &, size_t)>
      copy = std::bind(&backup_rocksdb_files, _1, _2, _3, &result);

  const auto live_wal_files = checkpoint.wal_files(log_status);

  par_for(PFS_NOT_INSTRUMENTED, live_wal_files, xtrabackup_parallel, copy);

  if (!result) {
    msg("xtrabackup: error: failed to backup rocksdb WAL files.\n");
  }

  return result;
}

static bool backup_rocksdb_checkpoint(const Myrocks_checkpoint &checkpoint,
                                      const log_status_t &log_status) {
  bool result = true;

  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;

  std::function<void(const Myrocks_datadir::const_iterator &,
                     const Myrocks_datadir::const_iterator &, size_t)>
      copy = std::bind(&backup_rocksdb_files, _1, _2, _3, &result);

  auto checkpoint_files = checkpoint.checkpoint_files(log_status);

  par_for(PFS_NOT_INSTRUMENTED, checkpoint_files, xtrabackup_parallel, copy);

  if (!result) {
    msg("xtrabackup: error: failed to backup rocksdb datadir.\n");
  }

  return result;
}

/* Backup non-InnoDB data.
@param  backup_lsn   backup LSN
@return true if success. */
bool backup_start(Backup_context &context) {
  if (!opt_no_lock) {
    if (opt_safe_slave_backup) {
      if (!wait_for_safe_slave(mysql_connection)) {
        return (false);
      }
    }

    if (!backup_files(MySQL_datadir_path.path().c_str(), true)) {
      return (false);
    }

    history_lock_time = time(NULL);

    if (!lock_tables_maybe(mysql_connection, opt_backup_lock_timeout,
                           opt_backup_lock_retry_count)) {
      return (false);
    }
  }

  if (!backup_files(MySQL_datadir_path.path().c_str(), false)) {
    return (false);
  }

  /* There is no need to stop slave thread before copying non-Innodb data when
  --no-lock option is used because --no-lock option requires that no DDL or
  DML to non-transaction tables can occur. */
  if (opt_no_lock && opt_safe_slave_backup) {
    if (!wait_for_safe_slave(mysql_connection)) {
      return (false);
    }
  }

  if (have_rocksdb) {
    context.myrocks_checkpoint.create(mysql_connection);
  }

  msg_ts("Executing FLUSH NO_WRITE_TO_BINLOG BINARY LOGS\n");
  xb_mysql_query(mysql_connection, "FLUSH NO_WRITE_TO_BINLOG BINARY LOGS",
                 false);

  context.log_status = log_status_get(mysql_connection);

  if (!write_current_binlog_file(mysql_connection)) {
    return (false);
  }

  if (have_rocksdb) {
    if (!backup_rocksdb_wal(context.myrocks_checkpoint, context.log_status)) {
      return (false);
    }
    context.myrocks_checkpoint.enable_file_deletions();
  }

  if (opt_slave_info) {
    if (!write_slave_info(mysql_connection)) {
      return (false);
    }
  }

  write_binlog_info(mysql_connection);

  if (have_flush_engine_logs) {
    msg_ts("Executing FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS...\n");
    xb_mysql_query(mysql_connection, "FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS",
                   false);
  }

  return (true);
}

/* Finsh the backup. Release all locks. Write down backup metadata.
@return true if success. */
bool backup_finish(Backup_context &context) {
  /* release all locks */
  if (!opt_no_lock) {
    unlock_all(mysql_connection);
    history_lock_time = 0;
  } else {
    history_lock_time = time(NULL) - history_lock_time;
  }

  if (opt_safe_slave_backup && sql_thread_started) {
    msg("Starting slave SQL thread\n");
    xb_mysql_query(mysql_connection, "START SLAVE SQL_THREAD", false);
  }

  /* Copy buffer pool dump or LRU dump */
  if (!opt_rsync) {
    if (opt_dump_innodb_buffer_pool) {
      check_dump_innodb_buffer_pool(mysql_connection);
    }
    if (buffer_pool_filename && file_exists(buffer_pool_filename)) {
      const char *dst_name;

      dst_name = trim_dotslash(buffer_pool_filename);
      copy_file(ds_data, buffer_pool_filename, dst_name, 0, FILE_PURPOSE_OTHER);
    }
    if (file_exists("ib_lru_dump")) {
      copy_file(ds_data, "ib_lru_dump", "ib_lru_dump", 0, FILE_PURPOSE_OTHER);
    }
  }

  if (have_rocksdb) {
    backup_rocksdb_checkpoint(context.myrocks_checkpoint, context.log_status);
    context.myrocks_checkpoint.remove();
  }

  msg_ts("Backup created in directory '%s'\n", xtrabackup_target_dir);
  if (!mysql_binlog_position.empty()) {
    msg("MySQL binlog position: %s\n", mysql_binlog_position.c_str());
  }
  if (!mysql_slave_position.empty() && opt_slave_info) {
    msg("MySQL slave binlog position: %s\n", mysql_slave_position.c_str());
  }

  if (!write_backup_config_file()) {
    return (false);
  }

  if (!write_xtrabackup_info(mysql_connection)) {
    return (false);
  }

  return (true);
}

bool copy_if_ext_matches(const char **ext_list, const datadir_entry_t &entry,
                         void *arg) {
  if (entry.is_empty_dir ||
      !filename_matches(entry.rel_path.c_str(), ext_list)) {
    return true;
  }

  if (file_exists(entry.rel_path.c_str())) {
    unlink(entry.rel_path.c_str());
  }

  if (!copy_file(ds_data, entry.path.c_str(), entry.rel_path.c_str(), 1,
                 FILE_PURPOSE_OTHER)) {
    msg("Failed to copy file %s\n", entry.path.c_str());
    return false;
  }

  return true;
}

struct binlog_file_location {
  /* binlog file path */
  std::string path;

  /* binlog file name */
  std::string name;

  /* binlog index path */
  std::string index_path;

  /* binlog index name */
  std::string index_name;

  /**
    Find binary log file and index in the backup directory

    @param[in]  dir     backup directory
    @param[out] binlog  binlog location
    @param[out] error   true if error
    @return     true if found
  */
  static bool find_binlog(const std::string &dir, binlog_file_location &binlog,
                          bool &error);

  std::vector<std::string> files() const {
    std::vector<std::string> result;
    if (!name.empty()) {
      result.push_back(name);
    }
    if (!index_name.empty()) {
      result.push_back(index_name);
    }
    return (result);
  }

  binlog_file_location target_location(const std::string &datadir) const {
    binlog_file_location r;

    if (!name.empty()) {
      std::string suffix = fn_ext(name.c_str());

      if (opt_log_bin != nullptr) {
        if (Fil_path::is_absolute_path(opt_log_bin)) {
          r.name =
              std::string(opt_log_bin).substr(0, dirname_length(opt_log_bin)) +
              suffix;
          r.path = opt_log_bin + suffix;
        } else {
          char buf[FN_REFLEN];
          fn_format(buf, opt_log_bin, datadir.c_str(), "", MY_UNPACK_FILENAME);
          r.name = opt_log_bin + suffix;
          r.path = buf + suffix;
        }
      } else {
        r.name = name;
        r.path = path;
      }
    }

    if (!index_name.empty()) {
      if (opt_binlog_index_name != nullptr) {
        r.index_path = opt_binlog_index_name;
        if (!ends_with(opt_binlog_index_name, ".index")) {
          r.index_path.append(".index");
        }
        r.index_name =
            r.index_path.substr(dirname_length(r.index_path.c_str()));
        if (!Fil_path::is_absolute_path(r.index_path)) {
          char buf[FN_REFLEN];
          fn_format(buf, r.index_path.c_str(), datadir.c_str(), "",
                    MY_UNPACK_FILENAME);
          r.index_path = buf;
        }
      } else if (opt_log_bin != nullptr) {
        char buf[FN_REFLEN];

        fn_format(buf, r.name.c_str(), "", ".index", MY_REPLACE_EXT);
        r.index_name = buf;

        fn_format(buf, r.path.c_str(), "", ".index", MY_REPLACE_EXT);
        r.index_path = buf;
      } else {
        r.index_name = index_name;

        char buf[FN_REFLEN];
        fn_format(buf, index_name.c_str(), datadir.c_str(), "",
                  MY_UNPACK_FILENAME);
        r.index_path = buf;
      }
    }

    return (r);
  }
};

bool binlog_file_location::find_binlog(const std::string &dir,
                                       binlog_file_location &binlog,
                                       bool &error) {
  binlog = binlog_file_location();
  error = false;

  Dir_Walker::walk(dir, false, [&](const std::string &path) mutable {
    if (ends_with(path.c_str(), ".index") && !Dir_Walker::is_directory(path)) {
      std::ifstream f_index(path);
      if (f_index.fail()) {
        msg("xtrabackup: Error cannot read '%s'\n", path.c_str());
        error = true;
        return (false);
      }

      char dirname[FN_REFLEN];
      size_t dirname_length;
      std::string binlog_dir;

      for (std::string line; std::getline(f_index, line);) {
        dirname_part(dirname, line.c_str(), &dirname_length);
        binlog.name = line.substr(dirname_length, std::string::npos);
        binlog_dir = dirname;
        binlog.path = line;
      }

      binlog.index_path = path;

      dirname_part(dirname, binlog.index_path.c_str(), &dirname_length);
      binlog.index_name =
          binlog.index_path.substr(dirname_length, std::string::npos);
    }

    return (true);
  });

  return (!binlog.name.empty());
}

bool copy_incremental_over_full() {
  const char *ext_list[] = {"MYD", "MYI", "MAD", "MAI", "MRG", "ARM",
                            "ARZ", "CSM", "CSV", "opt", "sdi", nullptr};
  const char *sup_files[] = {"xtrabackup_binlog_info",
                             "xtrabackup_galera_info",
                             "xtrabackup_slave_info",
                             "xtrabackup_info",
                             "xtrabackup_keys",
                             "xtrabackup_tablespaces",
                             "ib_lru_dump",
                             nullptr};
  bool ret = true;
  char path[FN_REFLEN];
  int i;

  /* If we were applying an incremental change set, we need to make
  sure non-InnoDB files and xtrabackup_* metainfo files are copied
  to the full backup directory. */

  if (xtrabackup_incremental) {
    ds_data = ds_create(xtrabackup_target_dir, DS_TYPE_LOCAL);

    if (!xb_process_datadir(
            xtrabackup_incremental_dir, "",
            std::bind(copy_if_ext_matches, ext_list, std::placeholders::_1,
                      std::placeholders::_2),
            nullptr)) {
      goto cleanup;
    }

    /* copy buffer pool dump */
    if (innobase_buffer_pool_filename) {
      const char *src_name;

      src_name = trim_dotslash(innobase_buffer_pool_filename);

      snprintf(path, sizeof(path), "%s/%s", xtrabackup_incremental_dir,
               src_name);

      if (file_exists(path)) {
        ret = copy_file(ds_data, path, innobase_buffer_pool_filename, 0,
                        FILE_PURPOSE_OTHER);
      }
    }

    if (!ret) {
      goto cleanup;
    }

    /* copy supplementary files */

    for (i = 0; sup_files[i]; i++) {
      snprintf(path, sizeof(path), "%s/%s", xtrabackup_incremental_dir,
               sup_files[i]);

      if (file_exists(path)) {
        if (file_exists(sup_files[i])) {
          unlink(sup_files[i]);
        }
        ret = copy_file(ds_data, path, sup_files[i], 0, FILE_PURPOSE_OTHER);
        if (!ret) {
          goto cleanup;
        }
      }
    }

    /* copy binary log */

    binlog_file_location binlog;
    char fullpath[FN_REFLEN];
    bool err;

    if (binlog_file_location::find_binlog(".", binlog, err)) {
      for (auto file : binlog.files()) {
        unlink(file.c_str());
      }
    }

    if (err) {
      ret = false;
      goto cleanup;
    }

    if (binlog_file_location::find_binlog(xtrabackup_incremental_dir, binlog,
                                          err)) {
      for (auto file : binlog.files()) {
        fn_format(fullpath, file.c_str(), xtrabackup_incremental_dir, "",
                  MYF(MY_RELATIVE_PATH));
        ret =
            copy_file(ds_data, fullpath, file.c_str(), 0, FILE_PURPOSE_BINLOG);
        if (!ret) {
          goto cleanup;
        }
      }
    }

    if (err) {
      ret = false;
      goto cleanup;
    }
  }

  /* copy rocksdb datadir */
  snprintf(path, sizeof(path), "%s/%s", xtrabackup_incremental_dir,
           ROCKSDB_SUBDIR);
  if (directory_exists(path, false)) {
    Myrocks_datadir rocksdb(path);

    if (directory_exists(ROCKSDB_SUBDIR, false)) {
      Myrocks_datadir old_rocksdb(ROCKSDB_SUBDIR);

      /* remove .rocksdb from the full backup first */
      for (const auto &file : old_rocksdb.files()) {
        if (unlink(file.path.c_str())) {
          msg("xtrabackup: error: unable to unlink file '%s'\n",
              file.path.c_str());
          ret = false;
          goto cleanup;
        }
      }
      if (rmdir(ROCKSDB_SUBDIR)) {
        msg("xtrabackup: error: unable to remove directory '" ROCKSDB_SUBDIR
            "'\n");
        ret = false;
        goto cleanup;
      }
    }

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    bool result = true;
    std::function<void(const Myrocks_datadir::const_iterator &,
                       const Myrocks_datadir::const_iterator &, size_t)>
        copy = std::bind(&par_copy_rocksdb_files, _1, _2, _3, ds_data, &result);

    par_for(PFS_NOT_INSTRUMENTED, rocksdb.files(), xtrabackup_parallel, copy);
  }

cleanup:
  if (ds_data != NULL) {
    ds_destroy(ds_data);
  }

  return (ret);
}

/* Removes empty directories and files in database subdirectories if those
files match given list of file extensions.
@param[in]	ext_list	list of extensions to match against
@param[in]	entry		datadir entry
@param[in]	arg		unused
@return true if success */
bool rm_for_cleanup_full_backup(const char **ext_list,
                                const datadir_entry_t &entry, void *arg) {
  char path[FN_REFLEN];

  if (entry.db_name.empty()) {
    return (true);
  }

  fn_format(path, entry.rel_path.c_str(), entry.datadir.c_str(), "",
            MY_UNPACK_FILENAME | MY_SAFE_PATH);

  if (entry.is_empty_dir) {
    if (rmdir(path) != 0) {
      return (false);
    }
  }

  if (xtrabackup_incremental && !entry.is_empty_dir &&
      !filename_matches(path, ext_list)) {
    if (unlink(path) != 0) {
      return (false);
    }
  }

  return (true);
}

bool cleanup_full_backup() {
  const char *ext_list[] = {"delta", "meta", "ibd", NULL};
  bool ret = true;

  /* If we are applying an incremental change set, we need to make
  sure non-InnoDB files are cleaned up from full backup dir before
  we copy files from incremental dir. */

  ret = xb_process_datadir(
      xtrabackup_target_dir, "",
      std::bind(rm_for_cleanup_full_backup, ext_list, std::placeholders::_1,
                std::placeholders::_2),
      nullptr);

  return (ret);
}

bool apply_log_finish() {
  if (!cleanup_full_backup() || !copy_incremental_over_full()) {
    return (false);
  }

  return (true);
}

bool should_skip_file_on_copy_back(const char *filepath) {
  const char *filename;
  char c_tmp;
  int i_tmp;

  const char *ext_list[] = {"backup-my.cnf",
                            "xtrabackup_logfile",
                            "xtrabackup_binary",
                            "xtrabackup_binlog_info",
                            "xtrabackup_checkpoints",
                            "xtrabackup_tablespaces",
                            ".qp",
                            ".lz4",
                            ".pmap",
                            ".tmp",
                            ".xbcrypt",
                            NULL};

  filename = base_name(filepath);

  /* skip .qp and .xbcrypt files */
  if (filename_matches(filename, ext_list)) {
    return true;
  }

  /* skip undo tablespaces */
  if (sscanf(filename, "undo_%d%c", &i_tmp, &c_tmp) == 1) {
    return true;
  }

  /* skip redo logs */
  if (sscanf(filename, "ib_logfile%d%c", &i_tmp, &c_tmp) == 1) {
    return true;
  }

  /* skip innodb data files */
  for (auto iter(srv_sys_space.files_begin()), end(srv_sys_space.files_end());
       iter != end; ++iter) {
    if (strcmp(iter->name(), filename) == 0) {
      return true;
    }
  }

  /* skip rocksdb files (we'll copy them to later to the rocksdb_datadir) */
  if (strstr(filepath, FN_DIRSEP ROCKSDB_SUBDIR FN_DIRSEP) != nullptr) {
    return true;
  }

  if (skip_copy_back_list.count(filename) > 0) {
    return true;
  }

  return false;
}

static void copy_back_thread_func(datadir_thread_ctxt_t *ctx) {
  bool ret = true;
  datadir_entry_t entry;

  if (my_thread_init()) {
    ret = false;
    goto cleanup;
  }

  while (ctx->queue->pop(entry)) {
    /* create empty directories */
    if (entry.is_empty_dir) {
      msg_ts("[%02u] Creating directory %s\n", ctx->n_thread,
             entry.path.c_str());

      if (mkdirp(entry.path.c_str(), 0777, MYF(0)) < 0) {
        char errbuf[MYSYS_STRERROR_SIZE];

        msg("Can not create directory %s: %s\n", entry.path.c_str(),
            my_strerror(errbuf, sizeof(errbuf), my_errno()));
        ret = false;

        goto cleanup;
      }

      msg_ts("[%02u] ...done.\n", ctx->n_thread);
      continue;
    }

    if (should_skip_file_on_copy_back(entry.path.c_str())) {
      continue;
    }

    file_purpose_t file_purpose;
    if (Fil_path::has_suffix(IBD, entry.path)) {
      file_purpose = FILE_PURPOSE_DATAFILE;
    } else if (Fil_path::has_suffix(IBU, entry.path)) {
      file_purpose = FILE_PURPOSE_UNDO_LOG;
    } else {
      file_purpose = FILE_PURPOSE_OTHER;
    }

    std::string dst_path = entry.rel_path;

    if (file_purpose == FILE_PURPOSE_DATAFILE ||
        file_purpose == FILE_PURPOSE_UNDO_LOG) {
      std::string tablespace_name = entry.path;
      /* Remove starting ./ and trailing .ibd/.ibu from tablespace name */
      tablespace_name = tablespace_name.substr(2, tablespace_name.length() - 6);
      std::string external_file_name =
          Tablespace_map::instance().external_file_name(tablespace_name);
      if (!external_file_name.empty()) {
        /* This is external tablespace. Copy it to it's original
        location */
        dst_path = external_file_name;
      }
    }

    if (!(ret = copy_or_move_file(entry.path.c_str(), dst_path.c_str(),
                                  mysql_data_home, ctx->n_thread,
                                  file_purpose))) {
      goto cleanup;
    }
  }

cleanup:
  my_thread_end();

  mutex_enter(ctx->count_mutex);
  --(*ctx->count);
  mutex_exit(ctx->count_mutex);

  ctx->ret = ret;
}

static bool get_one_option(int optid MY_ATTRIBUTE((unused)),
                           const struct my_option *opt MY_ATTRIBUTE((unused)),
                           char *argument MY_ATTRIBUTE((unused))) {
  return 0;
}

static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

static bool load_backup_my_cnf() {
  const char *groups[] = {"mysqld", NULL};

  my_option bakcup_options[] = {
      {"innodb_checksum_algorithm", 0, "", &srv_checksum_algorithm,
       &srv_checksum_algorithm, &innodb_checksum_algorithm_typelib, GET_ENUM,
       REQUIRED_ARG, SRV_CHECKSUM_ALGORITHM_INNODB, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

  char *exename = (char *)"xtrabackup";
  char **backup_my_argv = &exename;
  int backup_my_argc = 1;
  char fname[FN_REFLEN];

  /* we need full name so that only backup-my.cnf will be read */
  if (fn_format(fname, "backup-my.cnf", xtrabackup_target_dir, "",
                MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
    return (false);
  }

  if (my_load_defaults(fname, groups, &backup_my_argc, &backup_my_argv,
                       &argv_alloc, NULL)) {
    return (false);
  }

  if (handle_options(&backup_my_argc, &backup_my_argv, bakcup_options,
                     get_one_option)) {
    return (false);
  }

  return (true);
}

bool copy_back(int argc, char **argv) {
  char *innobase_data_file_path_copy;
  bool ret = true, err;
  char *dst_dir;
  binlog_file_location binlog;

  ut_crc32_init();

  if (!opt_force_non_empty_dirs) {
    if (!directory_exists_and_empty(mysql_data_home, "Original data")) {
      return (false);
    }
  } else {
    if (!directory_exists(mysql_data_home, true)) {
      return (false);
    }
  }
  if (srv_undo_dir && *srv_undo_dir && !directory_exists(srv_undo_dir, true)) {
    return (false);
  }
  if (innobase_data_home_dir && *innobase_data_home_dir &&
      !directory_exists(innobase_data_home_dir, true)) {
    return (false);
  }
  if (srv_log_group_home_dir && *srv_log_group_home_dir &&
      !directory_exists(srv_log_group_home_dir, true)) {
    return (false);
  }

  /* cd to backup directory */
  if (my_setwd(xtrabackup_target_dir, MYF(MY_WME))) {
    msg("cannot my_setwd %s\n", xtrabackup_target_dir);
    return (false);
  }

  if (!load_backup_my_cnf()) {
    msg("xtrabackup: Error: failed to load backup-my.cnf\n");
    return (false);
  }

  if (!Tablespace_map::instance().deserialize("./")) {
    msg("xtrabackup: Error: failed to load tablespaces list.\nIt is possible "
        "that the backup was created by Percona XtraBackup 2.4 or earlier "
        "version. Please use the same XtraBackup version to restore.\n");
    return (false);
  }

  if (opt_generate_new_master_key && !xb_tablespace_keys_exist()) {
    msg("xtrabackup: Error: option --generate_new_master_key "
        "is specified but xtrabackup_keys is absent.\n");
    return (false);
  }

  if (xb_tablespace_keys_exist() && opt_generate_new_master_key) {
    FILE *f = fopen("xtrabackup_master_key_id", "r");
    if (f == NULL) {
      msg("xtrabackup: Error: can't read master_key_id\n");
      return (false);
    }
    int ret = fscanf(f, "%lu", &Encryption::s_master_key_id);
    ut_a(ret == 1);
    fclose(f);

    if (!xb_keyring_init_for_copy_back(argc, argv)) {
      msg("xtrabackup: Error: failed to init keyring plugin\n");
      return (false);
    }
    if (!xb_tablespace_keys_load(
            "./", opt_transition_key,
            opt_transition_key != NULL ? strlen(opt_transition_key) : 0)) {
      msg("xtrabackup: Error: failed to load tablespace keys\n");
      return (false);
    }

    byte *master_key = NULL;

    Encryption::create_master_key(&master_key);

    if (master_key == NULL) {
      msg("xtrabackup: Error: can't generate new master "
          "key. Please check keyring plugin settings.\n");
      return (false);
    }

    my_free(master_key);

    ulint master_key_id;
    Encryption::get_master_key(&master_key_id, &master_key);

    msg_ts("Generated new master key with ID '%s-%lu'.\n", server_uuid,
           master_key_id);

    my_free(master_key);
  }

  /* parse data file path */

  if (!innobase_data_file_path) {
    innobase_data_file_path = (char *)"ibdata1:10M:autoextend";
  }
  innobase_data_file_path_copy = strdup(innobase_data_file_path);

  srv_sys_space.set_path(".");

  if (!srv_sys_space.parse_params(innobase_data_file_path, true, false)) {
    msg("syntax error in innodb_data_file_path\n");
    return (false);
  }

  /* temporally dummy value to avoid crash */
  srv_page_size_shift = 14;
  srv_page_size = (1 << srv_page_size_shift);
  srv_max_n_threads = 1000;
  sync_check_init(srv_max_n_threads);
  ut_crc32_init();

  /* copy undo tablespaces */
  if (srv_undo_tablespaces > 0) {
    dst_dir = (srv_undo_dir && *srv_undo_dir) ? srv_undo_dir : mysql_data_home;

    ds_data = ds_create(dst_dir, DS_TYPE_LOCAL);

    for (ulong i = 1; i <= srv_undo_tablespaces; i++) {
      char filename[20];
      sprintf(filename, "undo_%03lu", i);
      if (Fil_path::get_file_type(filename) != OS_FILE_TYPE_FILE) continue;
      if (!(ret = copy_or_move_file(filename, filename, dst_dir, 1,
                                    FILE_PURPOSE_UNDO_LOG))) {
        goto cleanup;
      }
    }

    ds_destroy(ds_data);
    ds_data = NULL;
  }

  /* copy redo logs */

  dst_dir = (srv_log_group_home_dir && *srv_log_group_home_dir)
                ? srv_log_group_home_dir
                : mysql_data_home;

  ds_data = ds_create(dst_dir, DS_TYPE_LOCAL);

  for (long i = 0; i < innobase_log_files_in_group; i++) {
    char filename[30];
    sprintf(filename, "ib_logfile%ld", i);

    if (!file_exists(filename)) {
      continue;
    }

    if (!(ret = copy_or_move_file(filename, filename, dst_dir, 1,
                                  FILE_PURPOSE_REDO_LOG))) {
      goto cleanup;
    }
  }

  ds_destroy(ds_data);
  ds_data = NULL;

  /* copy innodb system tablespace(s) */

  dst_dir = (innobase_data_home_dir && *innobase_data_home_dir)
                ? innobase_data_home_dir
                : mysql_data_home;

  ds_data = ds_create(dst_dir, DS_TYPE_LOCAL);

  for (auto iter(srv_sys_space.files_begin()), end(srv_sys_space.files_end());
       iter != end; ++iter) {
    const char *filename = base_name(iter->name());

    if (!(ret = copy_or_move_file(filename, iter->name(), dst_dir, 1,
                                  FILE_PURPOSE_DATAFILE))) {
      goto cleanup;
    }
  }

  ds_destroy(ds_data);
  ds_data = NULL;

  /* copy the rest of tablespaces */
  ds_data = ds_create(mysql_data_home, DS_TYPE_LOCAL);

  /* copy binary log and .index files */
  if (binlog_file_location::find_binlog(".", binlog, err)) {
    const auto target = binlog.target_location(mysql_data_home);

    if (!target.name.empty()) {
      if (!(ret = copy_or_move_file(binlog.name.c_str(), target.path.c_str(),
                                    mysql_data_home, 1, FILE_PURPOSE_BINLOG))) {
        goto cleanup;
      }
      ret = xb_binlog_password_reencrypt(target.path.c_str());
      if (!ret) {
        msg("xtrabackup: Error: failed to reencrypt binary log file "
            "header.\n");
      }
      /* make sure we don't copy binary log and .index files twice */
      skip_copy_back_list.insert(binlog.name.c_str());
    }
    if (!target.index_name.empty()) {
      if (!(ret = copy_or_move_file(binlog.index_name.c_str(),
                                    target.index_path.c_str(), mysql_data_home,
                                    1, FILE_PURPOSE_BINLOG))) {
        goto cleanup;
      }
      /* make sure we don't copy binary log and .index files twice */
      skip_copy_back_list.insert(binlog.index_name.c_str());
      /* fixup binlog index */
      if (Fil_path(mysql_data_home).is_ancestor(target.path)) {
        std::ofstream f_index(target.index_path.c_str());
        f_index << target.name.c_str() << std::endl;
      } else {
        std::ofstream f_index(target.index_path.c_str());
        f_index << target.path.c_str() << std::endl;
      }
    }
  }

  if (err || !ret) goto cleanup;

  ut_a(xtrabackup_parallel >= 0);
  if (xtrabackup_parallel > 1) {
    msg("xtrabackup: Starting %u threads for parallel data files transfer\n",
        xtrabackup_parallel);
  }

  ret = run_data_threads(".", copy_back_thread_func, xtrabackup_parallel,
                         "copy-back");
  if (!ret) goto cleanup;

  /* copy buufer pool dump */
  if (innobase_buffer_pool_filename) {
    const char *src_name;
    char path[FN_REFLEN];

    src_name = trim_dotslash(innobase_buffer_pool_filename);

    snprintf(path, sizeof(path), "%s/%s", mysql_data_home, src_name);

    /* could be already copied with other files from data directory */
    if (file_exists(src_name) && !file_exists(innobase_buffer_pool_filename)) {
      ret = copy_or_move_file(src_name, innobase_buffer_pool_filename,
                              mysql_data_home, 0, FILE_PURPOSE_OTHER);
      if (!ret) goto cleanup;
    }
  }

  ds_destroy(ds_data);
  ds_data = NULL;

  /* copy rocksdb datadir */
  if (directory_exists(ROCKSDB_SUBDIR, false)) {
    Myrocks_datadir rocksdb(ROCKSDB_SUBDIR);

    std::string rocksdb_datadir =
        (opt_rocksdb_datadir != nullptr && *opt_rocksdb_datadir != 0)
            ? opt_rocksdb_datadir
            : std::string(mysql_data_home) + FN_DIRSEP ROCKSDB_SUBDIR;

    std::string rocksdb_wal_dir =
        (opt_rocksdb_wal_dir != nullptr && *opt_rocksdb_wal_dir != 0)
            ? opt_rocksdb_wal_dir
            : "";

    if (!directory_exists(rocksdb_datadir.c_str(), true)) {
      return (false);
    }

    ds_data = ds_create(rocksdb_datadir.c_str(), DS_TYPE_LOCAL);

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    std::function<void(const Myrocks_datadir::const_iterator &,
                       const Myrocks_datadir::const_iterator &, size_t)>
        copy = std::bind(&par_copy_rocksdb_files, _1, _2, _3, ds_data, &ret);

    if (rocksdb_wal_dir.empty()) {
      par_for(PFS_NOT_INSTRUMENTED, rocksdb.files("", ""), xtrabackup_parallel,
              copy);
    } else {
      par_for(PFS_NOT_INSTRUMENTED, rocksdb.data_files(""), xtrabackup_parallel,
              copy);
    }

    if (!ret) goto cleanup;

    ds_destroy(ds_data);
    ds_data = nullptr;

    if (!rocksdb_wal_dir.empty()) {
      if (!Fil_path::is_absolute_path(rocksdb_wal_dir)) {
        rocksdb_wal_dir =
            std::string(mysql_data_home) + FN_DIRSEP + rocksdb_wal_dir;
      }

      if (!directory_exists(rocksdb_wal_dir.c_str(), true)) {
        return (false);
      }

      ds_data = ds_create(rocksdb_wal_dir.c_str(), DS_TYPE_LOCAL);

      using std::placeholders::_1;
      using std::placeholders::_2;
      using std::placeholders::_3;
      std::function<void(const Myrocks_datadir::const_iterator &,
                         const Myrocks_datadir::const_iterator &, size_t)>
          copy = std::bind(&par_copy_rocksdb_files, _1, _2, _3, ds_data, &ret);

      par_for(PFS_NOT_INSTRUMENTED, rocksdb.wal_files(""), xtrabackup_parallel,
              copy);

      if (!ret) goto cleanup;

      ds_destroy(ds_data);
      ds_data = nullptr;
    }
  }

cleanup:

  free(innobase_data_file_path_copy);

  if (ds_data != NULL) {
    ds_destroy(ds_data);
  }

  ds_data = NULL;

  xb_keyring_shutdown();

  sync_check_close();

  return (ret);
}

bool decrypt_decompress_file(const char *filepath, uint thread_n) {
  std::stringstream cmd, message;
  char *dest_filepath = strdup(filepath);
  bool needs_action = false;

  cmd << "cat " << filepath;

  if (ends_with(filepath, ".xbcrypt") && opt_decrypt) {
    cmd << " | xbcrypt --decrypt --encrypt-algo="
        << xtrabackup_encrypt_algo_names[opt_decrypt_algo];
    if (xtrabackup_encrypt_key) {
      cmd << " --encrypt-key=" << xtrabackup_encrypt_key;
    } else {
      cmd << " --encrypt-key-file=" << xtrabackup_encrypt_key_file;
    }
    dest_filepath[strlen(dest_filepath) - 8] = 0;
    message << "decrypting";
    needs_action = true;
  }

  if (opt_decompress && (ends_with(filepath, ".qp") ||
                         (ends_with(filepath, ".qp.xbcrypt") && opt_decrypt))) {
    cmd << " | qpress -dio ";
    dest_filepath[strlen(dest_filepath) - 3] = 0;
    if (needs_action) {
      message << " and ";
    }
    message << "decompressing";
    needs_action = true;
  }

  if (opt_decompress &&
      (ends_with(filepath, ".lz4") ||
       (ends_with(filepath, ".lz4.xbcrypt") && opt_decrypt))) {
    cmd << " | lz4 -d ";
    dest_filepath[strlen(dest_filepath) - 4] = 0;
    if (needs_action) {
      message << " and ";
    }
    message << "decompressing";
    needs_action = true;
  }

  cmd << " > " << dest_filepath;
  message << " " << filepath;

  free(dest_filepath);

  if (needs_action) {
    msg_ts("[%02u] %s\n", thread_n, message.str().c_str());

    if (system(cmd.str().c_str()) != 0) {
      return (false);
    }

    if (opt_remove_original) {
      msg_ts("[%02u] removing %s\n", thread_n, filepath);
      if (my_delete(filepath, MYF(MY_WME)) != 0) {
        return (false);
      }
    }
  }

  return (true);
}

static void decrypt_decompress_thread_func(datadir_thread_ctxt_t *ctxt) {
  bool ret = true;
  datadir_entry_t entry;

  while (ctxt->queue->pop(entry)) {
    if (entry.is_empty_dir) {
      continue;
    }

    if (!ends_with(entry.path.c_str(), ".qp") &&
        !ends_with(entry.path.c_str(), ".xbcrypt") &&
        !ends_with(entry.path.c_str(), ".lz4")) {
      continue;
    }

    if (!(ret = decrypt_decompress_file(entry.path.c_str(), ctxt->n_thread))) {
      goto cleanup;
    }
  }

cleanup:

  mutex_enter(ctxt->count_mutex);
  --(*ctxt->count);
  mutex_exit(ctxt->count_mutex);

  ctxt->ret = ret;
}

bool decrypt_decompress() {
  bool ret;

  srv_max_n_threads = 1000;
  sync_check_init(srv_max_n_threads);

  /* cd to backup directory */
  if (my_setwd(xtrabackup_target_dir, MYF(MY_WME))) {
    msg("cannot my_setwd %s\n", xtrabackup_target_dir);
    return (false);
  }

  /* copy the rest of tablespaces */
  ds_data = ds_create(".", DS_TYPE_LOCAL);

  ut_a(xtrabackup_parallel >= 0);

  ret = run_data_threads(".", decrypt_decompress_thread_func,
                         xtrabackup_parallel, "decrypt and decompress");

  if (ds_data != NULL) {
    ds_destroy(ds_data);
  }

  ds_data = NULL;

  sync_check_close();

  return (ret);
}

#ifdef HAVE_VERSION_CHECK
void version_check() {
  if (opt_password != NULL) {
    setenv("option_mysql_password", opt_password, 1);
  }
  if (opt_user != NULL) {
    setenv("option_mysql_user", opt_user, 1);
  }
  if (opt_host != NULL) {
    setenv("option_mysql_host", opt_host, 1);
  }
  if (opt_socket != NULL) {
    setenv("option_mysql_socket", opt_socket, 1);
  }
  if (opt_port != 0) {
    char port[20];
    snprintf(port, sizeof(port), "%u", opt_port);
    setenv("option_mysql_port", port, 1);
  }
  setenv("XTRABACKUP_VERSION", XTRABACKUP_VERSION, 1);

  FILE *pipe = popen("perl", "w");
  if (pipe == NULL) {
    return;
  }

  fwrite((const char *)version_check_pl, version_check_pl_len, 1, pipe);

  pclose(pipe);
}
#endif
