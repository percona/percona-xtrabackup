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

#ifndef XB_DATASINK_H
#define XB_DATASINK_H

#include <my_compiler.h>
#include <my_dir.h>
#include <atomic>
#include <cstdint>
#include <string_view>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

struct datasink_struct;
typedef struct datasink_struct datasink_t;

/* ---------------------------------------------------------------------
   Backup-run uncompressed byte counter.

   A single global instance (defined in xtrabackup.cc) receives the raw
   byte count of every top-level ds_write / ds_write_sparse that has
   tracking enabled.  Its definition lives here rather than in
   xtrabackup.h so datasink.cc can inline add_uncompressed() without
   pulling in the full xtrabackup include cone, which keeps standalone
   tools (xbcrypt, xbstream) cheap to link.

   Kept lexically distinct from the tier-1 ds_metric / report_metrics /
   ds_find_metric system: that one describes per-datasink observations,
   this one is a single backup-run aggregate.  No shared names.
   --------------------------------------------------------------------- */

struct xb_uncomp_bytes {
  /** Record the raw byte count of one top-level ds_write /
  ds_write_sparse.  Called by the datasink framework when the
  issuing ds_file_t has tracking enabled. */
  ALWAYS_INLINE void add_uncompressed(uint64_t n) {
    uncompressed_bytes_.fetch_add(n, std::memory_order_relaxed);
  }

