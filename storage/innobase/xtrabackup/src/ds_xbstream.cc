/******************************************************
Copyright (c) 2011-2023 Percona LLC and/or its affiliates.

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

#include <my_base.h>
#include <my_io.h>
#include <mysql/service_mysql_alloc.h>
#include <mysql_version.h>
#include <list>
#include <mutex>
#include "common.h"
#include "datasink.h"
#include "msg.h"
#include "xbstream.h"

typedef struct {
  xb_wstream_t *xbstream;
  ds_file_t *dest_file;
  std::mutex mutex;
} ds_stream_ctxt_t;

typedef struct {
  std::list<ds_stream_ctxt_t *> ctx_list;
  std::mutex mutex;
} ds_parallel_stream_ctxt_t;

typedef struct {
  xb_wstream_file_t *xbstream_file;
  ds_stream_ctxt_t *stream_ctxt;
} ds_stream_file_t;

extern uint xtrabackup_fifo_streams;
/***********************************************************************
General streaming interface */

static ds_ctxt_t *xbstream_init(const char *root);
static ds_file_t *xbstream_open(ds_ctxt_t *ctxt, const char *path,
                                MY_STAT *mystat);
static int xbstream_write(ds_file_t *file, const void *buf, size_t len);
static int xbstream_write_sparse(ds_file_t *file, const void *buf, size_t len,
                                 size_t sparse_map_size,
                                 const ds_sparse_chunk_t *sparse_map,
                                 bool punch_hole_supported);
static int xbstream_close(ds_file_t *file);
static void xbstream_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_xbstream = {&xbstream_init,  &xbstream_open,
                                &xbstream_write, &xbstream_write_sparse,
                                &xbstream_close, &xbstream_deinit};

static ssize_t my_xbstream_write_callback(xb_wstream_file_t *f
                                          __attribute__((unused)),
                                          void *userdata, const void *buf,
                                          size_t len) {
  ds_stream_ctxt_t *stream_ctxt;

  stream_ctxt = (ds_stream_ctxt_t *)userdata;

  xb_ad(stream_ctxt != NULL);
  xb_ad(stream_ctxt->dest_file != NULL);

  if (!ds_write(stream_ctxt->dest_file, buf, len)) {
    return len;
  }
  return -1;
}

static ds_ctxt_t *xbstream_init(const char *root __attribute__((unused))) {
  ds_ctxt_t *ctxt;
  ds_parallel_stream_ctxt_t *parallel_stream_ctxt =
      new ds_parallel_stream_ctxt_t;

  ctxt = static_cast<ds_ctxt_t *>(
      my_malloc(PSI_NOT_INSTRUMENTED,
                sizeof(ds_ctxt_t) + sizeof(ds_parallel_stream_ctxt_t) +
                    (sizeof(ds_stream_ctxt_t) * (xtrabackup_fifo_streams + 1)),
                MYF(MY_FAE)));

  for (uint i = 0; i < xtrabackup_fifo_streams; i++) {
    ds_stream_ctxt_t *stream_ctxt = new ds_stream_ctxt_t;
    xb_wstream_t *xbstream;
    xbstream = xb_stream_write_new();
    if (xbstream == NULL) {
      msg("xb_stream_write_new() failed.\n");
      goto err;
    }
    stream_ctxt->xbstream = xbstream;
    stream_ctxt->dest_file = NULL;
    parallel_stream_ctxt->ctx_list.push_back(stream_ctxt);
  }

  ctxt->ptr = parallel_stream_ctxt;

  return ctxt;

err:
  my_free(ctxt);
  return NULL;
}

