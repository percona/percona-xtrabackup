/******************************************************
Copyright (c) 2019 Percona LLC and/or its affiliates.

Compressing datasink implementation using LZ4 for XtraBackup.

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

#include <lz4.h>
#include <my_base.h>
#include <my_byteorder.h>
#include <my_io.h>
#include <mysql/service_mysql_alloc.h>
#include <mysql_version.h>
#include "../extra/lz4/my_xxhash.h"
#include "common.h"
#include "datasink.h"
#include "thread_pool.h"

#define COMPRESS_CHUNK_SIZE ((size_t)(xtrabackup_compress_chunk_size))

#define LZ4F_MAGICNUMBER 0x184d2204U
#define LZ4F_UNCOMPRESSED_BIT (1U << 31)

typedef struct {
  const char *from;
  size_t from_len;
  char *to;
  size_t to_len;
  size_t to_size;
} comp_thread_ctxt_t;

typedef struct {
  Thread_pool *thread_pool;
} ds_compress_ctxt_t;

typedef struct {
  ds_file_t *dest_file;
  ds_compress_ctxt_t *comp_ctxt;
  size_t bytes_processed;
  char *comp_buf;
  size_t comp_buf_size;
  std::vector<std::future<void>> tasks;
  std::vector<comp_thread_ctxt_t> contexts;
} ds_compress_file_t;

/* Compression options */
extern char *xtrabackup_compress_alg;
extern uint xtrabackup_compress_threads;
extern ulonglong xtrabackup_compress_chunk_size;

static ds_ctxt_t *compress_init(const char *root);
static ds_file_t *compress_open(ds_ctxt_t *ctxt, const char *path,
                                MY_STAT *mystat);
static int compress_write(ds_file_t *file, const void *buf, size_t len);
static int compress_close(ds_file_t *file);
static void compress_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_compress_lz4 = {&compress_init,  &compress_open,
                                    &compress_write, nullptr,
                                    &compress_close, &compress_deinit};

static inline int write_uint32_le(ds_file_t *file, uint32_t n);

static ds_ctxt_t *compress_init(const char *root) {
  ds_compress_ctxt_t *compress_ctxt = new ds_compress_ctxt_t;
  compress_ctxt->thread_pool = new Thread_pool(xtrabackup_compress_threads);

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

  /* Append the .lz4 extension to the filename */
  fn_format(new_name, path, "", ".lz4", MYF(MY_APPEND_EXT));

  ds_file_t *dest_file = ds_open(dest_ctxt, new_name, mystat);
  if (dest_file == nullptr) {
    return nullptr;
  }

  ds_compress_file_t *comp_file = new ds_compress_file_t;
  comp_file->dest_file = dest_file;
  comp_file->comp_ctxt = comp_ctxt;
  comp_file->bytes_processed = 0;
  comp_file->comp_buf = nullptr;
  comp_file->comp_buf_size = 0;

  ds_file_t *file = new ds_file_t;
  file->ptr = comp_file;
  file->path = dest_file->path;

  return file;
}

