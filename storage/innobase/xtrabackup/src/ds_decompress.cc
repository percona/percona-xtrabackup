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
  pthread_t id;
  uint num;
  pthread_mutex_t ctrl_mutex;
  pthread_cond_t ctrl_cond;
  pthread_mutex_t data_mutex;
  pthread_cond_t data_cond;
  bool started;
  bool data_avail;
  bool cancelled;
  const char *from;
  char *from_to_free;
  char *to;
  size_t to_len;
  qlz_state_decompress state;
} decomp_thread_ctxt_t;

typedef struct {
  decomp_thread_ctxt_t *threads;
  uint nthreads;
  ulonglong chunk_size;
} ds_decompress_ctxt_t;

typedef struct {
  ds_file_t *dest_file;
  ds_decompress_ctxt_t *decomp_ctxt;
  decompress_state_t state;
  size_t nbytes_expected;
  size_t nbytes_read;
  char header[20];
  char *buffer;
} ds_decompress_file_t;

/* User-configurable decompression options */
uint ds_compress_decompress_threads;

static ds_ctxt_t *decompress_init(const char *root);
static ds_file_t *decompress_open(ds_ctxt_t *ctxt, const char *path,
                                  MY_STAT *mystat);
static int decompress_write(ds_file_t *file, const void *buf, size_t len);
static int decompress_close(ds_file_t *file);
static void decompress_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_decompress = {&decompress_init, &decompress_open,
                                  &decompress_write, &decompress_close,
                                  &decompress_deinit};

static int decompress_process_metadata(ds_decompress_file_t *file,
                                       const char **ptr, size_t *len);
static decomp_thread_ctxt_t *create_worker_threads(uint n);
static void initialize_worker_thread_buffers(decomp_thread_ctxt_t *threads,
                                             uint n, ulonglong chunk_size);
static void destroy_worker_threads(decomp_thread_ctxt_t *threads, uint n);
static void *decompress_worker_thread_func(void *arg);

static ds_ctxt_t *decompress_init(const char *root) {
  ds_ctxt_t *ctxt;
  ds_decompress_ctxt_t *decompress_ctxt;
  decomp_thread_ctxt_t *threads;

  threads = create_worker_threads(ds_compress_decompress_threads);
  if (threads == NULL) {
    msg("decompress: failed to create worker threads.\n");
    return NULL;
  }

  ctxt = (ds_ctxt_t *)my_malloc(
      PSI_NOT_INSTRUMENTED, sizeof(ds_ctxt_t) + sizeof(ds_decompress_ctxt_t),
      MYF(MY_FAE));

  decompress_ctxt = (ds_decompress_ctxt_t *)(ctxt + 1);
  decompress_ctxt->threads = threads;
  decompress_ctxt->nthreads = ds_compress_decompress_threads;
  decompress_ctxt->chunk_size = 0;

  ctxt->ptr = decompress_ctxt;
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *decompress_open(ds_ctxt_t *ctxt, const char *path,
                                  MY_STAT *mystat) {
  ds_decompress_ctxt_t *decomp_ctxt;
  ds_ctxt_t *dest_ctxt;
  ds_file_t *dest_file;
  char new_name[FN_REFLEN];
  ds_file_t *file;
  ds_decompress_file_t *decomp_file;
  const char *qp_ext_pos;

  xb_ad(ctxt->pipe_ctxt != NULL);
  dest_ctxt = ctxt->pipe_ctxt;

  decomp_ctxt = (ds_decompress_ctxt_t *)ctxt->ptr;

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

  dest_file = ds_open(dest_ctxt, new_name, mystat);
  if (dest_file == NULL) {
    return NULL;
  }

  file = (ds_file_t *)my_malloc(
      PSI_NOT_INSTRUMENTED, sizeof(ds_file_t) + sizeof(ds_decompress_file_t),
      MYF(MY_FAE));
  decomp_file = (ds_decompress_file_t *)(file + 1);
  decomp_file->dest_file = dest_file;
  decomp_file->decomp_ctxt = decomp_ctxt;
  decomp_file->state = STATE_READ_ARCHIVE_HEADER;
  decomp_file->nbytes_expected = 16;
  decomp_file->nbytes_read = 0;
  decomp_file->buffer = NULL;

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

  threads = decomp_ctxt->threads;
  nthreads = decomp_ctxt->nthreads;

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

      pthread_mutex_lock(&thd->ctrl_mutex);

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

      pthread_mutex_lock(&thd->data_mutex);
      thd->data_avail = true;
      pthread_cond_signal(&thd->data_cond);
      pthread_mutex_unlock(&thd->data_mutex);
    }

    /* Reap and stream the decompressed data */
    for (i = 0; i < max_thread; i++) {
      thd = threads + i;

      pthread_mutex_lock(&thd->data_mutex);
      while (thd->data_avail == true) {
        pthread_cond_wait(&thd->data_cond, &thd->data_mutex);
      }

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

      pthread_mutex_unlock(&threads[i].data_mutex);
      pthread_mutex_unlock(&threads[i].ctrl_mutex);
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
          initialize_worker_thread_buffers(decomp_file->decomp_ctxt->threads,
                                           decomp_file->decomp_ctxt->nthreads,
                                           chunk_size);
        } else if (chunk_size != decomp_file->decomp_ctxt->chunk_size) {
          /* all files in single archive should be using the same chunk size */
          msg("decompress: multiple chunk sizes found: %lld != %lld.\n",
              chunk_size, decomp_file->decomp_ctxt->chunk_size);
          return 1;
        }
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
  my_free(file);

  if (last_state != STATE_REACHED_EOF) {
    msg("decompress: file closed before reaching end of compressed data.\n");
    rc = 1;
  }

  return rc;
}

