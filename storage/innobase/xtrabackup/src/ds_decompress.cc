/******************************************************
Copyright (c) 2019 Aiven, Helsinki, Finland. https://aiven.io/

Decompressing datasink implementation for XtraBackup.

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

/* Possible states of input data parsing. There are quite a few different
states since we want to be able to do multithreaded processing while avoiding
any extra memory copying whenever data can be used directly from source buffer
*/
typedef enum {
  STATE_READ_ARCHIVE_HEADER,
  STATE_READ_FILE_HEADER,
  STATE_SKIP_FILE_NAME,
  STATE_DETECT_BLOCK_OR_EOF,
  STATE_READ_BLOCK_HEADER,
  STATE_READ_BLOCK_SIZE_HEADER,
  STATE_BUFFER_BLOCK_DATA,
  STATE_PROCESS_DIRECT_BLOCK_DATA,
  STATE_PROCESS_BUFFERED_BLOCK_DATA,
  STATE_READ_FILE_TRAILER,
  STATE_REACHED_EOF,
} decompress_state_t;

typedef struct {
  const char *from;
  char *from_to_free;
  char *to;
  size_t to_len;
  qlz_state_decompress state;
} decomp_thread_ctxt_t;

typedef struct {
  ulonglong chunk_size;
  Thread_pool *thread_pool;
} ds_decompress_ctxt_t;

typedef struct {
  ds_file_t *dest_file;
  ds_decompress_ctxt_t *decomp_ctxt;
  decompress_state_t state;
  size_t nbytes_expected;
  size_t nbytes_read;
  char header[20];
  char *buffer;
  decomp_thread_ctxt_t *threads;
  std::vector<std::future<void>> tasks;
  uint nthreads;
} ds_decompress_file_t;

/* User-configurable decompression options */
uint ds_decompress_quicklz_threads;

static ds_ctxt_t *decompress_init(const char *root);
static ds_file_t *decompress_open(ds_ctxt_t *ctxt, const char *path,
                                  MY_STAT *mystat);
static int decompress_write(ds_file_t *file, const void *buf, size_t len);
static int decompress_close(ds_file_t *file);
static void decompress_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_decompress = {&decompress_init,  &decompress_open,
                                  &decompress_write, nullptr,
                                  &decompress_close, &decompress_deinit};

static int decompress_process_metadata(ds_decompress_file_t *file,
                                       const char **ptr, size_t *len);
static void initialize_worker_thread_buffers(decomp_thread_ctxt_t *threads,
                                             uint n, ulonglong chunk_size);
static void destroy_worker_thread_buffers(decomp_thread_ctxt_t *threads,
                                          uint n);

