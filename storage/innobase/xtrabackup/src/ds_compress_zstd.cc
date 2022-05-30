/******************************************************
Copyright (c) 2022 Percona LLC and/or its affiliates.

Compressing datasink implementation using ZSTD for XtraBackup.

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
#define ZSTD_STATIC_LINKING_ONLY  // Thread pool
#include <zstd.h>
#include "common.h"
#include "datasink.h"
#include "msg.h"

typedef struct {
  ZSTD_threadPool *thread_pool;
} ds_compress_ctxt_t;

typedef struct {
  ds_file_t *dest_file;
  char *comp_buf;
  size_t comp_buf_size;
  size_t raw_bytes;
  size_t comp_bytes;
  ZSTD_CCtx *cctx;
} ds_compress_file_t;

/* Compression options */
extern char *xtrabackup_compress_alg;
extern uint xtrabackup_compress_threads;
extern uint xtrabackup_compress_zstd_level;

static ds_ctxt_t *compress_init(const char *root);
static ds_file_t *compress_open(ds_ctxt_t *ctxt, const char *path,
                                MY_STAT *mystat);
static int compress_write(ds_file_t *file, const void *buf, size_t len);
static int compress_close(ds_file_t *file);
static void compress_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_compress_zstd = {&compress_init,  &compress_open,
                                     &compress_write, nullptr,
                                     &compress_close, &compress_deinit};

static ds_ctxt_t *compress_init(const char *root) {
  ds_compress_ctxt_t *compress_ctxt = new ds_compress_ctxt_t;
  compress_ctxt->thread_pool =
      ZSTD_createThreadPool(xtrabackup_compress_threads);

  ds_ctxt_t *ctxt = new ds_ctxt_t;
  ctxt->ptr = compress_ctxt;
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *compress_open(ds_ctxt_t *ctxt, const char *path,
                                MY_STAT *mystat) {
  char new_name[FN_REFLEN];

  xb_ad(ctxt->pipe_ctxt != nullptr);
  ds_ctxt_t *dest_ctxt = ctxt->pipe_ctxt;

  ds_compress_ctxt_t *comp_ctxt = (ds_compress_ctxt_t *)ctxt->ptr;

  /* Append the .zst extension to the filename */
  fn_format(new_name, path, "", ".zst", MYF(MY_APPEND_EXT));

  ds_file_t *dest_file = ds_open(dest_ctxt, new_name, mystat);
  if (dest_file == nullptr) {
    return nullptr;
  }

  ds_compress_file_t *comp_file = new ds_compress_file_t;
  comp_file->dest_file = dest_file;
  comp_file->comp_buf = nullptr;
  comp_file->comp_buf_size = 0;
  comp_file->raw_bytes = 0;
  comp_file->comp_bytes = 0;
  ZSTD_CCtx *cctx = ZSTD_createCCtx();
  ZSTD_CCtx_refThreadPool(cctx, comp_ctxt->thread_pool);
  ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel,
                         xtrabackup_compress_zstd_level);
  ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, xtrabackup_compress_threads);
  ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

  comp_file->cctx = cctx;

  ds_file_t *file = new ds_file_t;
  file->ptr = comp_file;
  file->path = dest_file->path;

  return file;
}

static int compress_write(ds_file_t *file, const void *buf, size_t len) {
  ds_compress_file_t *comp_file = (ds_compress_file_t *)file->ptr;
  ds_file_t *dest_file = comp_file->dest_file;

  /* make sure we have enough memory for compression */
  const size_t comp_size = ZSTD_CStreamOutSize();
  size_t n_chunks = (len / comp_size * comp_size == len)
                        ? (len / comp_size)
                        : (len / comp_size + 1);
  /* empty file */
  if (n_chunks == 0) n_chunks = 1;
  const size_t comp_buf_size = comp_size * n_chunks;
  if (comp_file->comp_buf_size < comp_buf_size) {
    comp_file->comp_buf = static_cast<char *>(
        my_realloc(PSI_NOT_INSTRUMENTED, comp_file->comp_buf, comp_buf_size,
                   MYF(MY_FAE | MY_ALLOW_ZERO_PTR)));
  }

  size_t read_chunk_size = ZSTD_CStreamInSize();
  if (read_chunk_size > len) read_chunk_size = len;
  bool last_chunk = false;
  while (1) {
    if (len - comp_file->raw_bytes <= read_chunk_size) {
      read_chunk_size = len - comp_file->raw_bytes;
      last_chunk = true;
    }
    if (comp_file->comp_bytes > 0 &&
        comp_file->comp_bytes + comp_size > comp_buf_size) {
      /* buffer full */
      if (ds_write(dest_file, comp_file->comp_buf, comp_file->comp_bytes)) {
        goto err;
      }
      comp_file->comp_bytes = 0;
    }
    ZSTD_EndDirective const mode = last_chunk ? ZSTD_e_end : ZSTD_e_continue;
    ZSTD_inBuffer input = {((const char *)buf + comp_file->raw_bytes),
                           read_chunk_size, 0};
    bool finished = false;
    ZSTD_outBuffer output = {comp_file->comp_buf + comp_file->comp_bytes,
                             comp_size, 0};
    size_t const remaining =
        ZSTD_compressStream2(comp_file->cctx, &output, &input, mode);
    comp_file->raw_bytes += input.pos;
    comp_file->comp_bytes += output.pos;
    finished = last_chunk ? (remaining == 0) : false;

    if (last_chunk && finished) {
      break;
    }
  }
  /* last write */
  if (ds_write(dest_file, comp_file->comp_buf, comp_file->comp_bytes)) {
    goto err;
  }
  comp_file->raw_bytes = 0;
  comp_file->comp_bytes = 0;
  return 0;

err:
  msg("compress: write to the destination stream failed.\n");
  return 1;
}

static int compress_close(ds_file_t *file) {
  ds_compress_file_t *comp_file = (ds_compress_file_t *)file->ptr;
  ds_file_t *dest_file = comp_file->dest_file;

  ZSTD_freeCCtx(comp_file->cctx);

  int rc = ds_close(dest_file);

  my_free(comp_file->comp_buf);
  delete file;
  delete comp_file;

  return rc;
}

static void compress_deinit(ds_ctxt_t *ctxt) {
  xb_ad(ctxt->pipe_ctxt != nullptr);

  ds_compress_ctxt_t *comp_ctxt = (ds_compress_ctxt_t *)ctxt->ptr;

  ZSTD_freeThreadPool(comp_ctxt->thread_pool);
  delete comp_ctxt;

  my_free(ctxt->root);
  delete ctxt;
}