static void decompress_deinit(ds_ctxt_t *ctxt) {
  ds_decompress_ctxt_t *decomp_ctxt;

  xb_ad(ctxt->pipe_ctxt != NULL);

  decomp_ctxt = (ds_decompress_ctxt_t *)ctxt->ptr;

  destroy_worker_threads(decomp_ctxt->threads, decomp_ctxt->nthreads);

  my_free(ctxt->root);
  my_free(ctxt);
}

static decomp_thread_ctxt_t *create_worker_threads(uint n) {
  decomp_thread_ctxt_t *threads;
  uint i;

  threads = (decomp_thread_ctxt_t *)my_malloc(
      PSI_NOT_INSTRUMENTED, sizeof(decomp_thread_ctxt_t) * n, MYF(MY_FAE));

  for (i = 0; i < n; i++) {
    decomp_thread_ctxt_t *thd = threads + i;
    thd->to = NULL;
    thd->from_to_free = NULL;
  }

  for (i = 0; i < n; i++) {
    decomp_thread_ctxt_t *thd = threads + i;

    thd->num = i + 1;
    thd->started = false;
    thd->cancelled = false;
    thd->data_avail = false;

    /* Don't initialize to yet. Need to get chunk_size
    from a compressed file before that can be done */

    /* Initialize the control mutex and condition var */
    if (pthread_mutex_init(&thd->ctrl_mutex, NULL) ||
        pthread_cond_init(&thd->ctrl_cond, NULL)) {
      goto err;
    }

    /* Initialize and data mutex and condition var */
    if (pthread_mutex_init(&thd->data_mutex, NULL) ||
        pthread_cond_init(&thd->data_cond, NULL)) {
      goto err;
    }

    pthread_mutex_lock(&thd->ctrl_mutex);

    if (pthread_create(&thd->id, NULL, decompress_worker_thread_func, thd)) {
      msg("decompress: pthread_create() failed: errno = %d\n", errno);
      goto err;
    }
  }

  /* Wait for the threads to start */
  for (i = 0; i < n; i++) {
    decomp_thread_ctxt_t *thd = threads + i;

    while (thd->started == false)
      pthread_cond_wait(&thd->ctrl_cond, &thd->ctrl_mutex);
    pthread_mutex_unlock(&thd->ctrl_mutex);
  }

  return threads;

err:
  destroy_worker_threads(threads, n);
  return NULL;
}

static void initialize_worker_thread_buffers(decomp_thread_ctxt_t *threads,
                                             uint n, ulonglong chunk_size) {
  uint i;

  for (i = 0; i < n; i++) {
    decomp_thread_ctxt_t *thd = threads + i;
    thd->to = (char *)my_malloc(PSI_NOT_INSTRUMENTED, chunk_size, MYF(MY_FAE));
  }
}

static void destroy_worker_threads(decomp_thread_ctxt_t *threads, uint n) {
  uint i;

  for (i = 0; i < n; i++) {
    decomp_thread_ctxt_t *thd = threads + i;

    pthread_mutex_lock(&thd->data_mutex);
    threads[i].cancelled = true;
    pthread_cond_signal(&thd->data_cond);
    pthread_mutex_unlock(&thd->data_mutex);

    pthread_join(thd->id, NULL);

    pthread_cond_destroy(&thd->data_cond);
    pthread_mutex_destroy(&thd->data_mutex);
    pthread_cond_destroy(&thd->ctrl_cond);
    pthread_mutex_destroy(&thd->ctrl_mutex);

    my_free(thd->to);
  }

  my_free(threads);
}

static void *decompress_worker_thread_func(void *arg) {
  decomp_thread_ctxt_t *thd = (decomp_thread_ctxt_t *)arg;

  pthread_mutex_lock(&thd->ctrl_mutex);

  pthread_mutex_lock(&thd->data_mutex);

  thd->started = true;
  pthread_cond_signal(&thd->ctrl_cond);

  pthread_mutex_unlock(&thd->ctrl_mutex);

  while (1) {
    thd->data_avail = false;
    pthread_cond_signal(&thd->data_cond);

    while (!thd->data_avail && !thd->cancelled) {
      pthread_cond_wait(&thd->data_cond, &thd->data_mutex);
    }

    if (thd->cancelled) break;

    thd->to_len = qlz_decompress(thd->from, thd->to, &thd->state);
  }

  pthread_mutex_unlock(&thd->data_mutex);

  return NULL;
}