  /** @return total raw (pre-compression, hole-excluded) bytes
  accumulated across all top-level tracked opens this run. */
  ALWAYS_INLINE uint64_t get_uncompressed_backup_size() const {
    return uncompressed_bytes_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<uint64_t> uncompressed_bytes_{0};
};

typedef struct ds_ctxt {
  datasink_t *datasink = nullptr;
  char *root = nullptr;
  void *ptr = nullptr;
  struct ds_ctxt *pipe_ctxt = nullptr;
  bool fs_support_punch_hole = false;
} ds_ctxt_t;

typedef struct {
  void *ptr = nullptr;
  char *path = nullptr;
  datasink_t *datasink = nullptr;
  /* The datasink context this file was opened through.  Set by
  ds_open() so that leaves and other per-datasink counters can reach
  their private state via file->ctxt->ptr. */
  ds_ctxt_t *ctxt = nullptr;
  /* Optional pointer to a backup-run uncompressed-byte counter that
  should receive the raw byte count of every ds_write/ds_write_sparse
  on this file.  NULL disables tracking.  Set by ds_track_uncomp() or
  by the ds_open_track_uncomp() convenience. */
  xb_uncomp_bytes *uncomp_bytes = nullptr;
} ds_file_t;

typedef struct {
  size_t skip;
  size_t len;
} ds_sparse_chunk_t;

/* ---------------------------------------------------------------------
   Tier-1 metrics: per-datasink observations.

   Each datasink may publish zero or more named uint64 values about
   itself via the report_metrics vtable slot.  The slot is optional
   (may be null) -- a datasink that has nothing to say leaves it null.

   A datasink's report_metrics output is a private namespace: names
   within one datasink must be unique.  The same name MAY appear in a
   different datasink's output and denotes a different metric there;
   queries are therefore always scoped to a specific node (see
   ds_find_metric() below) rather than flattened across a pipeline.
   --------------------------------------------------------------------- */

/** A single datum published by a datasink's report_metrics.  The name
is a non-owning view (typically a string literal in the producer); the
value is a snapshot at read time. */
struct ds_metric {
  std::string_view name;
  uint64_t value;
};

struct datasink_struct {
  ds_ctxt_t *(*init)(const char *root);
  ds_file_t *(*open)(ds_ctxt_t *ctxt, const char *path, MY_STAT *stat);
  int (*write)(ds_file_t *file, const void *buf, size_t len);
  int (*write_sparse)(ds_file_t *file, const void *buf, size_t len,
                      size_t sparse_map_size,
                      const ds_sparse_chunk_t *sparse_map,
                      bool punch_hole_supported);
  int (*close)(ds_file_t *file);
  void (*deinit)(ds_ctxt_t *ctxt);
  /* Optional: append zero or more {name, value} entries describing
  this datasink's internal state.  NULL means "no metrics".  Only the
  three leaf datasinks populate this today (to publish
  "bytes_written"); wrappers leave it null and future wrappers can
  opt in by implementing it without any framework change. */
  void (*report_metrics)(const ds_ctxt_t *ctxt, std::vector<ds_metric> &out);
};

/* Supported datasink types */
typedef enum {
  DS_TYPE_STDOUT,
  DS_TYPE_FIFO,
  DS_TYPE_LOCAL,
  DS_TYPE_XBSTREAM,
  DS_TYPE_COMPRESS_QUICKLZ,
  DS_TYPE_COMPRESS_LZ4,
  DS_TYPE_COMPRESS_ZSTD,
  DS_TYPE_DECOMPRESS_QUICKLZ,
  DS_TYPE_DECOMPRESS_LZ4,
  DS_TYPE_DECOMPRESS_ZSTD,
  DS_TYPE_ENCRYPT,
  DS_TYPE_DECRYPT,
  DS_TYPE_TMPFILE,
  DS_TYPE_BUFFER
} ds_type_t;

/************************************************************************
Create a datasink of the specified type */
ds_ctxt_t *ds_create(const char *root, ds_type_t type);

/************************************************************************
Open a datasink file.  The returned file has uncompressed-byte tracking
disabled (file->uncomp_bytes == NULL); callers that want per-byte
accounting into a backup-run counter use ds_open_track_uncomp() or
ds_track_uncomp(). */
ds_file_t *ds_open(ds_ctxt_t *ctxt, const char *path, MY_STAT *stat);

/** Enable uncompressed-byte tracking on an already-opened ds_file_t.
After this call, every ds_write / ds_write_sparse on @p file adds its
raw byte count to *@p uncomp_bytes.  Safe to call with file == nullptr
(no-op).
@param[in,out] file         file to bind; nullptr makes the call a no-op
@param[in]     uncomp_bytes backup-run counter to receive the counts;
                            nullptr leaves tracking disabled. */
void ds_track_uncomp(ds_file_t *file, xb_uncomp_bytes *uncomp_bytes);

/** Convenience: ds_open() followed by ds_track_uncomp().  Use at
top-level backup opens (ds_data / ds_redo / ds_meta /
ds_uncompressed_data).  Pipeline-internal opens inside wrappers keep
using plain ds_open() so every logical byte is counted exactly once at
the top.
@param[in]     ctxt         pipeline to open through
@param[in]     path         path relative to the pipeline root
@param[in]     stat         size/mode hints for downstream datasinks
@param[in,out] uncomp_bytes backup-run counter; nullptr to skip tracking
@return newly opened file, or nullptr on error. */
ds_file_t *ds_open_track_uncomp(ds_ctxt_t *ctxt, const char *path,
                                MY_STAT *stat, xb_uncomp_bytes *uncomp_bytes);

/************************************************************************
Write to a datasink file.
@return 0 on success, 1 on error. */
int ds_write(ds_file_t *file, const void *buf, size_t len);

/************************************************************************
Check if sparse files are supported.
@return 1 if yes. */
int ds_is_sparse_write_supported(ds_file_t *file);

/************************************************************************
Write sparse chunk if supported.
@return 0 on success, 1 on error. */
int ds_write_sparse(ds_file_t *file, const void *buf, size_t len,
                    size_t sparse_map_size, const ds_sparse_chunk_t *sparse_map,
                    bool punch_hole_supported);

/************************************************************************
Close a datasink file.
@return 0 on success, 1, on error. */
int ds_close(ds_file_t *file);

/************************************************************************
Destroy a datasink handle */
void ds_destroy(ds_ctxt_t *ctxt);

/************************************************************************
Set the destination pipe for a datasink (only makes sense for compress and
tmpfile). */
void ds_set_pipe(ds_ctxt_t *ctxt, ds_ctxt_t *pipe_ctxt);

/** Walk the pipe_ctxt chain of @p head to its terminal node.
@param[in] head  any node in a datasink pipeline
@return the leaf ctxt (same as @p head if head has no pipe). */
const ds_ctxt_t *ds_leaf(const ds_ctxt_t *head);

/** Look up one named metric on a single datasink node.

Invokes @p node->datasink->report_metrics (if any) and returns the
first entry whose name equals @p name.  The lookup is scoped to a
single node: pass ds_leaf(head) (or any other specific ctxt) to
disambiguate when multiple datasinks in a pipeline publish metrics.

Safe to call when the node is quiesced (e.g. from backup_finish).
Returns false (and leaves *out untouched) if @p node is null, the
datasink does not implement report_metrics, or the name is not
published.
@param[in]  node  a specific datasink ctxt (not a pipeline head)
@param[in]  name  metric name to look up
@param[out] out   receives the metric value on a hit; may be null
@return true on hit, false otherwise. */
bool ds_find_metric(const ds_ctxt_t *node, std::string_view name,
                    uint64_t *out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XB_DATASINK_H */
