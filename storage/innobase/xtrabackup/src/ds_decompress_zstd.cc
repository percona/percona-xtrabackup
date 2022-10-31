/******************************************************
Copyright (c) 2022 Percona LLC and/or its affiliates.

Decompressing datasink implementation using ZSTD for XtraBackup.

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

#include <my_io.h>
#include <mysql/service_mysql_alloc.h>
#define ZSTD_STATIC_LINKING_ONLY  // Advanced API - ZSTD_frameHeader
#include <zstd.h>
#include <zstd_errors.h>
#include "common.h"
#include "datasink.h"
#include "ds_istream.h"
#include "msg.h"
#define XXH_STATIC_LINKING_ONLY
#include "my_xxhash.h"

#define LAST_BLOCK_MASK ((1 << 1) - 1)
#define BLOCK_TYPE_MASK ((1 << 2) - 1)

class ZSTD_stream {
 public:
  enum err_t { ZSTD_OK, ZSTD_ERROR, ZSTD_INCOMPLETE };

  ~ZSTD_stream() {}

  void reset() { stream.reset(); }

  void set_buffer(const char *buf, size_t len) { stream.add_buffer(buf, len); }

  bool empty() const { return stream.empty(); }

  err_t next_frame(size_t &frame_size);

  const char *ptr(size_t n) { return stream.ptr(n); }

 private:
  Datasink_istream stream;
};

/* check if we have a full frame to decode */
ZSTD_stream::err_t ZSTD_stream::next_frame(size_t &frame_size) {
  auto len = stream.length();
  stream.save_pos();
  auto zstd_frame_size = ZSTD_findFrameCompressedSize(stream.ptr(len), len);
  stream.restore_pos();
  auto is_error = ZSTD_isError(zstd_frame_size);
  if (!is_error) {
    frame_size = zstd_frame_size;
    return ZSTD_OK;
  }

  if (ZSTD_getErrorCode(zstd_frame_size) ==
      ZSTD_ErrorCode::ZSTD_error_srcSize_wrong)
    return ZSTD_INCOMPLETE;

  return ZSTD_ERROR;
}

typedef struct {
  ds_file_t *dest_file;
  char *decomp_buf;
  size_t decomp_buf_size;
  ZSTD_stream stream;
  ZSTD_DCtx *dctx;
} ds_decompress_zstd_file_t;

static ds_ctxt_t *decompress_init(const char *root);
static ds_file_t *decompress_open(ds_ctxt_t *ctxt, const char *path,
                                  MY_STAT *mystat);
static int decompress_write(ds_file_t *file, const void *buf, size_t len);
static int decompress_close(ds_file_t *file);
static void decompress_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_decompress_zstd = {&decompress_init,  &decompress_open,
                                       &decompress_write, nullptr,
                                       &decompress_close, &decompress_deinit};

static ds_ctxt_t *decompress_init(const char *root) {
  ds_ctxt_t *ctxt = new ds_ctxt_t;
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *decompress_open(ds_ctxt_t *ctxt, const char *path,
                                  MY_STAT *mystat) {
  char new_name[FN_REFLEN];
  const char *zstd_ext_pos;

  xb_ad(ctxt->pipe_ctxt != NULL);
  ds_ctxt_t *dest_ctxt = ctxt->pipe_ctxt;

  /* Remove the .zst extension from the filename */
  if ((zstd_ext_pos = strrchr(path, '.')) && !strcmp(zstd_ext_pos, ".zst")) {
    strncpy(new_name, path, zstd_ext_pos - path);
    new_name[zstd_ext_pos - path] = 0;
  } else {
    /* Compressed files always have .zst extension. If that is missing assume
    this particular file isn't compressed for some reason and skip the
    decompression phase */
    msg("decompress: File %s passed to decompress but missing .zst extension\n",
        path);
    return NULL;
  }

  ds_file_t *dest_file = ds_open(dest_ctxt, new_name, mystat);
  if (dest_file == NULL) {
    return NULL;
  }

  ds_file_t *file = new ds_file_t;
  ds_decompress_zstd_file_t *decomp_file = new ds_decompress_zstd_file_t;
  decomp_file->dest_file = dest_file;
  decomp_file->dctx = ZSTD_createDCtx();
  decomp_file->decomp_buf_size = 8 * 1024 * 1024;
  decomp_file->decomp_buf = (char *)my_malloc(
      PSI_NOT_INSTRUMENTED, decomp_file->decomp_buf_size, MYF(MY_FAE));

  file->ptr = decomp_file;
  file->path = dest_file->path;

  return file;
}

static int decompress_write(ds_file_t *file, const void *buf, size_t len) {
  ds_decompress_zstd_file_t *decomp_file =
      (ds_decompress_zstd_file_t *)file->ptr;
  decomp_file->stream.set_buffer(static_cast<const char *>(buf), len);

  bool error = false;

  while (true) {
    size_t frame_size = 0;
    const auto err = decomp_file->stream.next_frame(frame_size);
    if (err == ZSTD_stream::ZSTD_ERROR) {
      error = true;
      break;
    } else if (err == ZSTD_stream::ZSTD_INCOMPLETE) {
      break;
    } else if (err == ZSTD_stream::ZSTD_OK) {
      // found a full frame
      ZSTD_inBuffer input = {decomp_file->stream.ptr(frame_size), frame_size,
                             0};
      while (input.pos < input.size) {
        ZSTD_outBuffer output = {decomp_file->decomp_buf,
                                 decomp_file->decomp_buf_size, 0};
        size_t const ret =
            ZSTD_decompressStream(decomp_file->dctx, &output, &input);
        if (ZSTD_isError(ret)) {
          error = true;
          break;
        }
        ds_write(decomp_file->dest_file, decomp_file->decomp_buf, output.pos);
      }
    }
  }

  decomp_file->stream.reset();
  return error ? 1 : 0;
}

static int decompress_close(ds_file_t *file) {
  ds_decompress_zstd_file_t *comp_file = (ds_decompress_zstd_file_t *)file->ptr;
  ds_file_t *dest_file = comp_file->dest_file;

  if (!comp_file->stream.empty()) {
    msg("decompress: unprocessed data left in the buffer for file %s.\n",
        file->path);
    return 1;
  }

  int rc = ds_close(dest_file);

  ZSTD_freeDCtx(comp_file->dctx);
  my_free(comp_file->decomp_buf);

  delete file;
  delete comp_file;

  return rc;
}

static void decompress_deinit(ds_ctxt_t *ctxt) {
  xb_ad(ctxt->pipe_ctxt != nullptr);

  my_free(ctxt->root);
  delete ctxt;
}
