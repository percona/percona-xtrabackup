/******************************************************

Copyright (c) 2011-2023 Percona LLC and/or its affiliates.

The xbstream utility: serialize/deserialize files in the XBSTREAM format.

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

#include "xbstream.h"
#include <gcrypt.h>
#include <my_base.h>
#include <my_getopt.h>
#include <my_thread.h>
#include <mysql/service_mysql_alloc.h>
#include <mysql_version.h>
#include <typelib.h>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "common.h"
#include "crc_glue.h"
#include "datasink.h"
#include "ds_decompress.h"
#include "ds_decompress_lz4.h"
#include "ds_decompress_zstd.h"
#include "ds_decrypt.h"
#include "file_utils.h"
#include "msg.h"
#include "nulls.h"
#include "template_utils.h"
#include "xbcrypt_common.h"
#include "xtrabackup_version.h"

#define XBSTREAM_VERSION XTRABACKUP_VERSION
#define XBSTREAM_REVISION XTRABACKUP_REVISION
#define XBSTREAM_BUFFER_SIZE (10 * 1024 * 1024UL)

typedef enum { RUN_MODE_NONE, RUN_MODE_CREATE, RUN_MODE_EXTRACT } run_mode_t;

const char *xbstream_encrypt_algo_names[] = {"NONE", "AES128", "AES192",
                                             "AES256", NullS};
TYPELIB xbstream_encrypt_algo_typelib = {
    array_elements(xbstream_encrypt_algo_names) - 1, "",
    xbstream_encrypt_algo_names, NULL};

/* Following definitions are to avoid linking with unused datasinks
   and their link dependencies */
datasink_t datasink_archive;
datasink_t datasink_xbstream;
datasink_t datasink_compress;
datasink_t datasink_compress_lz4;
datasink_t datasink_compress_zstd;
datasink_t datasink_tmpfile;
datasink_t datasink_encrypt;
datasink_t datasink_fifo;

static run_mode_t opt_mode;
static char *opt_directory = NULL;
static bool opt_verbose = 0;
static int opt_parallel = 1;
static int opt_fifo_streams = 1;
static char *opt_fifo_dir = nullptr;
static uint opt_fifo_timeout = 60;
static ulong opt_encrypt_algo;
static char *opt_encrypt_key_file = NULL;
static char *opt_encrypt_key = NULL;
static int opt_encrypt_threads = 1;
static bool opt_decompress = 0;
static uint opt_decompress_threads = 1;
static bool opt_absolute_names = 0;

static const int compression_prefix_len = 4;
static const int compression_and_encryption_prefix_len = 12;

enum {
  OPT_DECOMPRESS = 256,
  OPT_DECOMPRESS_THREADS,
  OPT_ENCRYPT_THREADS,
  OPT_PARALLEL,
  OPT_FIFO_DIR,
  OPT_FIFO_TIMEOUT
};

