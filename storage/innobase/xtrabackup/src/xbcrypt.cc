/******************************************************
Copyright (c) 2013-2023 Percona LLC and/or its affiliates.

The xbcrypt utility: decrypt files in the XBCRYPT format.

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

#include "xbcrypt.h"
#include <my_base.h>
#include <my_dir.h>
#include <my_getopt.h>
#include <mysql/service_mysql_alloc.h>
#include <typelib.h>
#include "common.h"
#include "crc_glue.h"
#include "datasink.h"
#include "ds_decrypt.h"
#include "ds_encrypt.h"
#include "msg.h"
#include "nulls.h"
#include "xbcrypt_common.h"
#include "xtrabackup_version.h"

#define XBCRYPT_VERSION XTRABACKUP_VERSION
#define XBCRYPT_REVISION XTRABACKUP_REVISION


typedef enum { RUN_MODE_NONE, RUN_MODE_ENCRYPT, RUN_MODE_DECRYPT } run_mode_t;

const char *xbcrypt_encrypt_algo_names[] = {"NONE", "AES128", "AES192",
                                            "AES256", NullS};
TYPELIB xbcrypt_encrypt_algo_typelib = {
    array_elements(xbcrypt_encrypt_algo_names) - 1, "",
    xbcrypt_encrypt_algo_names, NULL};

static run_mode_t opt_run_mode = RUN_MODE_ENCRYPT;
static char *opt_input_file = NULL;
static char *opt_output_file = NULL;
static ulong opt_encrypt_algo;
static char *opt_encrypt_key_file = NULL;
static char *opt_encrypt_key = NULL;
static ulonglong opt_encrypt_chunk_size = 0;
static bool opt_verbose = false;
static uint opt_encrypt_threads = 1;
static uint opt_read_buffer_size = 0;

static struct my_option my_long_options[] = {
    {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
     0, 0, 0, 0, 0},

    {"version", 'V', "Display version and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
     0, 0, 0, 0, 0, 0},

    {"decrypt", 'd', "Decrypt data input to output.", 0, 0, 0, GET_NO_ARG,
     NO_ARG, 0, 0, 0, 0, 0, 0},

    {"input", 'i',
     "Optional input file. If not specified, input"
     " will be read from standard input.",
     &opt_input_file, &opt_input_file, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},

    {"output", 'o',
     "Optional output file. If not specified, output"
     " will be written to standard output.",
     &opt_output_file, &opt_output_file, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},

    {"encrypt-algo", 'a', "Encryption algorithm.", &opt_encrypt_algo,
     &opt_encrypt_algo, &xbcrypt_encrypt_algo_typelib, GET_ENUM, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},

    {"encrypt-key", 'k', "Encryption key", 0, 0, 0, GET_STR_ALLOC, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},

    {"encrypt-key-file", 'f', "File which contains encryption key.",
     &opt_encrypt_key_file, &opt_encrypt_key_file, 0, GET_STR_ALLOC,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"encrypt-chunk-size", 's',
     "Size of working buffer for encryption in"
     " bytes. The default value is 64K.",
     &opt_encrypt_chunk_size, &opt_encrypt_chunk_size, 0, GET_ULL, REQUIRED_ARG,
     (1 << 16), 1024, ULLONG_MAX, 0, 0, 0},

    {"encrypt-threads", 't',
     "Number of threads for parallel data encryption/decryption. "
     "The default value is 1",
     &opt_encrypt_threads, &opt_encrypt_threads, 0, GET_UINT, OPT_ARG, 1, 1,
     UINT_MAX, 0, 0, 0},

    {"read-buffer-size", 'r', "Read buffer size. The defaut value is 10Mb.",
     &opt_read_buffer_size, &opt_read_buffer_size, 0, GET_UINT, OPT_ARG,
     10 * 1024 * 1024, 1, UINT_MAX, 0, 0, 0},

    {"verbose", 'v', "Display verbose status output.", &opt_verbose,
     &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

/* Following definitions are to avoid linking with unused datasinks
   and their link dependencies */
