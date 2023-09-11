/******************************************************
Copyright (c) 2023 Percona LLC and/or its affiliates.

FIFO datasink implementation for XtraBackup.

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
#include <my_thread_local.h>
#include <mysql/service_mysql_alloc.h>
#include <mysys_err.h>
#include <mutex>
#include <unordered_map>
#include "common.h"
#include "datasink.h"
#include "file_utils.h"
#include "msg.h"

struct ds_fifo_ctxt_t {
  /* List of FIFO files to be used for stream */
  std::unordered_map<std::string, File> FIFO_list;
  /* Mutex protecting FIFO list */
  std::mutex fifo_mutex;

  /* Add a new pair of fullpath and fd to FIFO_list */
  void populate_list(std::string fullpath, File fd) {
    std::lock_guard<std::mutex> g(fifo_mutex);
    FIFO_list.insert({fullpath, fd});
  }

  /**
  Get the first file from FIFO_list. This call will remove the
  file from the list.
  @return false in case of file found from list, true in case of error */
  bool allocate_from_list(std::string &fullpath, File &fd) {
    std::lock_guard<std::mutex> g(fifo_mutex);
    auto fifo_it = FIFO_list.begin();
    if (fifo_it == FIFO_list.end()) {
      return true;
    }
    fullpath = fifo_it->first;
    fd = fifo_it->second;
    FIFO_list.erase(fifo_it);
    return false;
  }
  /* Close all FIFO files in FIFO_list */
  void close_all() {
    std::lock_guard<std::mutex> g(fifo_mutex);
    for (auto &fifo : FIFO_list) {
      my_close(fifo.second, MYF(0));
    }
    FIFO_list.clear();
  }
};

typedef struct ds_fifo_ctxt_t ds_fifo_ctxt_t;

typedef struct {
  File fd;
  char *path;
  ds_fifo_ctxt_t *fifo_context;
} ds_fifo_file_t;

extern uint xtrabackup_fifo_streams;
extern uint xtrabackup_fifo_timeout;
static ds_ctxt_t *fifo_init(const char *root);
static ds_file_t *fifo_open(ds_ctxt_t *ctxt, const char *path, MY_STAT *mystat);
static int fifo_write(ds_file_t *file, const void *buf, size_t len);
static int fifo_close(ds_file_t *file);
static void fifo_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_fifo = {&fifo_init, &fifo_open,  &fifo_write,
                            nullptr,    &fifo_close, &fifo_deinit};

static void cleanup_on_error(const char *root, ds_fifo_ctxt_t *ctxt) {
  std::string path;
  char fullpath[FN_REFLEN];
  ctxt->close_all();
  for (uint i = 0; i < xtrabackup_fifo_streams; i++) {
    path = "thread_" + std::to_string(i);
    fn_format(fullpath, path.c_str(), root, "", MYF(MY_RELATIVE_PATH));
    unlink(fullpath);
  }
}

/**
Initialize FIFO datasink. This function is responsible for creating stream dir,
and the fifo files

@param [in]  root  path to create FIFO files.

@return ds_ctxt_t object with a ptr to ds_fifo_ctxt_t object.
NULL in case of error. */
static ds_ctxt_t *fifo_init(const char *root) {
  ds_fifo_ctxt_t *fifo_context = new ds_fifo_ctxt_t;
  File fd;
  ds_ctxt_t *ctxt = new ds_ctxt_t;
  char fullpath[FN_REFLEN];

  if (my_mkdir(root, 0600, MYF(0)) < 0 && my_errno() != EEXIST &&
      my_errno() != EISDIR) {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(EE_CANT_MKDIR, MYF(0), root, my_errno(),
             my_strerror(errbuf, sizeof(errbuf), my_errno()));
    return NULL;
  }

  for (uint i = 0; i < xtrabackup_fifo_streams; i++) {
    std::string path = "thread_" + std::to_string(i);
    fn_format(fullpath, path.c_str(), root, "", MYF(MY_RELATIVE_PATH));
    if (mkfifo(fullpath, 0600) < 0) {
      msg_ts("mkfifo(%s) failed with error %d\n", fullpath, errno);
      if (errno == EEXIST) {
        msg_ts(
            "FIFO file %s already exists. Please ensure you don't have other "
            "xtrabackup instance running, remove the file(s) and try again.\n",
            fullpath);
      } else {
        cleanup_on_error(root, fifo_context);
      }
      return NULL;
    }
  }

  for (uint i = 0; i < xtrabackup_fifo_streams; i++) {
    std::string path = "thread_" + std::to_string(i);
    fn_format(fullpath, path.c_str(), root, "", MYF(MY_RELATIVE_PATH));
    fd = open_fifo_for_write_with_timeout(fullpath, xtrabackup_fifo_timeout);
    if (fd < 0) {
      cleanup_on_error(root, fifo_context);
      return NULL;
    }
    fifo_context->populate_list(fullpath, fd);
  }

  ctxt->ptr = fifo_context;
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *fifo_open(ds_ctxt_t *ctxt,
                            const char *path __attribute__((unused)),
                            MY_STAT *mystat __attribute__((unused))) {
  ds_fifo_ctxt_t *fifo_context = (ds_fifo_ctxt_t *)ctxt->ptr;
  std::string fifo_path;
  File fd;
  if (fifo_context->allocate_from_list(fifo_path, fd)) return NULL;

  size_t path_len = fifo_path.length() + 1; /* terminating '\0' */

  ds_file_t *file = (ds_file_t *)my_malloc(
      PSI_NOT_INSTRUMENTED,
      sizeof(ds_file_t) + sizeof(ds_fifo_file_t) + path_len, MYF(MY_FAE));
  ds_fifo_file_t *fifo_file = (ds_fifo_file_t *)(file + 1);

  fifo_file->fd = fd;
  fifo_file->fifo_context = fifo_context;
  fifo_file->path = (char *)fifo_path.c_str();
  file->path = (char *)fifo_file + sizeof(ds_fifo_file_t);
  memcpy(file->path, fifo_path.c_str(), path_len);

  file->ptr = fifo_file;

  return file;
}

static int fifo_write(ds_file_t *file, const void *buf, size_t len) {
  File fd = ((ds_fifo_file_t *)file->ptr)->fd;

  if (!my_write(fd, static_cast<const uchar *>(buf), len,
                MYF(MY_WME | MY_NABP))) {
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    return 0;
  }

  return 1;
}

static int fifo_close(ds_file_t *file) {
  ds_fifo_ctxt_t *fifo_context = ((ds_fifo_file_t *)file->ptr)->fifo_context;
  File fd = ((ds_fifo_file_t *)file->ptr)->fd;
  my_close(fd, MYF(MY_WME));
  fifo_context->populate_list(((ds_fifo_file_t *)file)->path, fd);
  my_free(file);

  return 0;
}

static void fifo_deinit(ds_ctxt_t *ctxt) {
  ds_fifo_ctxt_t *fifo_context = (ds_fifo_ctxt_t *)ctxt->ptr;
  assert(fifo_context->FIFO_list.size() == xtrabackup_fifo_streams);
  delete fifo_context;
  my_free(ctxt->root);
  delete ctxt;
}
