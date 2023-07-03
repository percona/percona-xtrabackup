/******************************************************
Copyright (c) 2011-2023 Percona LLC and/or its affiliates.

The xbstream format reader implementation.

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
#include <zlib.h>
#include <mutex>
#include <thread>
#include "common.h"
#include "crc_glue.h"
#include "file_utils.h"
#include "msg.h"
#include "xbstream.h"

/* Allocate 1 MB for the payload buffer initially */
#define INIT_BUFFER_LEN (1024 * 1024)

#ifndef MY_OFF_T_MAX
#define MY_OFF_T_MAX (~(my_off_t)0UL)
#endif

xb_rstream_t *xb_stream_read_new_fifo(const char *path, int timeout) {
  xb_rstream_t *stream;
  File fd;
  std::mutex *mutex = new std::mutex();
  fd = open_fifo_for_read_with_timeout(path, timeout);

  if (fd < 0) return nullptr;

  stream = (xb_rstream_t *)my_malloc(PSI_NOT_INSTRUMENTED, sizeof(xb_rstream_t),
                                     MYF(MY_FAE));
  stream->fd = fd;
  stream->offset = 0;
  stream->mutex = mutex;

#ifdef __WIN__
  setmode(stream->fd, _O_BINARY);
#endif

  return stream;
}

xb_rstream_t *xb_stream_read_new_stdin(void) {
  xb_rstream_t *stream;
  std::mutex *mutex = new std::mutex();
  stream = (xb_rstream_t *)my_malloc(PSI_NOT_INSTRUMENTED, sizeof(xb_rstream_t),
                                     MYF(MY_FAE));

  stream->fd = fileno(stdin);
  stream->offset = 0;
  stream->mutex = mutex;

#ifdef __WIN__
  setmode(stream->fd, _O_BINARY);
#endif

  return stream;
}

static inline xb_chunk_type_t validate_chunk_type(uchar code) {
  switch ((xb_chunk_type_t)code) {
    case XB_CHUNK_TYPE_PAYLOAD:
    case XB_CHUNK_TYPE_SPARSE:
    case XB_CHUNK_TYPE_EOF:
      return (xb_chunk_type_t)code;
    default:
      return XB_CHUNK_TYPE_UNKNOWN;
  }
}

xb_rstream_result_t xb_stream_validate_checksum(xb_rstream_chunk_t *chunk) {
  ulong checksum;

  checksum =
      crc32_iso3309(chunk->checksum_part,
                    static_cast<const uchar *>(chunk->data), chunk->length);
  if (checksum != chunk->checksum) {
    msg("xb_stream_read_chunk(): invalid checksum at offset "
        "0x%llx: expected 0x%lx, read 0x%lx.\n",
        (ulonglong)chunk->checksum_offset, chunk->checksum, checksum);
    return XB_STREAM_READ_ERROR;
  }

  return XB_STREAM_READ_CHUNK;
}

#define F_READ(buf, len)                                          \
  do {                                                            \
    if (xb_read_full(fd, static_cast<uchar *>(buf), len) < len) { \
      msg("xb_stream_read_chunk(): my_read() failed.\n");         \
      goto err;                                                   \
    }                                                             \
  } while (0)

