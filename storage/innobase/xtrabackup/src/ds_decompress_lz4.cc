/******************************************************
Copyright (c) 2019 Percona LLC and/or its affiliates.

Encryption datasink implementation for XtraBackup.

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
#include "ds_istream.h"
#include "thread_pool.h"

#define LZ4F_MAGICNUMBER 0x184d2204U
#define LZ4F_UNCOMPRESSED_BIT (1U << 31)

class LZ4_stream {
 public:
  enum err_t { LZ4_OK, LZ4_ERROR, LZ4_INCOMPLETE, LZ4_LAST_BLOCK };

  struct block_info_t {
    uint32_t data_size;
    uint32_t checksum;
    bool uncompressed;
    const char *data;
    block_info_t()
        : data_size(0), checksum(0), uncompressed(false), data(nullptr) {}
  };

 private:
  enum state_t {
    LZ4_STREAM_STATE_BEFORE_FRAME_HEADER,
    LZ4_STREAM_STATE_BEFORE_BLOCK_HEADER
  };

  struct frame_info_t {
    size_t header_size;
    size_t content_size;
    size_t block_max_size;
    bool block_checksum;
    bool content_checksum;
    bool independent_blocks;
    uint32_t stored_checksum;
    frame_info_t()
        : header_size(0),
          content_size(0),
          block_max_size(0),
          block_checksum(false),
          content_checksum(false),
          independent_blocks(false),
          stored_checksum(0) {}
  };

  err_t parse_frame_header();

  err_t parse_block(block_info_t &block_info);

 public:
  ~LZ4_stream() {}

  void reset() { stream.reset(); }

  void set_buffer(const char *buf, size_t len) { stream.add_buffer(buf, len); }

  err_t next_block(block_info_t &block_info);

  bool empty() const { return stream.empty(); };

  size_t block_max_size() const { return frame_info.block_max_size; }

  uint32_t content_checksum() const { return frame_info.stored_checksum; }

  bool has_content_checksum() const { return frame_info.content_checksum; }

 private:
  Datasink_istream stream;
  frame_info_t frame_info;
  state_t state{LZ4_STREAM_STATE_BEFORE_FRAME_HEADER};
};

LZ4_stream::err_t LZ4_stream::next_block(block_info_t &block_info) {
  if (state == LZ4_STREAM_STATE_BEFORE_FRAME_HEADER) {
    /* parse frame header */
    stream.save_pos();
    const auto err = parse_frame_header();
    if (err == LZ4_ERROR) {
      /* parse error, exit with error */
      return LZ4_ERROR;
    } else if (err == LZ4_INCOMPLETE) {
      /* incomplete frame header, save remaining data and exit normally */
      stream.restore_pos();
      return LZ4_INCOMPLETE;
    }
    state = LZ4_STREAM_STATE_BEFORE_BLOCK_HEADER;
  }
  if (state == LZ4_STREAM_STATE_BEFORE_BLOCK_HEADER) {
    /* parse block */
    stream.save_pos();
    const auto err = parse_block(block_info);
    if (err == LZ4_ERROR) {
      /* parse error */
      return LZ4_ERROR;
    } else if (err == LZ4_INCOMPLETE) {
      /* incomplete block, save remaining data and exit normally */
      stream.restore_pos();
      return LZ4_INCOMPLETE;
    }
    if (block_info.data_size == 0) {
      /* zero block is the last block of the frame */
      state = LZ4_STREAM_STATE_BEFORE_FRAME_HEADER;
      return LZ4_LAST_BLOCK;
    }
  }
  return LZ4_OK;
}

LZ4_stream::err_t LZ4_stream::parse_frame_header() {
  char header[14];
  char *ptr = header;

  frame_info = frame_info_t();

  uint32_t magic;
  if (!stream.read_u32_le(&magic)) {
    return LZ4_INCOMPLETE;
  }

  if (magic != LZ4F_MAGICNUMBER) {
    msg("decompress: wrong LZ4 frame magic number\n");
    return LZ4_ERROR;
  }

  if (stream.read_bytes(ptr, 2) < 2) {
    return LZ4_INCOMPLETE;
  }

  const uint8_t flg = ptr[0];
  const uint8_t bd = ptr[1];

  ptr += 2;

  const uint8_t content_size_flag = (flg >> 3) & 0x1;
  const uint8_t dict_id_flag = flg & 0x1;

  frame_info.block_checksum = (flg >> 4) & 0x1;
  frame_info.content_checksum = (flg >> 2) & 0x1;
  frame_info.independent_blocks = (flg >> 5) & 0x1;

  switch ((bd >> 4) & 0x7) {
    case 4:
      frame_info.block_max_size = 64 * 1024;
      break;
    case 5:
      frame_info.block_max_size = 256 * 1024;
      break;
    case 6:
      frame_info.block_max_size = 1 * 1024 * 1024;
      break;
    case 7:
      frame_info.block_max_size = 4 * 1024 * 1024;
      break;
    default:
      msg("decompress: invalid value for maximum block size: 0x%0x\n",
          (bd >> 4) & 0x7);
      return LZ4_ERROR;
  }

  frame_info.header_size =
      7 + (content_size_flag ? 8 : 0) + (dict_id_flag ? 4 : 0);

  if (content_size_flag) {
    if (stream.read_bytes(ptr, 8) < 8) {
      return LZ4_INCOMPLETE;
    }
    frame_info.content_size = uint8korr(ptr);
    ptr += 8;
  }

  if (dict_id_flag) {
    msg("decompress: compression dictionaries are not supported\n");
    return LZ4_ERROR;
  }

  uint8_t hc;
  if (!stream.read_u8(&hc)) {
    return LZ4_INCOMPLETE;
  }

  const uint8_t checksum = (MY_XXH32(header, ptr - header, 0) >> 8) & 0xff;
  if (checksum != hc) {
    msg("decompress: frame checksum mismatch\n");
    return LZ4_ERROR;
  }

  return LZ4_OK;
}