static int compress_write(ds_file_t *file, const void *buf, size_t len) {
  ds_compress_file_t *comp_file = (ds_compress_file_t *)file->ptr;
  ds_compress_ctxt_t *comp_ctxt = comp_file->comp_ctxt;
  ds_file_t *dest_file = comp_file->dest_file;

  /* make sure we have enough memory for compression */
  const size_t comp_size = LZ4_compressBound(COMPRESS_CHUNK_SIZE);
  const size_t n_chunks =
      (len / COMPRESS_CHUNK_SIZE * COMPRESS_CHUNK_SIZE == len)
          ? (len / COMPRESS_CHUNK_SIZE)
          : (len / COMPRESS_CHUNK_SIZE + 1);
  const size_t comp_buf_size = comp_size * n_chunks;
  if (comp_file->comp_buf_size < comp_buf_size) {
    comp_file->comp_buf = static_cast<char *>(
        my_realloc(PSI_NOT_INSTRUMENTED, comp_file->comp_buf, comp_buf_size,
                   MYF(MY_FAE | MY_ALLOW_ZERO_PTR)));
  }

  /* parallel compress using trhead pool */
  if (comp_file->tasks.size() < n_chunks) {
    comp_file->tasks.resize(n_chunks);
  }
  if (comp_file->contexts.size() < n_chunks) {
    comp_file->contexts.resize(n_chunks);
  }

  for (size_t i = 0; i < n_chunks; i++) {
    size_t chunk_len =
        std::min(len - i * COMPRESS_CHUNK_SIZE, COMPRESS_CHUNK_SIZE);
    auto &thd = comp_file->contexts[i];
    thd.from = ((const char *)buf) + COMPRESS_CHUNK_SIZE * i;
    thd.from_len = chunk_len;
    thd.to_size = comp_size;
    thd.to = comp_file->comp_buf + comp_size * i;

    comp_file->tasks[i] =
        comp_ctxt->thread_pool->add_task([&thd](size_t thread_id) {
          thd.to_len =
              LZ4_compress_default(thd.from, thd.to, thd.from_len, thd.to_size);
        });
  }

  /* while compression is in progress, calculate content checksum */
  const uint32_t checksum = MY_XXH32(buf, len, 0);

  /* write LZ4 frame */

  /* Frame header (4 bytes magic, 1 byte FLG, 1 byte BD,
     8 bytes uncompressed content size, 1 byte HC) */
  uint8_t header[15];

  /* Magic Number */
  int4store(header, LZ4F_MAGICNUMBER);

  /* FLG Byte */
  const uint8_t flg =
      (1 << 6) | /* version = 01 */
      (1 << 5) | /* block independence (1 means blocks are independent) */
      (0 << 4) | /* block checksum (0 means no block checksum, we rely on
                    xbstream checksums) */
      (1 << 3) | /* content size (include uncompressed content size) */
      (1 << 2) | /* content checksum (include checksum of uncompressed data) */
      (0 << 1) | /* reserved */
      (0 << 0) /* dict id */;
  header[4] = flg;

  /* BD Byte (set maximum uncompressed block size to 4M) */
  uint8_t max_block_size_code = 0;
  if (COMPRESS_CHUNK_SIZE <= 64 * 1024) {
    max_block_size_code = 4;
  } else if (COMPRESS_CHUNK_SIZE <= 256 * 1024) {
    max_block_size_code = 5;
  } else if (COMPRESS_CHUNK_SIZE <= 1 * 1204 * 1024) {
    max_block_size_code = 6;
  } else if (COMPRESS_CHUNK_SIZE <= 4 * 1024 * 1024) {
    max_block_size_code = 7;
  } else {
    msg("compress: compress chunk size is too large for LZ4 compressor.\n");
    return 1;
  }
  const uint8_t bd = (max_block_size_code << 4);
  header[5] = bd;

  /* uncompressed content size */
  int8store(header + 6, len);

  /* HC Byte */
  header[14] = (MY_XXH32(header + 4, 10, 0) >> 8) & 0xff;

  bool error = false;

  /* write frame header */
  if (ds_write(dest_file, header, sizeof(header))) {
    error = true;
  }

  /* write compressed blocks */
  for (size_t i = 0; i < n_chunks; i++) {
    const auto &thd = comp_file->contexts[i];

    /* reap */
    comp_file->tasks[i].wait();

    if (error) continue;

    if (thd.to_len > 0) {
      /* compressed block length */
      if (write_uint32_le(dest_file, thd.to_len)) {
        error = true;
        continue;
      }

      /* block contents */
      if (ds_write(dest_file, thd.to, thd.to_len)) {
        error = true;
      }
    } else {
      /* uncompressed block length */
      if (write_uint32_le(dest_file, thd.from_len | LZ4F_UNCOMPRESSED_BIT)) {
        error = true;
        continue;
      }

      /* block contents */
      if (ds_write(dest_file, thd.from, thd.from_len)) {
        error = true;
        continue;
      }
    }

    comp_file->bytes_processed += thd.from_len;
  }

  if (error) goto err;

  /* LZ4 frame trailer */

  /* empty mark is zero-sized block */
  if (write_uint32_le(dest_file, 0)) {
    goto err;
  }

  /* content checksum */
  if (write_uint32_le(dest_file, checksum)) {
    goto err;
  }

  return 0;

err:
  msg("compress: write to the destination stream failed.\n");
  return 1;
}

static int compress_close(ds_file_t *file) {
  ds_compress_file_t *comp_file = (ds_compress_file_t *)file->ptr;
  ds_file_t *dest_file = comp_file->dest_file;

  int rc = ds_close(dest_file);

  my_free(comp_file->comp_buf);
  delete file;
  delete comp_file;

  return rc;
}

static void compress_deinit(ds_ctxt_t *ctxt) {
  xb_ad(ctxt->pipe_ctxt != nullptr);

  ds_compress_ctxt_t *comp_ctxt = (ds_compress_ctxt_t *)ctxt->ptr;

  delete comp_ctxt->thread_pool;
  delete comp_ctxt;

  my_free(ctxt->root);
  delete ctxt;
}

static inline int write_uint32_le(ds_file_t *file, uint32_t n) {
  uchar tmp[4];
  int4store(tmp, n);
  return ds_write(file, tmp, sizeof(tmp));
}