static ds_file_t *xbstream_open(ds_ctxt_t *ctxt, const char *path,
                                MY_STAT *mystat) {
  ds_file_t *file;
  ds_parallel_stream_ctxt_t *parallel_stream_ctxt;
  ds_stream_file_t *stream_file;
  ds_stream_ctxt_t *stream_ctxt;
  ds_ctxt_t *dest_ctxt;
  xb_wstream_t *xbstream;
  xb_wstream_file_t *xbstream_file;

  xb_ad(ctxt->pipe_ctxt != NULL);
  dest_ctxt = ctxt->pipe_ctxt;

  parallel_stream_ctxt = (ds_parallel_stream_ctxt_t *)ctxt->ptr;
  parallel_stream_ctxt->mutex.lock();
  stream_ctxt = parallel_stream_ctxt->ctx_list.front();
  parallel_stream_ctxt->ctx_list.pop_front();
  parallel_stream_ctxt->ctx_list.push_back(stream_ctxt);
  parallel_stream_ctxt->mutex.unlock();

  stream_ctxt->mutex.lock();
  if (stream_ctxt->dest_file == NULL) {
    stream_ctxt->dest_file = ds_open(dest_ctxt, path, mystat);
    if (stream_ctxt->dest_file == NULL) {
      return NULL;
    }
  }
  stream_ctxt->mutex.unlock();

  file = (ds_file_t *)my_malloc(PSI_NOT_INSTRUMENTED,
                                sizeof(ds_file_t) + sizeof(ds_stream_file_t),
                                MYF(MY_FAE));
  stream_file = (ds_stream_file_t *)(file + 1);

  xbstream = stream_ctxt->xbstream;

  xbstream_file = xb_stream_write_open(xbstream, path, mystat, stream_ctxt,
                                       my_xbstream_write_callback);

  if (xbstream_file == NULL) {
    msg("xb_stream_write_open() failed.\n");
    goto err;
  }

  stream_file->xbstream_file = xbstream_file;
  stream_file->stream_ctxt = stream_ctxt;
  file->ptr = stream_file;
  file->path = stream_ctxt->dest_file->path;

  return file;

err:
  if (stream_ctxt->dest_file) {
    ds_close(stream_ctxt->dest_file);
    stream_ctxt->dest_file = NULL;
  }
  my_free(file);

  return NULL;
}

static int xbstream_write(ds_file_t *file, const void *buf, size_t len) {
  ds_stream_file_t *stream_file;
  xb_wstream_file_t *xbstream_file;

  stream_file = (ds_stream_file_t *)file->ptr;

  xbstream_file = stream_file->xbstream_file;

  if (xb_stream_write_data(xbstream_file, buf, len)) {
    msg("xb_stream_write_data() failed.\n");
    return 1;
  }

  return 0;
}

static int xbstream_write_sparse(ds_file_t *file, const void *buf, size_t len,
                                 size_t sparse_map_size,
                                 const ds_sparse_chunk_t *sparse_map,
                                 bool punch_hole_supported) {
  ds_stream_file_t *stream_file;
  xb_wstream_file_t *xbstream_file;

  stream_file = (ds_stream_file_t *)file->ptr;

  xbstream_file = stream_file->xbstream_file;

  if (xb_stream_write_sparse_data(xbstream_file, buf, len, sparse_map_size,
                                  sparse_map)) {
    msg("xb_stream_write_sparse_data() failed.\n");
    return 1;
  }

  return 0;
}

static int xbstream_close(ds_file_t *file) {
  ds_stream_file_t *stream_file;
  int rc = 0;

  stream_file = (ds_stream_file_t *)file->ptr;

  rc = xb_stream_write_close(stream_file->xbstream_file);

  my_free(file);

  return rc;
}

static void xbstream_deinit(ds_ctxt_t *ctxt) {
  ds_parallel_stream_ctxt_t *parallel_stream_ctxt =
      (ds_parallel_stream_ctxt_t *)ctxt->ptr;
  ds_stream_ctxt_t *stream_ctxt;

  for (uint i = 0; i < xtrabackup_fifo_streams; i++) {
    stream_ctxt = parallel_stream_ctxt->ctx_list.front();
    parallel_stream_ctxt->ctx_list.pop_front();
    if (xb_stream_write_done(stream_ctxt->xbstream)) {
      msg("xb_stream_done() failed.\n");
    }

    if (stream_ctxt->dest_file) {
      ds_close(stream_ctxt->dest_file);
      stream_ctxt->dest_file = NULL;
    }
    delete stream_ctxt;
  }
  delete parallel_stream_ctxt;
  my_free(ctxt);
}