datasink_t datasink_archive;
datasink_t datasink_xbstream;
datasink_t datasink_compress;
datasink_t datasink_compress_lz4;
datasink_t datasink_compress_zstd;
datasink_t datasink_decompress;
datasink_t datasink_decompress_lz4;
datasink_t datasink_decompress_zstd;
datasink_t datasink_tmpfile;
datasink_t datasink_buffer;
datasink_t datasink_fifo;

static int get_options(int *argc, char ***argv);

static bool get_one_option(int optid,
                           const struct my_option *opt __attribute__((unused)),
                           char *argument __attribute__((unused)));

static void print_version(void);

static void usage(void);

static int process(File filein, ds_file_t *fileout, const char *action);

int main(int argc, char **argv) {
  MY_STAT mystat;
  File filein = 0;
  ds_ctxt_t *datasink = NULL;
  ds_ctxt_t *output_ds = NULL;
  ds_ctxt_t *crypto_ds = NULL;
  ds_file_t *fileout = NULL;
  char output_file_buf[FN_REFLEN] = {"stdout"};
  const char *output_file = output_file_buf;
  int result = EXIT_FAILURE;

  MY_INIT(argv[0]);

  if (get_options(&argc, &argv)) {
    goto cleanup;
  }
  if (opt_encrypt_key == nullptr) {
    get_env_value(opt_encrypt_key, "XBCRYPT_ENCRYPTION_KEY");
  }

  xb_libgcrypt_init();

  crc_init();

  if (opt_input_file) {
    MY_STAT input_file_stat;

    if (opt_verbose)
      msg("%s: input file \"%s\".\n", my_progname, opt_input_file);

    if (my_stat(opt_input_file, &input_file_stat, MYF(MY_WME)) == NULL) {
      goto cleanup;
    }
    if (!MY_S_ISREG(input_file_stat.st_mode)) {
      msg("%s: \"%s\" is not a regular file, exiting.\n", my_progname,
          opt_input_file);
      goto cleanup;
    }
    if ((filein = my_open(opt_input_file, O_RDONLY, MYF(MY_WME))) < 0) {
      msg("%s: failed to open \"%s\".\n", my_progname, opt_input_file);
      goto cleanup;
    }
  } else {
    if (opt_verbose) msg("%s: input from standard input.\n", my_progname);
    filein = fileno(stdin);
  }

  if (opt_output_file) {
    output_file = opt_output_file;
    char dirpath[FN_REFLEN];
    size_t dirpath_len;

    if (opt_verbose) msg("%s: output file \"%s\".\n", my_progname, output_file);

    dirname_part(dirpath, output_file, &dirpath_len);
    output_ds = ds_create(dirpath, DS_TYPE_LOCAL);
  } else {
    if (opt_verbose) msg("%s: output to standard output.\n", my_progname);
    output_ds = ds_create(".", DS_TYPE_STDOUT);
  }
  if (!output_ds) {
    msg("%s: failed to create datasink.\n", my_progname);
    goto cleanup;
  }

  ds_encrypt_algo = opt_encrypt_algo;
  ds_encrypt_key = opt_encrypt_key;
  ds_encrypt_key_file = opt_encrypt_key_file;

  if (opt_run_mode == RUN_MODE_DECRYPT) {
    ds_decrypt_encrypt_threads = opt_encrypt_threads;
    ds_decrypt_modify_file_extension = false;
    if (!(crypto_ds = ds_create(".", DS_TYPE_DECRYPT))) {
      goto cleanup;
    }
  } else if (opt_run_mode == RUN_MODE_ENCRYPT) {
    ds_encrypt_encrypt_chunk_size = opt_encrypt_chunk_size;
    ds_encrypt_encrypt_threads = opt_encrypt_threads;
    ds_encrypt_modify_file_extension = false;
    if (!(crypto_ds = ds_create(".", DS_TYPE_ENCRYPT))) {
      goto cleanup;
    }
  } else {
    msg("%s unknown run_mode", my_progname);
    goto cleanup;
  }

  ds_set_pipe(crypto_ds, output_ds);
  datasink = crypto_ds;

  memset(&mystat, 0, sizeof(mystat));
  fileout = ds_open(datasink, output_file, &mystat);
  if (!fileout) {
    msg("%s failed to create output file: %s\n", my_progname, output_file);
    goto cleanup;
  }
  if (process(filein, fileout,
              opt_run_mode == RUN_MODE_DECRYPT ? "decrypt" : "encrypt")) {
    goto cleanup;
  }

  result = EXIT_SUCCESS;

cleanup:
  if (opt_input_file && filein) {
    my_close(filein, MYF(MY_WME));
  }

  if (fileout && ds_close(fileout)) {
    result = EXIT_FAILURE;
  }
  if (crypto_ds) {
    ds_destroy(crypto_ds);
  }

  if (output_ds) {
    ds_destroy(output_ds);
  }

  my_cleanup_options(my_long_options);
  my_free(opt_encrypt_key);

  my_end(0);

  exit(result);
}