static ds_ctxt_t *decompress_init(const char *root) {
  ds_decompress_ctxt_t *decompress_ctxt = new ds_decompress_ctxt_t;
  decompress_ctxt->thread_pool = new Thread_pool(ds_decompress_quicklz_threads);
  decompress_ctxt->chunk_size = 0;

  ds_ctxt_t *ctxt = new ds_ctxt_t;
  ctxt->ptr = decompress_ctxt;
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *decompress_open(ds_ctxt_t *ctxt, const char *path,
                                  MY_STAT *mystat) {
  char new_name[FN_REFLEN];
  const char *qp_ext_pos;
  decomp_thread_ctxt_t *threads;

  xb_ad(ctxt->pipe_ctxt != NULL);
  ds_ctxt_t *dest_ctxt = ctxt->pipe_ctxt;

  ds_decompress_ctxt_t *decomp_ctxt = (ds_decompress_ctxt_t *)ctxt->ptr;

  threads = (decomp_thread_ctxt_t *)my_malloc(
      PSI_NOT_INSTRUMENTED,
      sizeof(decomp_thread_ctxt_t) * ds_decompress_quicklz_threads,
      MYF(MY_FAE));
  if (threads == NULL) {
    msg("decompress: failed to create worker threads.\n");
    return NULL;
  }

  /* Remove the .qp extension from the filename */
  if ((qp_ext_pos = strrchr(path, '.')) && !strcmp(qp_ext_pos, ".qp")) {
    strncpy(new_name, path, qp_ext_pos - path);
    new_name[qp_ext_pos - path] = 0;
  } else {
    /* Compressed files always have .qp extension. If that is missing assume
    this particular file isn't compressed for some reason and skip the
    decompression phase */
    msg("decompress: File %s passed to decompress but missing .qp extension\n",
        path);
    return NULL;
  }

  ds_file_t *dest_file = ds_open(dest_ctxt, new_name, mystat);
  if (dest_file == NULL) {
    return NULL;
  }

  ds_decompress_file_t *decomp_file = new ds_decompress_file_t;

  decomp_file->dest_file = dest_file;
  decomp_file->decomp_ctxt = decomp_ctxt;
  decomp_file->state = STATE_READ_ARCHIVE_HEADER;
  decomp_file->nbytes_expected = 16;
  decomp_file->nbytes_read = 0;
  decomp_file->buffer = NULL;
  decomp_file->threads = threads;
  decomp_file->nthreads = ds_decompress_quicklz_threads;
  decomp_file->tasks.resize(ds_decompress_quicklz_threads);

  ds_file_t *file = new ds_file_t;
  file->ptr = decomp_file;
  file->path = dest_file->path;

  return file;
}

static int decompress_write(ds_file_t *file, const void *buf, size_t len) {
  ds_decompress_file_t *decomp_file;
  ds_decompress_ctxt_t *decomp_ctxt;
  decomp_thread_ctxt_t *threads;
  decomp_thread_ctxt_t *thd;
  uint nthreads;
  uint i;
  int res;
  const char *ptr;
  ds_file_t *dest_file;

  decomp_file = (ds_decompress_file_t *)file->ptr;
  decomp_ctxt = decomp_file->decomp_ctxt;
  dest_file = decomp_file->dest_file;

  threads = decomp_file->threads;
  nthreads = decomp_file->nthreads;

  res = 0;
  ptr = (const char *)buf;

  while (len > 0) {
    uint max_thread;

    /* Send data to worker threads for decompression */
    for (max_thread = 0; max_thread < nthreads; max_thread++) {
      res = decompress_process_metadata(decomp_file, &ptr, &len);
      if (res) {
        return res;
      }

      if (decomp_file->state != STATE_PROCESS_BUFFERED_BLOCK_DATA &&
          decomp_file->state != STATE_PROCESS_DIRECT_BLOCK_DATA) {
        break;
      }

      thd = threads + max_thread;

      if (decomp_file->state == STATE_PROCESS_BUFFERED_BLOCK_DATA) {
        thd->from = decomp_file->buffer;
        thd->from_to_free = decomp_file->buffer;
        decomp_file->buffer = NULL;
      } else {
        thd->from = ptr;
        thd->from_to_free = NULL;
        ptr += decomp_file->nbytes_expected;
        len -= decomp_file->nbytes_expected;
      }

      decomp_file->state = STATE_DETECT_BLOCK_OR_EOF;
      decomp_file->nbytes_read = 0;
      decomp_file->nbytes_expected = 1;

      decomp_file->tasks[max_thread] =
          decomp_ctxt->thread_pool->add_task([thd](uint32_t n) {
            thd->to_len = qlz_decompress(thd->from, thd->to, &thd->state);
          });
    }

    /* Reap and stream the decompressed data */
    for (i = 0; i < max_thread; i++) {
      thd = threads + i;

      decomp_file->tasks[i].wait();

      if (thd->from_to_free) {
        my_free(thd->from_to_free);
        thd->from_to_free = NULL;
      }

      xb_a(threads[i].to_len > 0);

      if (!res) {
        /* Don't stop iteration on failure here because the locks are global
        and we need to release them for all threads to avoid bad state and
        also need to loop threads to free buffers */
        res = ds_write(dest_file, threads[i].to, threads[i].to_len);
      }
    }

    if (res) {
      msg("decompress: write to the destination stream failed.\n");
      return res;
    }
  }

  return 0;
}

static int decompress_process_metadata(ds_decompress_file_t *decomp_file,
                                       const char **ptr, size_t *len) {
  size_t nbytes_missing;
  size_t nbytes_available;
  const char *header;
  ulonglong chunk_size;

  header = decomp_file->header;

  while ((*len) > 0) {
    if (decomp_file->state == STATE_REACHED_EOF) {
      msg("decompress: received unexpected bytes after file trailer.\n");
      return 1;
    }

    nbytes_missing = decomp_file->nbytes_expected - decomp_file->nbytes_read;
    nbytes_available = nbytes_missing <= *len ? nbytes_missing : *len;
    if (decomp_file->state == STATE_READ_ARCHIVE_HEADER ||
        decomp_file->state == STATE_READ_FILE_HEADER ||
        decomp_file->state == STATE_SKIP_FILE_NAME ||
        decomp_file->state == STATE_DETECT_BLOCK_OR_EOF ||
        decomp_file->state == STATE_READ_BLOCK_HEADER ||
        decomp_file->state == STATE_READ_FILE_TRAILER) {
      if (decomp_file->state != STATE_SKIP_FILE_NAME) {
        /* skip useless memcpy if all data is available immediately */
        if (decomp_file->nbytes_read == 0 &&
            nbytes_available == nbytes_missing) {
          header = *ptr;
        } else {
          memcpy(decomp_file->header + decomp_file->nbytes_read, *ptr,
                 nbytes_available);
        }
      }

      (*ptr) += nbytes_available;
      (*len) -= nbytes_available;
      decomp_file->nbytes_read += nbytes_available;
      if (decomp_file->nbytes_read < decomp_file->nbytes_expected) {
        return 0;
      }

      decomp_file->nbytes_read = 0;

      if (decomp_file->state == STATE_READ_ARCHIVE_HEADER) {
        if (strncmp(header, "qpress10", 8)) {
          msg("decompress: invalid archive header, 'qpress10' not found.\n");
          return 1;
        }
        chunk_size = uint8korr(header + 8);
        if (decomp_file->decomp_ctxt->chunk_size == 0) {
          decomp_file->decomp_ctxt->chunk_size = chunk_size;
        } else if (chunk_size != decomp_file->decomp_ctxt->chunk_size) {
          /* all files in single archive should be using the same chunk size */
          msg("decompress: multiple chunk sizes found: %lld != %lld.\n",
              chunk_size, decomp_file->decomp_ctxt->chunk_size);
          return 1;
        }
        initialize_worker_thread_buffers(decomp_file->threads,
                                         decomp_file->nthreads, chunk_size);
        decomp_file->state = STATE_READ_FILE_HEADER;
        decomp_file->nbytes_expected = 5;
      } else if (decomp_file->state == STATE_READ_FILE_HEADER) {
        if (header[0] != 'F') {
          msg("decompress: invalid file header, 'F' not found.\n");
          return 1;
        }
        decomp_file->state = STATE_SKIP_FILE_NAME;
        /* + 1 because file name length does not include zero terminator */
        decomp_file->nbytes_expected = uint4korr(header + 1) + 1;
      } else if (decomp_file->state == STATE_SKIP_FILE_NAME) {
        decomp_file->state = STATE_DETECT_BLOCK_OR_EOF;
        decomp_file->nbytes_expected = 1;
      } else if (decomp_file->state == STATE_DETECT_BLOCK_OR_EOF) {
        if (header[0] == 'N') {
          decomp_file->state = STATE_READ_BLOCK_HEADER;
          decomp_file->nbytes_expected = 19;
        } else if (header[0] == 'E') {
          decomp_file->state = STATE_READ_FILE_TRAILER;
          decomp_file->nbytes_expected = 15;
        } else {
          msg("decompress: invalid block header, 'N' or 'E' not found.\n");
          return 1;
        }
      } else if (decomp_file->state == STATE_READ_BLOCK_HEADER) {
        if (strncmp(header, "EWBNEWB", 7)) {
          msg("decompress: invalid block header, 'NEWBNEWB' not found.\n");
          return 1;
        }
        decomp_file->state = STATE_READ_BLOCK_SIZE_HEADER;
        decomp_file->nbytes_expected = 9;
      } else if (decomp_file->state == STATE_READ_FILE_TRAILER) {
        if (strncmp(header, "NDSENDS", 7)) {
          msg("decompress: invalid file trailing, 'ENDSENDS' not found.\n");
          return 1;
        }
        decomp_file->state = STATE_REACHED_EOF;
      }
    } else if (decomp_file->state == STATE_READ_BLOCK_SIZE_HEADER) {
      /* Block size handling requires a bit of extra effort because the
      decompress code expects size bytes to be part of the data buffer */
      if (decomp_file->nbytes_read == 0 && nbytes_available == nbytes_missing) {
        decomp_file->nbytes_expected = qlz_size_compressed(*ptr);
        if ((*len) >= decomp_file->nbytes_expected) {
          decomp_file->state = STATE_PROCESS_DIRECT_BLOCK_DATA;
          return 0;
        } else {
          decomp_file->state = STATE_BUFFER_BLOCK_DATA;
          decomp_file->buffer = (char *)my_malloc(
              PSI_NOT_INSTRUMENTED, decomp_file->nbytes_expected, MYF(MY_FAE));
          memcpy(decomp_file->buffer, *ptr, *len);
          decomp_file->nbytes_read = *len;
          (*ptr) += (*len);
          (*len) = 0;
          return 0;
        }
      } else {
        memcpy(decomp_file->header + decomp_file->nbytes_read, *ptr,
               nbytes_available);
        decomp_file->nbytes_read += nbytes_available;
        (*ptr) += nbytes_available;
        (*len) -= nbytes_available;
        if (decomp_file->nbytes_read < decomp_file->nbytes_expected) {
          return 0;
        }

        decomp_file->nbytes_expected = qlz_size_compressed(decomp_file->header);
        decomp_file->state = STATE_BUFFER_BLOCK_DATA;
        decomp_file->buffer = (char *)my_malloc(
            PSI_NOT_INSTRUMENTED, decomp_file->nbytes_expected, MYF(MY_FAE));
        memcpy(decomp_file->buffer, decomp_file->header,
               decomp_file->nbytes_read);
      }
    } else if (decomp_file->state == STATE_BUFFER_BLOCK_DATA) {
      memcpy(decomp_file->buffer + decomp_file->nbytes_read, *ptr,
             nbytes_available);
      (*ptr) += nbytes_available;
      (*len) -= nbytes_available;
      decomp_file->nbytes_read += nbytes_available;
      if (decomp_file->nbytes_read == decomp_file->nbytes_expected) {
        decomp_file->state = STATE_PROCESS_BUFFERED_BLOCK_DATA;
      }
      return 0;
    } else if (decomp_file->state == STATE_PROCESS_DIRECT_BLOCK_DATA ||
               decomp_file->state == STATE_PROCESS_BUFFERED_BLOCK_DATA) {
      return 0;
    }
  }

  return 0;
}

static int decompress_close(ds_file_t *file) {
  ds_decompress_file_t *decomp_file;
  ds_file_t *dest_file;
  int rc;
  decompress_state_t last_state;

  decomp_file = (ds_decompress_file_t *)file->ptr;
  dest_file = decomp_file->dest_file;
  last_state = decomp_file->state;

  rc = ds_close(dest_file);

  my_free(decomp_file->buffer);
  decomp_file->buffer = NULL;
  destroy_worker_thread_buffers(decomp_file->threads, decomp_file->nthreads);
  my_free(decomp_file->threads);
  delete decomp_file;
  delete file;

  if (last_state != STATE_REACHED_EOF) {
    msg("decompress: file closed before reaching end of compressed data.\n");
    rc = 1;
  }

  return rc;
}

static void decompress_deinit(ds_ctxt_t *ctxt) {
  xb_ad(ctxt->pipe_ctxt != NULL);

  ds_decompress_ctxt_t *decomp_ctxt = (ds_decompress_ctxt_t *)ctxt->ptr;
  delete decomp_ctxt->thread_pool;
  delete decomp_ctxt;

  my_free(ctxt->root);
  delete ctxt;
}

static void initialize_worker_thread_buffers(decomp_thread_ctxt_t *threads,
                                             uint n, ulonglong chunk_size) {
  uint i;

  for (i = 0; i < n; i++) {
    decomp_thread_ctxt_t *thd = threads + i;
    thd->to = (char *)my_malloc(PSI_NOT_INSTRUMENTED, chunk_size, MYF(MY_FAE));
  }
}

static void destroy_worker_thread_buffers(decomp_thread_ctxt_t *threads,
                                          uint n) {
  uint i;

  for (i = 0; i < n; i++) {
    decomp_thread_ctxt_t *thd = threads + i;
    my_free(thd->to);
  }
}
