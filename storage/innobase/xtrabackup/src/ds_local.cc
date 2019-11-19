/******************************************************
Copyright (c) 2011-2019 Percona LLC and/or its affiliates.

Local datasink implementation for XtraBackup.

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

#include <my_base.h>
#include <my_io.h>
#include <my_sys.h>
#include <my_thread_local.h>
#include <mysql/service_mysql_alloc.h>
#include <mysql_version.h>
#include <mysys_err.h>
#include "common.h"
#include "datasink.h"

typedef struct {
  File fd;
  size_t last_seek;  // to track last page sparse_file
} ds_local_file_t;

static ds_ctxt_t *local_init(const char *root);
static ds_file_t *local_open(ds_ctxt_t *ctxt, const char *path,
                             MY_STAT *mystat);
static int local_write(ds_file_t *file, const void *buf, size_t len);
static int local_write_sparse(ds_file_t *file, const void *buf, size_t len,
                              size_t sparse_map_size,
                              const ds_sparse_chunk_t *sparse_map);
static int local_close(ds_file_t *file);
static void local_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_local = {&local_init,         &local_open,  &local_write,
                             &local_write_sparse, &local_close, &local_deinit};

static ds_ctxt_t *local_init(const char *root) {
  ds_ctxt_t *ctxt;

  if (my_mkdir(root, 0777, MYF(0)) < 0 && my_errno() != EEXIST &&
      my_errno() != EISDIR) {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(EE_CANT_MKDIR, MYF(ME_BELL), root, my_errno(),
             my_strerror(errbuf, sizeof(errbuf), my_errno()));
    return NULL;
  }

  ctxt = static_cast<ds_ctxt_t *>(
      my_malloc(PSI_NOT_INSTRUMENTED, sizeof(ds_ctxt_t), MYF(MY_FAE)));

  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *local_open(ds_ctxt_t *ctxt, const char *path,
                             MY_STAT *mystat __attribute__((unused))) {
  char fullpath[FN_REFLEN];
  char dirpath[FN_REFLEN];
  size_t dirpath_len;
  size_t path_len;
  ds_local_file_t *local_file;
  ds_file_t *file;
  File fd;

  fn_format(fullpath, path, ctxt->root, "", MYF(MY_RELATIVE_PATH));

  /* Create the directory if needed */
  dirname_part(dirpath, fullpath, &dirpath_len);
  if (my_mkdir(dirpath, 0777, MYF(0)) < 0 && my_errno() != EEXIST) {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(EE_CANT_MKDIR, MYF(ME_BELL), dirpath, my_errno(),
             my_strerror(errbuf, sizeof(errbuf), my_errno()));
    return NULL;
  }

  fd = my_create(fullpath, 0, O_WRONLY | O_EXCL | O_NOFOLLOW, MYF(MY_WME));
  if (fd < 0) {
    return NULL;
  }

  path_len = strlen(fullpath) + 1; /* terminating '\0' */

  file = (ds_file_t *)my_malloc(
      PSI_NOT_INSTRUMENTED,
      sizeof(ds_file_t) + sizeof(ds_local_file_t) + path_len, MYF(MY_FAE));
  local_file = (ds_local_file_t *)(file + 1);

  local_file->fd = fd;
  local_file->last_seek = 0;

  file->path = (char *)local_file + sizeof(ds_local_file_t);
  memcpy(file->path, fullpath, path_len);

  file->ptr = local_file;

  return file;
}

static int local_write(ds_file_t *file, const void *buf, size_t len) {
  auto local_file = ((ds_local_file_t *)file->ptr);
  File fd = local_file->fd;
  local_file->last_seek = 0;

  if (!my_write(fd, static_cast<const uchar *>(buf), len,
                MYF(MY_WME | MY_NABP))) {
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    return 0;
  }

  return 1;
}

static int local_write_sparse(ds_file_t *file, const void *buf, size_t len,
                              size_t sparse_map_size,
                              const ds_sparse_chunk_t *sparse_map) {
  auto local_file = ((ds_local_file_t *)file->ptr);
  File fd = local_file->fd;

  const uchar *ptr = static_cast<const uchar *>(buf);

  for (size_t i = 0; i < sparse_map_size; ++i) {
    size_t rc;

    rc = my_seek(fd, sparse_map[i].skip, MY_SEEK_CUR, MYF(MY_WME));
    if (rc == MY_FILEPOS_ERROR) {
      return 1;
    }

    rc = my_write(fd, ptr, sparse_map[i].len, MYF(MY_WME | MY_NABP));
    if (rc != 0) {
      return 1;
    }

    ptr += sparse_map[i].len;
  }
  /* to track if last page is sparse */
  if (sparse_map[sparse_map_size - 1].len == 0) {
    local_file->last_seek = sparse_map[sparse_map_size - 1].skip;
  } else
    local_file->last_seek = 0;

  return 0;
}

static int local_close(ds_file_t *file) {
  auto local_file = ((ds_local_file_t *)file->ptr);
  File fd = local_file->fd;

  /* Write the last page complete in full size. We achieve this by writing the
   * last byte of page as zero, this can only happen in case of sparse file */
  if (local_file->last_seek > 0) {
    size_t rc;
    rc = my_seek(fd, -1, MY_SEEK_CUR, MYF(MY_WME));
    if (rc == MY_FILEPOS_ERROR) {
      return 1;
    }
    unsigned char b = 0;
    rc = my_write(fd, &b, 1, MYF(MY_WME | MY_NABP));
    if (rc != 0) {
      return 1;
    }
  }

  my_free(file);

  my_sync(fd, MYF(MY_WME));

  return my_close(fd, MYF(MY_WME));
}

static void local_deinit(ds_ctxt_t *ctxt) {
  my_free(ctxt->root);
  my_free(ctxt);
}
