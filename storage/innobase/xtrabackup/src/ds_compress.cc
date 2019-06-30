/******************************************************
Copyright (c) 2011-2019 Percona LLC and/or its affiliates.

Compressing datasink implementation using quicklz for XtraBackup.

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
#include <my_byteorder.h>
#include <my_io.h>
#include <mysql/service_mysql_alloc.h>
#include <mysql_version.h>
#include <quicklz.h>
#include <zlib.h>
#include "common.h"
#include "datasink.h"
#include "thread_pool.h"

#define COMPRESS_CHUNK_SIZE ((size_t)(xtrabackup_compress_chunk_size))
#define MY_QLZ_COMPRESS_OVERHEAD 400

typedef struct {
  const char *from;
  size_t from_len;
  char *to;
  size_t to_len;
  size_t to_size;
  uint32_t adler;
  qlz_state_compress state;
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

datasink_t datasink_compress = {&compress_init,  &compress_open,
                                &compress_write, nullptr,
                                &compress_close, &compress_deinit};

static inline int write_uint32_le(ds_file_t *file, uint32_t n);
static inline int write_uint64_le(ds_file_t *file, ulonglong n);

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
  xb_ad(ctxt->pipe_ctxt != nullptr);
  ds_ctxt_t *dest_ctxt = ctxt->pipe_ctxt;

  ds_compress_ctxt_t *comp_ctxt = (ds_compress_ctxt_t *)ctxt->ptr;

  /* Append the .qp extension to the filename */
  char new_name[FN_REFLEN];
  fn_format(new_name, path, "", ".qp", MYF(MY_APPEND_EXT));

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

  /* Write the qpress archive header */
  if (ds_write(dest_file, "qpress10", 8) ||
      write_uint64_le(dest_file, COMPRESS_CHUNK_SIZE)) {
    ds_close(dest_file);
    return nullptr;
  }

  /* We are going to create a one-file "flat" (i.e. with no
  subdirectories) archive. So strip the directory part from the path and
  remove the '.qp' suffix. */
  fn_format(new_name, path, "", "", MYF(MY_REPLACE_DIR));

  /* Write the qpress file header */
  size_t name_len = strlen(new_name);
  if (ds_write(dest_file, "F", 1) || write_uint32_le(dest_file, name_len) ||
      /* we want to write the terminating \0 as well */
      ds_write(dest_file, new_name, name_len + 1)) {
    ds_close(dest_file);
    return nullptr;
  }

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
  const size_t comp_size = COMPRESS_CHUNK_SIZE + MY_QLZ_COMPRESS_OVERHEAD;
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

          thd.to_len = qlz_compress(thd.from, thd.to, thd.from_len, &thd.state);

          /* qpress uses 0x00010000 as the initial value, but its own
          Adler-32 implementation treats the value differently:
            1. higher order bits are the sum of all bytes in the sequence
            2. lower order bits are the sum of resulting values at every
               step.
          So it's the other way around as compared to zlib's adler32().
          That's why  0x00000001 is being passed here to be compatible
          with qpress implementation. */

          thd.adler = adler32(0x00000001, (uchar *)thd.to, thd.to_len);
        });
  }

  bool error = false;

  /* write compressed blocks */
  for (size_t i = 0; i < n_chunks; i++) {
    const auto &thd = comp_file->contexts[i];

    /* reap */
    comp_file->tasks[i].wait();

    if (error) continue;

    if (thd.to_len > 0) {
      if (ds_write(dest_file, "NEWBNEWB", 8) ||
          write_uint64_le(dest_file, comp_file->bytes_processed)) {
        error = true;
        goto err;
      }

      comp_file->bytes_processed += thd.from_len;

      if (write_uint32_le(dest_file, thd.adler) ||
          ds_write(dest_file, thd.to, thd.to_len)) {
        error = true;
        goto err;
      }
    }

    comp_file->bytes_processed += thd.from_len;
  }

  return 0;

err:
  msg("compress: write to the destination stream failed.\n");
  return 1;
}

static int compress_close(ds_file_t *file) {
  ds_compress_file_t *comp_file = (ds_compress_file_t *)file->ptr;
  ds_file_t *dest_file = comp_file->dest_file;

  /* Write the qpress file trailer */
  ds_write(dest_file, "ENDSENDS", 8);

  /* Supposedly the number of written bytes should be written as a
  "recovery information" in the file trailer, but in reality qpress
  always writes 8 zeros here. Let's do the same */

  write_uint64_le(dest_file, 0);

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

static inline int write_uint64_le(ds_file_t *file, ulonglong n) {
  uchar tmp[8];

  int8store(tmp, n);
  return ds_write(file, tmp, sizeof(tmp));
}