LZ4_stream::err_t LZ4_stream::parse_block(block_info_t &block_info) {
  block_info = block_info_t();

  if (!stream.read_u32_le(&block_info.data_size)) {
    return LZ4_INCOMPLETE;
  }

  if (block_info.data_size & LZ4F_UNCOMPRESSED_BIT) {
    block_info.data_size ^= LZ4F_UNCOMPRESSED_BIT;
    block_info.uncompressed = true;
  } else if (block_info.data_size > frame_info.block_max_size) {
    msg("decompress: compressed block size exceeds maximum block size.\n");
    return LZ4_ERROR;
  }

  if (block_info.data_size == 0) {
    /* zero-block is a special case */
    if (frame_info.content_checksum) {
      if (!stream.read_u32_le(&frame_info.stored_checksum)) {
        return LZ4_INCOMPLETE;
      }
    }
    return LZ4_OK;
  }

  block_info.data = stream.ptr(block_info.data_size);
  if (block_info.data == nullptr) {
    return LZ4_INCOMPLETE;
  }

  if (frame_info.block_checksum) {
    if (!stream.read_u32_le(&block_info.checksum)) {
      return LZ4_INCOMPLETE;
    }
  }

  return LZ4_OK;
}

typedef struct {
  const char *from;
  size_t from_len;
  char *to;
  size_t to_len;
  size_t to_size;
  bool uncompressed;
} comp_thread_ctxt_t;

typedef struct {
  Thread_pool *thread_pool;
} ds_decompress_ctxt_t;

typedef struct {
  ds_file_t *dest_file;
  ds_decompress_ctxt_t *decomp_ctxt;
  size_t bytes_processed;
  char *decomp_buf;
  size_t decomp_buf_size;
  std::vector<std::future<void>> tasks;
  std::vector<comp_thread_ctxt_t> contexts;
  LZ4_stream stream;
  XXH32_state_t xxh;
} ds_decompress_file_t;

/* User-configurable decompression options */
uint ds_decompress_lz4_threads;

static ds_ctxt_t *decompress_init(const char *root);
static ds_file_t *decompress_open(ds_ctxt_t *ctxt, const char *path,
                                  MY_STAT *mystat);
static int decompress_write(ds_file_t *file, const void *buf, size_t len);
static int decompress_close(ds_file_t *file);
static void decompress_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_decompress_lz4 = {&decompress_init, &decompress_open,
                                      &decompress_write, &decompress_close,
                                      &decompress_deinit};

