/******************************************************
Copyright (c) 2011-2023 Percona LLC and/or its affiliates.

Data sink interface.

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

#include "datasink.h"
#include <my_base.h>
#include "common.h"
#include "ds_buffer.h"
#include "ds_compress.h"
#include "ds_compress_lz4.h"
#include "ds_compress_zstd.h"
#include "ds_decompress.h"
#include "ds_decompress_lz4.h"
#include "ds_decompress_zstd.h"
#include "ds_decrypt.h"
#include "ds_encrypt.h"
#include "ds_fifo.h"
#include "ds_local.h"
#include "ds_stdout.h"
#include "ds_tmpfile.h"
#include "ds_xbstream.h"
#include "msg.h"

/************************************************************************
Create a datasink of the specified type */
ds_ctxt_t *ds_create(const char *root, ds_type_t type) {
  datasink_t *ds;
  ds_ctxt_t *ctxt;

  switch (type) {
    case DS_TYPE_STDOUT:
      ds = &datasink_stdout;
      break;
    case DS_TYPE_FIFO:
      ds = &datasink_fifo;
      break;
    case DS_TYPE_LOCAL:
      ds = &datasink_local;
      break;
    case DS_TYPE_XBSTREAM:
      ds = &datasink_xbstream;
      break;
    case DS_TYPE_COMPRESS_QUICKLZ:
      ds = &datasink_compress;
      break;
    case DS_TYPE_COMPRESS_LZ4:
      ds = &datasink_compress_lz4;
      break;
    case DS_TYPE_COMPRESS_ZSTD:
      ds = &datasink_compress_zstd;
      break;
    case DS_TYPE_DECOMPRESS_QUICKLZ:
      ds = &datasink_decompress;
      break;
    case DS_TYPE_DECOMPRESS_LZ4:
      ds = &datasink_decompress_lz4;
      break;
    case DS_TYPE_DECOMPRESS_ZSTD:
      ds = &datasink_decompress_zstd;
      break;
    case DS_TYPE_ENCRYPT:
      ds = &datasink_encrypt;
      break;
    case DS_TYPE_DECRYPT:
      ds = &datasink_decrypt;
      break;
    case DS_TYPE_TMPFILE:
      ds = &datasink_tmpfile;
      break;
    case DS_TYPE_BUFFER:
      ds = &datasink_buffer;
      break;
    default:
      msg("Unknown datasink type: %d\n", type);
      xb_ad(0);
      return NULL;
  }

  ctxt = ds->init(root);
  if (ctxt != NULL) {
    ctxt->datasink = ds;
  } else {
    msg("Error: failed to initialize datasink.\n");
    exit(EXIT_FAILURE);
  }

  return ctxt;
}

/************************************************************************
Open a datasink file */
ds_file_t *ds_open(ds_ctxt_t *ctxt, const char *path, MY_STAT *stat) {
  ds_file_t *file;

  file = ctxt->datasink->open(ctxt, path, stat);
  if (file != NULL) {
    file->datasink = ctxt->datasink;
    /* Save the ctxt this file is attached to so per-datasink state
    (e.g. leaf bytes_written counters) can be reached from the write
    paths via file->ctxt->ptr.  Tracking defaults to off; callers that
    want uncompressed-byte accounting call ds_track_uncomp() or use
    the ds_open_track_uncomp() convenience. */
    file->ctxt = ctxt;
    file->uncomp_bytes = nullptr;
  }

  return file;
}

void ds_track_uncomp(ds_file_t *file, xb_uncomp_bytes *uncomp_bytes) {
  if (file != nullptr) {
    file->uncomp_bytes = uncomp_bytes;
  }
}

ds_file_t *ds_open_track_uncomp(ds_ctxt_t *ctxt, const char *path,
                                MY_STAT *stat, xb_uncomp_bytes *uncomp_bytes) {
  ds_file_t *file = ds_open(ctxt, path, stat);
  ds_track_uncomp(file, uncomp_bytes);
  return file;
}

/************************************************************************
Write to a datasink file.
@return 0 on success, 1 on error. */
int ds_write(ds_file_t *file, const void *buf, size_t len) {
  const int rc = file->datasink->write(file, buf, len);
  /* Bump the backup-run uncompressed byte counter with the logical
  byte count only on success.  Wrapper-internal opens do not set
  file->uncomp_bytes, so every logical byte is counted exactly once
  at the top. */
  if (rc == 0 && file->uncomp_bytes != nullptr) {
    file->uncomp_bytes->add_uncompressed(len);
  }
  return rc;
}

/************************************************************************
Check if sparse files are supported.
@return 1 if yes. */
int ds_is_sparse_write_supported(ds_file_t *file) {
  if (file->datasink->write_sparse != nullptr) {
    return 1;
  }
  return 0;
}

/************************************************************************
Write sparse chunk if supported.
@return 0 on success, 1 on error. */
int ds_write_sparse(ds_file_t *file, const void *buf, size_t len,
                    size_t sparse_map_size, const ds_sparse_chunk_t *sparse_map,
                    bool punch_hole_supported) {
  if (file->datasink->write_sparse == nullptr) {
    return 1;
  }
  const int rc = file->datasink->write_sparse(file, buf, len, sparse_map_size,
                                              sparse_map, punch_hole_supported);
  if (rc == 0 && file->uncomp_bytes != nullptr) {
    /* `len` is the packed (hole-excluded) payload size: callers pre-pack
    the buffer and pass its length here, and local_write_sparse writes
    exactly that many bytes across the sparse_map chunks. */
    file->uncomp_bytes->add_uncompressed(len);
  }
  return rc;
}

/************************************************************************
Close a datasink file.
@return 0 on success, 1, on error. */
int ds_close(ds_file_t *file) { return file->datasink->close(file); }

/************************************************************************
Destroy a datasink handle */
void ds_destroy(ds_ctxt_t *ctxt) { ctxt->datasink->deinit(ctxt); }

/************************************************************************
Set the destination pipe for a datasink (only makes sense for compress and
tmpfile). */
void ds_set_pipe(ds_ctxt_t *ctxt, ds_ctxt_t *pipe_ctxt) {
  ctxt->pipe_ctxt = pipe_ctxt;
}

const ds_ctxt_t *ds_leaf(const ds_ctxt_t *head) {
  const ds_ctxt_t *c = head;
  if (c == nullptr) return nullptr;
  while (c->pipe_ctxt != nullptr) {
    c = c->pipe_ctxt;
  }
  return c;
}

bool ds_find_metric(const ds_ctxt_t *node, std::string_view name,
                    uint64_t *out) {
  if (node == nullptr || node->datasink == nullptr ||
      node->datasink->report_metrics == nullptr) {
    return false;
  }
  std::vector<ds_metric> v;
  node->datasink->report_metrics(node, v);
  for (const auto &m : v) {
    if (m.name == name) {
      if (out != nullptr) *out = m.value;
      return true;
    }
  }
  return false;
}