static struct my_option my_long_options[] = {
    {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
     0, 0, 0, 0, 0},
    {"version", 'V', "Display version and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
     0, 0, 0, 0, 0, 0},
    {"create", 'c', "Stream the specified files to the standard output.", 0, 0,
     0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"decompress", OPT_DECOMPRESS, "Decompress individual backup files.",
     &opt_decompress, &opt_decompress, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"decompress-threads", OPT_DECOMPRESS_THREADS,
     "Number of threads for parallel data decompression. The default value is "
     "1.",
     &opt_decompress_threads, &opt_decompress_threads, 0, GET_UINT,
     REQUIRED_ARG, 1, 1, UINT_MAX, 0, 0, 0},
    {"extract", 'x',
     "Extract to disk files from the stream on the "
     "standard input.",
     0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"directory", 'C',
     "Change the current directory to the specified one "
     "before streaming or extracting.",
     &opt_directory, &opt_directory, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0,
     0, 0},
    {"verbose", 'v', "Print verbose output.", &opt_verbose, &opt_verbose, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"parallel", OPT_PARALLEL,
     "Number of worker threads for reading / writing.", &opt_parallel,
     &opt_parallel, 0, GET_INT, REQUIRED_ARG, 1, 1, INT_MAX, 0, 0, 0},
    {"fifo-streams", 'f',
     "Number of FIFO files to use for parallel datafiles stream. Setting this "
     "parameter to 1 disables FIFO and stream is sent to STDOUT",
     &opt_fifo_streams, &opt_fifo_streams, 0, GET_INT, REQUIRED_ARG, 1, 1,
     INT_MAX, 0, 0, 0},
    {"fifo-dir", OPT_FIFO_DIR, "Directory to read Named Pipe.", &opt_fifo_dir,
     &opt_fifo_dir, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"fifo-timeout", OPT_FIFO_TIMEOUT,
     "How many seconds to wait for other end to open the stream. "
     "Default 60 seconds",
     &opt_fifo_timeout, &opt_fifo_timeout, 0, GET_INT, REQUIRED_ARG, 60, 1,
     INT_MAX, 0, 0, 0},
    {"decrypt", 'd', "Decrypt files ending with .xbcrypt.", &opt_encrypt_algo,
     &opt_encrypt_algo, &xbstream_encrypt_algo_typelib, GET_ENUM, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},
    {"encrypt-key", 'k', "Encryption key", 0, 0, 0, GET_STR_ALLOC, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},
    {"encrypt-key-file", 'f', "File which contains encryption key.",
     &opt_encrypt_key_file, &opt_encrypt_key_file, 0, GET_STR_ALLOC,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"encrypt-threads", OPT_ENCRYPT_THREADS,
     "Number of threads for parallel data encryption. "
     "The default value is 1.",
     &opt_encrypt_threads, &opt_encrypt_threads, 0, GET_INT, REQUIRED_ARG, 1, 1,
     INT_MAX, 0, 0, 0},
    {"absolute-names", 'P',
     "Don't strip leading slashes from file names when creating archives.",
     &opt_absolute_names, &opt_absolute_names, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
     0, 0},

    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

typedef struct {
  char *path;
  uint pathlen;
  my_off_t offset;
  ds_file_t *file;
  std::mutex *mutex;
} file_entry_t;

typedef struct {
  int thread_id;
  std::unordered_map<std::string, file_entry_t *> *filehash;
  std::list<xb_rstream_t *> *streams;
  ds_ctxt_t *ds_ctxt;
  ds_ctxt_t *ds_decompress_quicklz_ctxt;
  ds_ctxt_t *ds_decompress_lz4_ctxt;
  ds_ctxt_t *ds_decompress_zstd_ctxt;
  ds_ctxt_t *ds_decrypt_quicklz_ctxt;
  ds_ctxt_t *ds_decrypt_lz4_ctxt;
  ds_ctxt_t *ds_decrypt_zstd_ctxt;
  ds_ctxt_t *ds_decrypt_uncompressed_ctxt;
  std::mutex *mutex;
  std::atomic<bool> *has_errors;
} extract_ctxt_t;

static int get_options(int *argc, char ***argv);
static int mode_create(int argc, char **argv);
static int mode_extract(int n_threads, int argc, char **argv);
static bool get_one_option(int optid, const struct my_option *opt,
                           char *argument);

int main(int argc, char **argv) {
  MY_INIT(argv[0]);

  if (get_options(&argc, &argv)) {
    goto err;
  }

  crc_init();

  if (opt_mode == RUN_MODE_NONE) {
    msg("%s: either -c or -x must be specified.\n", my_progname);
    goto err;
  }

  /* Change the current directory if -C is specified */
  if (opt_directory && my_setwd(opt_directory, MYF(MY_WME))) {
    goto err;
  }

  if (opt_mode == RUN_MODE_EXTRACT && opt_fifo_streams > 1) {
    if (opt_fifo_dir == nullptr) {
      msg("%s: --fifo-streams requires --fifo-dir parameter.\n", my_progname);
      goto err;
    }
    /* adjust n_threads to at least fifo streams */
    if (opt_parallel < opt_fifo_streams) {
      msg_ts(
          "%s: Adjusting number of threads from %d to %d to match number of "
          "fifo streams.\n",
          my_progname, opt_parallel, opt_fifo_streams);
      opt_parallel = opt_fifo_streams;
    }
  }

  if (opt_encrypt_algo || opt_encrypt_key) {
    xb_libgcrypt_init();
  }

  if (opt_mode == RUN_MODE_CREATE && mode_create(argc, argv)) {
    goto err;
  } else if (opt_mode == RUN_MODE_EXTRACT &&
             mode_extract(opt_parallel, argc, argv)) {
    goto err;
  }

  my_cleanup_options(my_long_options);

  my_end(0);

  return EXIT_SUCCESS;
err:
  my_cleanup_options(my_long_options);

  my_end(0);

  exit(EXIT_FAILURE);
}

static int get_options(int *argc, char ***argv) {
  int ho_error;

  if ((ho_error =
           handle_options(argc, argv, my_long_options, get_one_option))) {
    exit(EXIT_FAILURE);
  }

  return 0;
}

static void print_version(void) {
  printf("%s  Ver %s for %s (%s) (revision id: %s)%s\n", my_progname,
         XBSTREAM_VERSION, SYSTEM_TYPE, MACHINE_TYPE, XBSTREAM_REVISION,
#ifdef PROBUILD
         "-pro"
#else
         ""
#endif
  );
}

static void usage(void) {
  print_version();
  puts("Copyright (C) 2011-2018 Percona LLC and/or its affiliates.");
  puts(
      "This software comes with ABSOLUTELY NO WARRANTY. "
      "This is free software,\nand you are welcome to modify and "
      "redistribute it under the GPL license.\n");

  puts("Serialize/deserialize files in the XBSTREAM format.\n");

  puts("Usage: ");
  printf(
      "  %s -c [OPTIONS...] FILES...	# stream specified files to "
      "standard output.\n",
      my_progname);
  printf(
      "  %s -x [OPTIONS...]		# extract files from the stream"
      "on the standard input.\n",
      my_progname);

  puts("\nOptions:");
  my_print_help(my_long_options);
}

static int set_run_mode(run_mode_t mode) {
  if (opt_mode != RUN_MODE_NONE) {
    msg("%s: can't set specify both -c and -x.\n", my_progname);
    return 1;
  }

  opt_mode = mode;

  return 0;
}

static bool get_one_option(int optid,
                           const struct my_option *opt __attribute__((unused)),
                           char *argument __attribute__((unused))) {
  switch (optid) {
    case 'c':
      if (set_run_mode(RUN_MODE_CREATE)) {
        return true;
      }
      break;
    case 'x':
      if (set_run_mode(RUN_MODE_EXTRACT)) {
        return true;
      }
      break;
    case 'k':
      hide_option(argument, &opt_encrypt_key);
      break;
    case 'V':
      print_version();
      exit(0);
    case '?':
      usage();
      exit(0);
  }

  return false;
}

static int stream_one_file(File file, xb_wstream_file_t *xbfile) {
  uchar *buf;
  size_t bytes;
  [[maybe_unused]] size_t offset;

  posix_fadvise(file, 0, 0, POSIX_FADV_SEQUENTIAL);
  offset = my_tell(file, MYF(MY_WME));

  buf = (uchar *)(my_malloc(PSI_NOT_INSTRUMENTED, XBSTREAM_BUFFER_SIZE,
                            MYF(MY_FAE)));

  while ((bytes = my_read(file, buf, XBSTREAM_BUFFER_SIZE, MYF(MY_WME))) > 0) {
    if (xb_stream_write_data(xbfile, buf, bytes)) {
      msg("%s: xb_stream_write_data() failed.\n", my_progname);
      my_free(buf);
      return 1;
    }
    posix_fadvise(file, offset, XBSTREAM_BUFFER_SIZE, POSIX_FADV_DONTNEED);
    offset += XBSTREAM_BUFFER_SIZE;
  }

  my_free(buf);

  if (bytes == (size_t)-1) {
    return 1;
  }

  return 0;
}

static int mode_create(int argc, char **argv) {
  int i;
  MY_STAT mystat;
  xb_wstream_t *stream;

  if (argc < 1) {
    msg("%s: no files are specified.\n", my_progname);
    return 1;
  }

  stream = xb_stream_write_new();
  if (stream == NULL) {
    msg("%s: xb_stream_write_new() failed.\n", my_progname);
    return 1;
  }

  for (i = 0; i < argc; i++) {
    char *filepath = argv[i];
    const char *filepath_dst;
    int filepath_prefix_len;
    File src_file;
    xb_wstream_file_t *file;

    if (my_stat(filepath, &mystat, MYF(MY_WME)) == NULL) {
      goto err;
    }
    if (!MY_S_ISREG(mystat.st_mode)) {
      msg("%s: %s is not a regular file, exiting.\n", my_progname, filepath);
      goto err;
    }

    if ((src_file = my_open(filepath, O_RDONLY, MYF(MY_WME))) < 0) {
      msg("%s: failed to open %s.\n", my_progname, filepath);
      goto err;
    }

    filepath_dst = opt_absolute_names
                       ? filepath
                       : safer_name_suffix(filepath, &filepath_prefix_len);
    file = xb_stream_write_open(stream, filepath_dst, &mystat, NULL, NULL);
    if (file == NULL) {
      goto err;
    }

    if (opt_verbose) {
      msg("%s\n", filepath);
    }

    if (stream_one_file(src_file, file) || xb_stream_write_close(file) ||
        my_close(src_file, MYF(MY_WME))) {
      goto err;
    }
  }

  xb_stream_write_done(stream);

  return 0;
err:
  xb_stream_write_done(stream);

  return 1;
}

/************************************************************************
Check if string ends with given suffix.
@return true if string ends with given suffix. */
static bool ends_with(const char *str, const char *suffix) {
  size_t suffix_len = strlen(suffix);
  size_t str_len = strlen(str);
  return (str_len >= suffix_len &&
          strcmp(str + str_len - suffix_len, suffix) == 0);
}

static file_entry_t *file_entry_new(extract_ctxt_t *ctxt, const char *path,
                                    uint pathlen) {
  file_entry_t *entry;
  ds_file_t *file;
  entry = (file_entry_t *)my_malloc(PSI_NOT_INSTRUMENTED, sizeof(file_entry_t),
                                    MYF(MY_WME | MY_ZEROFILL));
  if (entry == NULL) {
    return NULL;
  }
  entry->mutex = new std::mutex();

  entry->path = my_strndup(PSI_NOT_INSTRUMENTED, path, pathlen, MYF(MY_WME));
  if (entry->path == NULL) {
    goto err;
  }
  entry->pathlen = pathlen;

  if (ctxt->ds_decrypt_quicklz_ctxt && ends_with(path, ".qp.xbcrypt")) {
    file = ds_open(ctxt->ds_decrypt_quicklz_ctxt, path, NULL);
  } else if (ctxt->ds_decrypt_lz4_ctxt && ends_with(path, ".lz4.xbcrypt")) {
    file = ds_open(ctxt->ds_decrypt_lz4_ctxt, path, NULL);
  } else if (ctxt->ds_decrypt_zstd_ctxt && ends_with(path, ".zst.xbcrypt")) {
    file = ds_open(ctxt->ds_decrypt_zstd_ctxt, path, NULL);
  } else if (ctxt->ds_decrypt_uncompressed_ctxt &&
             ends_with(path, ".xbcrypt")) {
    file = ds_open(ctxt->ds_decrypt_uncompressed_ctxt, path, NULL);
  } else if (ctxt->ds_decompress_quicklz_ctxt && ends_with(path, ".qp")) {
    file = ds_open(ctxt->ds_decompress_quicklz_ctxt, path, NULL);
  } else if (ctxt->ds_decompress_lz4_ctxt && ends_with(path, ".lz4")) {
    file = ds_open(ctxt->ds_decompress_lz4_ctxt, path, NULL);
  } else if (ctxt->ds_decompress_zstd_ctxt && ends_with(path, ".zst")) {
    file = ds_open(ctxt->ds_decompress_zstd_ctxt, path, NULL);
  } else {
    file = ds_open(ctxt->ds_ctxt, path, NULL);
  }
  if (file == NULL) {
    msg("%s: failed to create file.\n", my_progname);
    goto err;
  }

  if (opt_verbose) {
    msg("%s\n", entry->path);
  }

  entry->file = file;

  return entry;

err:
  if (entry->path != NULL) {
    my_free(entry->path);
  }
  my_free(entry);

  return NULL;
}

static void file_entry_free(file_entry_t *entry) {
  ds_close(entry->file);
  my_free(entry->path);
  delete entry->mutex;
  my_free(entry);
}

static void extract_worker_thread_func(extract_ctxt_t &ctxt) {
  xb_rstream_t *stream = NULL;
  xb_rstream_chunk_t chunk;
  file_entry_t *entry;
  xb_rstream_result_t res = XB_STREAM_READ_CHUNK;

  ctxt.mutex->lock();
  stream = ctxt.streams->front();
  ctxt.streams->pop_front();
  ctxt.streams->push_back(stream);
  ctxt.mutex->unlock();

  my_thread_init();

  memset(&chunk, 0, sizeof(chunk));

  while (1) {
    /* Abort in case of error in any thread */
    if (ctxt.has_errors->load()) {
      break;
    }

    stream->mutex->lock();
    res = xb_stream_read_chunk(stream, &chunk);
    if (res != XB_STREAM_READ_CHUNK) {
      stream->mutex->unlock();
      break;
    }

    /* If unknown type and ignorable flag is set, skip this chunk */
    if (chunk.type == XB_CHUNK_TYPE_UNKNOWN &&
        !(chunk.flags & XB_STREAM_FLAG_IGNORABLE)) {
      stream->mutex->unlock();
      continue;
    }

    if (!opt_absolute_names) {
      int filepath_prefix_len;
      safer_name_suffix(chunk.path, &filepath_prefix_len);
      if (filepath_prefix_len != 0) {
        msg("%s: absolute path not allowed: %.*s.\n", my_progname,
            chunk.pathlen, chunk.path);
        res = XB_STREAM_READ_ERROR;
        stream->mutex->unlock();
        break;
      }
    }

    ctxt.mutex->lock();
    stream->mutex->unlock();
    /* See if we already have this file open */
    std::unordered_map<std::string, file_entry_t *>::const_iterator entry_it =
        ctxt.filehash->find(chunk.path);

    if (entry_it == ctxt.filehash->end()) {
      entry = file_entry_new(&ctxt, chunk.path, chunk.pathlen);
      if (entry == NULL) {
        res = XB_STREAM_READ_ERROR;
        ctxt.mutex->unlock();
        break;
      }
      ctxt.filehash->insert({chunk.path, entry});
    } else {
      entry = entry_it->second;
    }

    entry->mutex->lock();

    ctxt.mutex->unlock();

    if (chunk.type == XB_CHUNK_TYPE_PAYLOAD ||
        chunk.type == XB_CHUNK_TYPE_SPARSE) {
      res = xb_stream_validate_checksum(&chunk);
    }

    if (res != XB_STREAM_READ_CHUNK) {
      entry->mutex->unlock();
      break;
    }

    if (chunk.type == XB_CHUNK_TYPE_EOF) {
      ctxt.mutex->lock();
      entry->mutex->unlock();
      ctxt.filehash->erase(entry->path);
      file_entry_free(entry);
      ctxt.mutex->unlock();
      /*
       * no need for mutex here. At this point, we are guarantee that all other
       * threads have completed its work with this file
       */
      if (opt_decompress && ctxt.ds_ctxt->fs_support_punch_hole &&
          (is_compressed_suffix(chunk.path) ||
           is_encrypted_and_compressed_suffix(chunk.path))) {
        char path[FN_REFLEN] = {0};
        memcpy(path, chunk.path, strlen(chunk.path));
        unsigned short int qpress_offset = is_qpress_file(path) ? 1 : 0;
        if (is_compressed_suffix(path))
          path[strlen(path) - compression_prefix_len + qpress_offset] = 0;
        if (is_encrypted_and_compressed_suffix(path))
          path[strlen(path) - compression_and_encryption_prefix_len +
               qpress_offset] = 0;

        char error[512];
        if (!restore_sparseness(path, XBSTREAM_BUFFER_SIZE, error)) {
          msg("%s: restore_sparseness failed for file %s: %s\n", my_progname,
              chunk.path, error);
        }
      }

      continue;
    }

    if (entry->offset != chunk.offset) {
      msg("%s: out-of-order chunk: real offset = 0x%llx, "
          "expected offset = 0x%llx\n",
          my_progname, chunk.offset, entry->offset);
      entry->mutex->unlock();
      res = XB_STREAM_READ_ERROR;
      break;
    }

    if (chunk.type == XB_CHUNK_TYPE_PAYLOAD) {
      if (ds_write(entry->file, chunk.data, chunk.length)) {
        msg("%s: my_write() failed.\n", my_progname);
        entry->mutex->unlock();
        res = XB_STREAM_READ_ERROR;
        break;
      }

      entry->offset += chunk.length;
    } else if (chunk.type == XB_CHUNK_TYPE_SPARSE) {
      if (ds_write_sparse(entry->file, chunk.data, chunk.length,
                          chunk.sparse_map_size, chunk.sparse_map,
                          ctxt.ds_ctxt->fs_support_punch_hole)) {
        msg("%s: my_write() failed.\n", my_progname);
        entry->mutex->unlock();
        res = XB_STREAM_READ_ERROR;
        break;
      }

      for (size_t i = 0; i < chunk.sparse_map_size; ++i)
        entry->offset += chunk.sparse_map[i].skip;
      entry->offset += chunk.length;
    }

    entry->mutex->unlock();
  }

  my_free(chunk.raw_data);
  my_free(chunk.sparse_map);

  my_thread_end();

  if (res == XB_STREAM_READ_ERROR) ctxt.has_errors->store(res);
}

static int mode_extract(int n_threads, int argc __attribute__((unused)),
                        char **argv __attribute__((unused))) {
  ds_ctxt_t *ds_ctxt = NULL;
  ds_ctxt_t *ds_decrypt_lz4_ctxt = NULL;
  ds_ctxt_t *ds_decrypt_zstd_ctxt = NULL;
  ds_ctxt_t *ds_decrypt_quicklz_ctxt = NULL;
  ds_ctxt_t *ds_decrypt_uncompressed_ctxt = NULL;
  ds_ctxt_t *ds_decompress_quicklz_ctxt = NULL;
  ds_ctxt_t *ds_decompress_lz4_ctxt = NULL;
  ds_ctxt_t *ds_decompress_zstd_ctxt = NULL;
  extract_ctxt_t *data_threads = NULL;
  std::list<xb_rstream_t *> *streams = new std::list<xb_rstream_t *>();
  xb_rstream_t *stream = NULL;
  std::atomic<bool> has_errors{false};
  std::vector<std::thread> threads;
  std::unordered_map<std::string, file_entry_t *> *filehash =
      new std::unordered_map<std::string, file_entry_t *>();
  int i;
  std::mutex mutex;
  int ret = 0;

  /* If --directory is specified, it is already set as CWD by now. */
  ds_ctxt = ds_create(".", DS_TYPE_LOCAL);
  if (ds_ctxt == NULL) {
    ret = 1;
    goto exit;
  }

  if (opt_decompress) {
    ds_decompress_quicklz_threads = opt_decompress_threads;
    ds_decompress_quicklz_ctxt = ds_create(".", DS_TYPE_DECOMPRESS_QUICKLZ);
    if (ds_decompress_quicklz_ctxt == NULL) {
      ret = 1;
      goto exit;
    }
    ds_set_pipe(ds_decompress_quicklz_ctxt, ds_ctxt);

    ds_decompress_lz4_threads = opt_decompress_threads;
    ds_decompress_lz4_ctxt = ds_create(".", DS_TYPE_DECOMPRESS_LZ4);
    if (ds_decompress_lz4_ctxt == NULL) {
      ret = 1;
      goto exit;
    }
    ds_set_pipe(ds_decompress_lz4_ctxt, ds_ctxt);

    ds_decompress_zstd_ctxt = ds_create(".", DS_TYPE_DECOMPRESS_ZSTD);
    if (ds_decompress_zstd_ctxt == NULL) {
      ret = 1;
      goto exit;
    }
    ds_set_pipe(ds_decompress_zstd_ctxt, ds_ctxt);
  }

  if (opt_encrypt_algo) {
    ds_encrypt_algo = opt_encrypt_algo;
    ds_encrypt_key = opt_encrypt_key;
    ds_encrypt_key_file = opt_encrypt_key_file;
    ds_decrypt_encrypt_threads = opt_encrypt_threads;
    ds_decrypt_uncompressed_ctxt = ds_create(".", DS_TYPE_DECRYPT);
    ds_set_pipe(ds_decrypt_uncompressed_ctxt, ds_ctxt);
    if (ds_decrypt_uncompressed_ctxt == NULL) {
      ret = 1;
      goto exit;
    }
    if (ds_decompress_quicklz_ctxt) {
      ds_decrypt_quicklz_ctxt = ds_create(".", DS_TYPE_DECRYPT);
      ds_set_pipe(ds_decrypt_quicklz_ctxt, ds_decompress_quicklz_ctxt);
    }
    if (ds_decompress_lz4_ctxt) {
      ds_decrypt_lz4_ctxt = ds_create(".", DS_TYPE_DECRYPT);
      ds_set_pipe(ds_decrypt_lz4_ctxt, ds_decompress_lz4_ctxt);
    }
    if (ds_decompress_zstd_ctxt) {
      ds_decrypt_zstd_ctxt = ds_create(".", DS_TYPE_DECRYPT);
      ds_set_pipe(ds_decrypt_zstd_ctxt, ds_decompress_zstd_ctxt);
    }
  }
  if (opt_fifo_streams > 1) {
    for (int idx = 0; idx < opt_fifo_streams; idx++) {
      char filename[FN_REFLEN];
      snprintf(filename, sizeof(filename), "%s%s%lu", opt_fifo_dir, "/thread_",
               (ulong)idx);
      stream = xb_stream_read_new_fifo(filename, opt_fifo_timeout);
      if (stream == nullptr) {
        msg_ts(
            "%s: xb_stream_read_new_fifo() failed for thread %d. Possibly "
            "sender "
            "did "
            "not start.\n",
            my_progname, idx);
        ret = 1;
        goto exit;
      }
      streams->push_back(stream);
    }
  } else {
    stream = xb_stream_read_new_stdin();
    if (stream == NULL) {
      msg("%s: xb_stream_read_new_stdin() failed.\n", my_progname);
      ret = 1;
      goto exit;
    }
    streams->push_back(stream);
  }

  data_threads = (extract_ctxt_t *)my_malloc(
      PSI_NOT_INSTRUMENTED, sizeof(extract_ctxt_t) * (n_threads + 1),
      MYF(MY_FAE));
  for (int i = 0; i < n_threads; i++) {
    data_threads[i].thread_id = i;
    data_threads[i].filehash = filehash;
    data_threads[i].ds_ctxt = ds_ctxt;
    data_threads[i].ds_decompress_quicklz_ctxt = ds_decompress_quicklz_ctxt;
    data_threads[i].ds_decompress_lz4_ctxt = ds_decompress_lz4_ctxt;
    data_threads[i].ds_decompress_zstd_ctxt = ds_decompress_zstd_ctxt;
    data_threads[i].ds_decrypt_uncompressed_ctxt = ds_decrypt_uncompressed_ctxt;
    data_threads[i].ds_decrypt_quicklz_ctxt = ds_decrypt_quicklz_ctxt;
    data_threads[i].ds_decrypt_lz4_ctxt = ds_decrypt_lz4_ctxt;
    data_threads[i].ds_decrypt_zstd_ctxt = ds_decrypt_zstd_ctxt;
    data_threads[i].mutex = &mutex;
    data_threads[i].has_errors = &has_errors;
    data_threads[i].streams = streams;

    threads.push_back(
        std::thread(extract_worker_thread_func, std::ref(data_threads[i])));
  }

  for (i = 0; i < n_threads; i++) threads.at(i).join();

  if (has_errors.load()) ret = 1;

exit:

  for (uint i = 0; i < (uint)n_threads; i++) {
    char filename[FN_REFLEN];
    snprintf(filename, sizeof(filename), "%s%s%lu", opt_fifo_dir, "/thread_",
             (ulong)i);
    unlink(filename);
  }
  delete filehash;

  if (ds_ctxt != NULL) {
    ds_destroy(ds_ctxt);
  }
  if (ds_decrypt_uncompressed_ctxt != NULL) {
    ds_destroy(ds_decrypt_uncompressed_ctxt);
  }
  if (ds_decrypt_lz4_ctxt != NULL) {
    ds_destroy(ds_decrypt_lz4_ctxt);
  }
  if (ds_decrypt_zstd_ctxt != NULL) {
    ds_destroy(ds_decrypt_zstd_ctxt);
  }
  if (ds_decrypt_quicklz_ctxt != NULL) {
    ds_destroy(ds_decrypt_quicklz_ctxt);
  }
  if (ds_decompress_lz4_ctxt != NULL) {
    ds_destroy(ds_decompress_lz4_ctxt);
  }
  if (ds_decompress_zstd_ctxt != NULL) {
    ds_destroy(ds_decompress_zstd_ctxt);
  }
  if (ds_decompress_quicklz_ctxt != NULL) {
    ds_destroy(ds_decompress_quicklz_ctxt);
  }

  if (data_threads != nullptr) {
    my_free(data_threads);
  }

  std::for_each(streams->begin(), streams->end(),
                [](xb_rstream_t *s) { xb_stream_read_done(s); });
  delete streams;
  if (ret) {
    msg("exit code: %d\n", ret);
  }

  return ret;
}