static int process(File filein, ds_file_t *fileout, const char *action) {
  size_t bytesread = 0;
  uchar *chunkbuf = NULL;
  ulonglong ttlchunkswritten = 0;
  ulonglong ttlbyteswritten = 0;

  chunkbuf = (uchar *)my_malloc(PSI_NOT_INSTRUMENTED, opt_read_buffer_size,
                                MYF(MY_FAE));
  while (true) {
    bytesread = my_read(filein, chunkbuf, opt_read_buffer_size, MYF(MY_WME));
    if (bytesread == 0) {
      break;
    }

    if (bytesread == MY_FILE_ERROR) {
      msg("%s failed to %s: can't read input.\n", my_progname, action);
      goto err;
    }

    if (ds_write(fileout, chunkbuf, bytesread)) {
      msg("%s failed to %s: can't write to output.\n", my_progname, action);
      goto err;
    }

    ttlchunkswritten++;
    ttlbyteswritten += bytesread;

    if (opt_verbose)
      msg("%s: %s: %llu chunks written, %llu bytes "
          "written\n.",
          my_progname, action, ttlchunkswritten, ttlbyteswritten);
  }
  my_free(chunkbuf);
  return 0;

err:
  my_free(chunkbuf);
  return 1;
}

static int get_options(int *argc, char ***argv) {
  int ho_error;

  if ((ho_error =
           handle_options(argc, argv, my_long_options, get_one_option))) {
    exit(EXIT_FAILURE);
  }

  return (0);
}

static bool get_one_option(int optid,
                           const struct my_option *opt __attribute__((unused)),
                           char *argument __attribute__((unused))) {
  switch (optid) {
    case 'd':
      opt_run_mode = RUN_MODE_DECRYPT;
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

  return (false);
}

static void print_version(void) {
  printf("%s  Ver %s%s for %s (%s) (revision id: %s)\n", my_progname,
         XBCRYPT_VERSION, get_suffix_str().c_str(), SYSTEM_TYPE, MACHINE_TYPE,
         XBCRYPT_REVISION);
}

static void usage(void) {
  print_version();
  puts("Copyright (C) 2011-2018 Percona Inc.");
  puts(
      "This software comes with ABSOLUTELY NO WARRANTY. "
      "This is free software,\nand you are welcome to modify and "
      "redistribute it under the GPL license.\n");

  puts("Encrypt or decrypt files in the XBCRYPT format.\n");

  puts("Usage: ");
  printf(
      "  %s [OPTIONS...]"
      " # read data from specified input, encrypting or decrypting "
      " and writing the result to the specified output.\n",
      my_progname);
  puts("\nOptions:");
  my_print_help(my_long_options);
}