static ds_ctxt_t *decompress_init(const char *root) {
  ds_decompress_ctxt_t *decompress_ctxt = new ds_decompress_ctxt_t;
  decompress_ctxt->thread_pool = new Thread_pool(ds_decompress_lz4_threads);

  ds_ctxt_t *ctxt = new ds_ctxt_t;
  ctxt->ptr = decompress_ctxt;
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *decompress_open(ds_ctxt_t *ctxt, const char *path,
                                  MY_STAT *mystat) {
  char new_name[FN_REFLEN];
  const char *lz4_ext_pos;

  xb_ad(ctxt->pipe_ctxt != NULL);
  ds_ctxt_t *dest_ctxt = ctxt->pipe_ctxt;

  ds_decompress_ctxt_t *decomp_ctxt = (ds_decompress_ctxt_t *)ctxt->ptr;

  /* Remove the .lz4 extension from the filename */
  if ((lz4_ext_pos = strrchr(path, '.')) && !strcmp(lz4_ext_pos, ".lz4")) {
    strncpy(new_name, path, lz4_ext_pos - path);
    new_name[lz4_ext_pos - path] = 0;
  } else {
    /* Compressed files always have .lz4 extension. If that is missing assume
    this particular file isn't compressed for some reason and skip the
    decompression phase */
    msg("decompress: File %s passed to decompress but missing .lz4 extension\n",
        path);
    return NULL;
  }

  ds_file_t *dest_file = ds_open(dest_ctxt, new_name, mystat);
  if (dest_file == NULL) {
    return NULL;
  }

  ds_file_t *file = new ds_file_t;
  ds_decompress_file_t *decomp_file = new ds_decompress_file_t;
  decomp_file->dest_file = dest_file;
  decomp_file->decomp_ctxt = decomp_ctxt;
  decomp_file->decomp_buf_size = 4 * 1024 * 1024 * ds_decompress_lz4_threads;
  decomp_file->decomp_buf = (char *)my_malloc(
      PSI_NOT_INSTRUMENTED, decomp_file->decomp_buf_size, MYF(MY_FAE));

  XXH32_reset(&decomp_file->xxh, 0);

  /* reserve space for as  */
  decomp_file->contexts.resize(ds_decompress_lz4_threads * (4 * 1024 / 64));
  decomp_file->tasks.resize(ds_decompress_lz4_threads * (4 * 1024 / 64));

  file->ptr = decomp_file;
  file->path = dest_file->path;

  return file;
}

static int reap_and_write(ds_decompress_file_t *file, size_t n_blocks,
                          bool error) {
  for (size_t i = 0; i < n_blocks; ++i) {
    const auto &thd = file->contexts[i];

    /* reap */
    file->tasks[i].wait();

    if (error) continue;

    if (ds_write(file->dest_file, thd.to, thd.to_len)) {
      error = true;
    }

    if (file->stream.has_content_checksum()) {
      MY_XXH32_update(&file->xxh, thd.to, thd.to_len);
    }
  }

  return error ? 1 : 0;
}

static int decompress_write(ds_file_t *file, const void *buf, size_t len) {
  ds_decompress_file_t *decomp_file = (ds_decompress_file_t *)file->ptr;
  ds_decompress_ctxt_t *decomp_ctxt = decomp_file->decomp_ctxt;

  decomp_file->stream.set_buffer(static_cast<const char *>(buf), len);

  size_t n_blocks = 0;
  size_t decomp_len = 0;

  bool error = false;

  while (true) {
    LZ4_stream::block_info_t block_info;

    const auto err = decomp_file->stream.next_block(block_info);

    if (err == LZ4_stream::LZ4_ERROR) {
      error = true;
      break;
    } else if (err == LZ4_stream::LZ4_INCOMPLETE) {
      break;
    } else if (err == LZ4_stream::LZ4_LAST_BLOCK) {
      if (reap_and_write(decomp_file, n_blocks, error)) {
        error = true;
        break;
      }
      n_blocks = 0;
      decomp_len = 0;

      if (decomp_file->stream.has_content_checksum() &&
          XXH32_digest(&decomp_file->xxh) !=
              decomp_file->stream.content_checksum()) {
        msg("decompress: content checksum mismatch.\n");
        error = true;
        break;
      }
      XXH32_reset(&decomp_file->xxh, 0);
    } else if (err == LZ4_stream::LZ4_OK) {
      if (decomp_file->decomp_buf_size < decomp_len + block_info.data_size ||
          n_blocks >= decomp_file->contexts.size()) {
        /* decomp buffer is full, write the data */
        if (reap_and_write(decomp_file, n_blocks, error)) {
          error = true;
          break;
        }
        n_blocks = 0;
        decomp_len = 0;
      }

      /* decompress the block using thread pool */
      auto &thd = decomp_file->contexts[n_blocks];
      thd.uncompressed = block_info.uncompressed;
      thd.from = block_info.data;
      thd.from_len = block_info.data_size;
      thd.to = decomp_file->decomp_buf + decomp_len;
      thd.to_size = decomp_file->stream.block_max_size();
      decomp_file->tasks[n_blocks] =
          decomp_ctxt->thread_pool->add_task([&thd](size_t n) {
            if (!thd.uncompressed) {
              thd.to_len = LZ4_decompress_safe(thd.from, thd.to, thd.from_len,
                                               thd.to_size);
            } else {
              thd.to = (char *)thd.from;
              thd.to_len = thd.from_len;
            }
          });
      decomp_len += thd.to_size;
      ++n_blocks;
    }
  }

  /* write remaining data */
  if (reap_and_write(decomp_file, n_blocks, error)) {
    error = true;
  }

  decomp_file->stream.reset();

  return error ? 1 : 0;
}

static int decompress_close(ds_file_t *file) {
  ds_decompress_file_t *comp_file = (ds_decompress_file_t *)file->ptr;
  ds_file_t *dest_file = comp_file->dest_file;

  if (!comp_file->stream.empty()) {
    msg("compress: unprocessed data left in the buffer.\n");
    return 1;
  }

  int rc = ds_close(dest_file);

  my_free(comp_file->decomp_buf);

  delete file;
  delete comp_file;

  return rc;
}

static void decompress_deinit(ds_ctxt_t *ctxt) {
  xb_ad(ctxt->pipe_ctxt != nullptr);

  ds_decompress_ctxt_t *comp_ctxt = (ds_decompress_ctxt_t *)ctxt->ptr;

  delete comp_ctxt->thread_pool;
  delete comp_ctxt;

  my_free(ctxt->root);
  delete ctxt;
}