xb_rstream_result_t xb_stream_read_chunk(xb_rstream_t *stream,
                                         xb_rstream_chunk_t *chunk) {
  uint pathlen;
  size_t tbytes;
  ulonglong ullval;
  File fd = stream->fd;
  uchar *ptr;

  const uint chunk_length_min =
      CHUNK_HEADER_CONSTANT_LEN + FN_REFLEN + 4 + 8 + 8 + 4;

  /* Reallocate the buffer if needed */
  if (chunk_length_min > chunk->buflen) {
    chunk->raw_data =
        my_realloc(PSI_NOT_INSTRUMENTED, chunk->raw_data, chunk_length_min,
                   MYF(MY_WME | MY_ALLOW_ZERO_PTR));
    if (chunk->raw_data == NULL) {
      msg("xb_stream_read_chunk(): failed to increase buffer"
          " to %lu bytes.\n",
          (ulong)chunk_length_min);
      goto err;
    }
    chunk->buflen = chunk_length_min;
  }

  ptr = (uchar *)(chunk->raw_data);

  /* This is the only place where we expect EOF, so read with
  xb_read_full() rather than F_READ() */
  tbytes = xb_read_full(fd, ptr, CHUNK_HEADER_CONSTANT_LEN);
  if (tbytes == 0) {
    return XB_STREAM_READ_EOF;
  } else if (tbytes < CHUNK_HEADER_CONSTANT_LEN) {
    msg("xb_stream_read_chunk(): unexpected end of stream at "
        "offset 0x%llx.\n",
        stream->offset);
    goto err;
  }

  /* Chunk magic value */
  if (memcmp(ptr, XB_STREAM_CHUNK_MAGIC, 8)) {
    msg("xb_stream_read_chunk(): wrong chunk magic at offset "
        "0x%llx.\n",
        (ulonglong)stream->offset);
    goto err;
  }
  ptr += 8;
  stream->offset += 8;

  /* Chunk flags */
  chunk->flags = *ptr++;
  stream->offset++;

  /* Chunk type, ignore unknown ones if ignorable flag is set */
  chunk->type = validate_chunk_type(*ptr);
  if (chunk->type == XB_CHUNK_TYPE_UNKNOWN &&
      !(chunk->flags & XB_STREAM_FLAG_IGNORABLE)) {
    msg("xb_stream_read_chunk(): unknown chunk type 0x%lu at "
        "offset 0x%llx.\n",
        (ulong)*ptr, (ulonglong)stream->offset);
    goto err;
  }
  ptr++;
  stream->offset++;

  /* Path length */
  pathlen = uint4korr(ptr);
  if (pathlen >= FN_REFLEN) {
    msg("xb_stream_read_chunk(): path length (%lu) is too large at "
        "offset 0x%llx.\n",
        (ulong)pathlen, stream->offset);
    goto err;
  }
  chunk->pathlen = pathlen;
  stream->offset += 4;
  ptr += 4;

  xb_ad((ptr - (uchar *)(chunk->raw_data)) == CHUNK_HEADER_CONSTANT_LEN);

  /* Path */
  if (chunk->pathlen > 0) {
    F_READ(ptr, pathlen);
    memcpy(chunk->path, ptr, pathlen);
    stream->offset += pathlen;
    ptr += pathlen;
  }
  chunk->path[pathlen] = '\0';

  if (chunk->type == XB_CHUNK_TYPE_EOF) {
    chunk->raw_length = ptr - (uchar *)(chunk->raw_data);
    return XB_STREAM_READ_CHUNK;
  }

  if (chunk->type == XB_CHUNK_TYPE_SPARSE) {
    /* Sparse map size */
    F_READ(ptr, 4);
    ullval = uint4korr(ptr);
    chunk->sparse_map_size = (size_t)ullval;
    stream->offset += 4;
    ptr += 4;
  } else {
    chunk->sparse_map_size = 0;
  }

  /* Payload length */
  F_READ(ptr, 16);
  ullval = uint8korr(ptr);
  if (ullval > (ulonglong)SIZE_T_MAX) {
    msg("xb_stream_read_chunk(): chunk length is too large at offset 0x%llx: "
        "0x%llx.\n",
        (ulonglong)stream->offset, ullval);
    goto err;
  }
  chunk->length = (size_t)ullval;
  stream->offset += 8;
  ptr += 8;

  /* Payload offset */
  ullval = uint8korr(ptr);
  if (ullval > (ulonglong)MY_OFF_T_MAX) {
    msg("xb_stream_read_chunk(): chunk offset is too large at offset 0x%llx: "
        "0x%llx.\n",
        (ulonglong)stream->offset, ullval);
    goto err;
  }
  chunk->offset = (my_off_t)ullval;
  stream->offset += 8;
  ptr += 8;

  /* Checksum */
  F_READ(ptr, 4);
  chunk->checksum = uint4korr(ptr);
  chunk->checksum_offset = stream->offset;
  stream->offset += 4;
  ptr += 4;

  chunk->raw_length = chunk->length + (ptr - (uchar *)(chunk->raw_data)) +
                      chunk->sparse_map_size * 4 * 2;

  /* Reallocate the buffer if needed */
  if (chunk->raw_length > chunk->buflen) {
    size_t ptr_offs = ptr - (uchar *)(chunk->raw_data);
    chunk->raw_data =
        my_realloc(PSI_NOT_INSTRUMENTED, chunk->raw_data, chunk->raw_length,
                   MYF(MY_WME | MY_ALLOW_ZERO_PTR));
    if (chunk->raw_data == NULL) {
      msg("xb_stream_read_chunk(): failed to increase buffer to %lu bytes.\n",
          (ulong)chunk->raw_length);
      goto err;
    }
    chunk->buflen = chunk->raw_length;
    ptr = (uchar *)(chunk->raw_data) + ptr_offs;
  }

  /* Sparse map */
  chunk->checksum_part = 0;
  if (chunk->sparse_map_size > 0) {
    if (chunk->sparse_map_alloc_size < chunk->sparse_map_size) {
      chunk->sparse_map = static_cast<ds_sparse_chunk_t *>(
          my_realloc(PSI_NOT_INSTRUMENTED, chunk->sparse_map,
                     chunk->sparse_map_size * sizeof(ds_sparse_chunk_t),
                     MYF(MY_WME | MY_ALLOW_ZERO_PTR)));
      chunk->sparse_map_alloc_size = chunk->sparse_map_size;
    }
    for (size_t i = 0; i < chunk->sparse_map_size; ++i) {
      F_READ(ptr, 8);
      chunk->checksum_part = crc32_iso3309(chunk->checksum_part, ptr, 8);
      chunk->sparse_map[i].skip = uint4korr(ptr);
      stream->offset += 4;
      ptr += 4;
      chunk->sparse_map[i].len = uint4korr(ptr);
      stream->offset += 4;
      ptr += 4;
    }
  }

  /* Payload */
  if (chunk->length > 0) {
    F_READ(ptr, chunk->length);
    stream->offset += chunk->length;
    chunk->data = ptr;
  }

  return XB_STREAM_READ_CHUNK;

err:
  return XB_STREAM_READ_ERROR;
}

int xb_stream_read_done(xb_rstream_t *stream) {
  /*
   * FIFO stream needs to close FD otherwise other part might not get notified
   * we are not reading anymore
   */
  if (stream->fd > 2) {
    my_close(stream->fd, MYF(0));
    stream->fd = -1;
  }
  delete stream->mutex;
  my_free(stream);

  return 0;
}
