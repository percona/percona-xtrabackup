/******************************************************
Copyright (c) 2013-2019 Percona LLC and/or its affiliates.

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

#include <my_base.h>
#include <my_io.h>
#include <mysql/service_mysql_alloc.h>
#include "common.h"
#include "datasink.h"
#include "thread_pool.h"
#include "xbcrypt.h"
#include "xbcrypt_common.h"

#define XB_CRYPT_CHUNK_SIZE ((size_t)(ds_encrypt_encrypt_chunk_size))

struct encrypt_thread_ctxt_t {
  const uchar *from{nullptr};
  size_t from_len{0};
  uchar *to{nullptr};
  uchar *iv{nullptr};
  size_t to_size{0};
  size_t to_len{0};
  gcry_cipher_hd_t cipher_handle{nullptr};
  encrypt_thread_ctxt_t() {
    if (xb_crypt_cipher_open(&cipher_handle)) {
      xb_a(0);
    }
  }
  encrypt_thread_ctxt_t(const encrypt_thread_ctxt_t &thd) = delete;
  encrypt_thread_ctxt_t(encrypt_thread_ctxt_t &&thd) { *this = std::move(thd); }
  encrypt_thread_ctxt_t &operator=(encrypt_thread_ctxt_t &&thd) {
    from = thd.from;
    from_len = thd.from_len;
    to = thd.to;
    to_len = thd.to_len;
    to_size = thd.to_size;
    iv = thd.iv;
    cipher_handle = thd.cipher_handle;
    thd.cipher_handle = nullptr;
    return *this;
  }
  ~encrypt_thread_ctxt_t() {
    if (cipher_handle != nullptr) {
      xb_crypt_cipher_close(cipher_handle);
    }
  }
};

typedef struct {
  Thread_pool *thread_pool;
} ds_encrypt_ctxt_t;

typedef struct {
  xb_wcrypt_t *xbcrypt_file;
  ds_encrypt_ctxt_t *crypt_ctxt;
  size_t bytes_processed;
  ds_file_t *dest_file;
  char *crypt_buf;
  size_t crypt_buf_size;
  char *iv_buf;
  size_t iv_buf_size;
  std::vector<std::future<void>> tasks;
  std::vector<encrypt_thread_ctxt_t> contexts;
} ds_encrypt_file_t;

/* Encryption options */
uint ds_encrypt_encrypt_threads;
ulonglong ds_encrypt_encrypt_chunk_size;
bool ds_encrypt_modify_file_extension = true;

static ds_ctxt_t *encrypt_init(const char *root);
static ds_file_t *encrypt_open(ds_ctxt_t *ctxt, const char *path,
                               MY_STAT *mystat);
static int encrypt_write(ds_file_t *file, const void *buf, size_t len);
static int encrypt_close(ds_file_t *file);
static void encrypt_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_encrypt = {&encrypt_init, &encrypt_open, &encrypt_write,
                               &encrypt_close, &encrypt_deinit};

static uint encrypt_iv_len = 0;

static ssize_t my_xb_crypt_write_callback(void *userdata, const void *buf,
                                          size_t len) {
  ds_encrypt_file_t *encrypt_file;

  encrypt_file = (ds_encrypt_file_t *)userdata;

  xb_ad(encrypt_file != NULL);
  xb_ad(encrypt_file->dest_file != NULL);

  if (!ds_write(encrypt_file->dest_file, buf, len)) {
    return len;
  }
  return -1;
}

static ds_ctxt_t *encrypt_init(const char *root) {
  if (xb_crypt_init(&encrypt_iv_len)) {
    return NULL;
  }

  ds_ctxt_t *ctxt = new ds_ctxt_t;

  ds_encrypt_ctxt_t *encrypt_ctxt = new ds_encrypt_ctxt_t;
  encrypt_ctxt->thread_pool = new Thread_pool(ds_encrypt_encrypt_threads);

  ctxt->ptr = encrypt_ctxt;
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *encrypt_open(ds_ctxt_t *ctxt, const char *path,
                               MY_STAT *mystat) {
  char new_name[FN_REFLEN];
  const char *used_name;

  xb_ad(ctxt->pipe_ctxt != NULL);
  ds_ctxt_t *dest_ctxt = ctxt->pipe_ctxt;

  ds_encrypt_ctxt_t *crypt_ctxt = (ds_encrypt_ctxt_t *)ctxt->ptr;

  ds_file_t *file = new ds_file_t;
  ds_encrypt_file_t *crypt_file = new ds_encrypt_file_t;

  /* xtrabackup and xbstream rely on fact that extension is appended on
     encryption and removed on decryption.
     That works well with piping compression and excryption datasinks,
     hinting on how to access contents of the file when it is needed.
     However, xbcrypt (and its users) does not expect such magic,
     and extension is set manually by the user or caller script.
     Here implicit extension modification causes more trouble than good.

     See also ds_decrypt_modify_file_extension . */
  if (ds_encrypt_modify_file_extension) {
    /* Append the .xbcrypt extension to the filename */
    fn_format(new_name, path, "", ".xbcrypt", MYF(MY_APPEND_EXT));
    used_name = new_name;
  } else {
    used_name = path;
  }

  crypt_file->dest_file = ds_open(dest_ctxt, used_name, mystat);
  if (crypt_file->dest_file == NULL) {
    msg("encrypt: ds_open(\"%s\") failed.\n", used_name);
    goto err;
  }

  crypt_file->crypt_buf = nullptr;
  crypt_file->crypt_buf_size = 0;
  crypt_file->iv_buf = nullptr;
  crypt_file->iv_buf_size = 0;
  crypt_file->crypt_ctxt = crypt_ctxt;
  crypt_file->xbcrypt_file =
      xb_crypt_write_open(crypt_file, my_xb_crypt_write_callback);

  if (crypt_file->xbcrypt_file == NULL) {
    msg("encrypt: xb_crypt_write_open() failed.\n");
    goto err;
  }

  file->ptr = crypt_file;
  file->path = crypt_file->dest_file->path;

  return file;

err:
  if (crypt_file->dest_file) {
    ds_close(crypt_file->dest_file);
  }
  my_free(file);
  return NULL;
}

static int encrypt_write(ds_file_t *file, const void *buf, size_t len) {
  ds_encrypt_file_t *crypt_file = (ds_encrypt_file_t *)file->ptr;
  ds_encrypt_ctxt_t *crypt_ctxt = crypt_file->crypt_ctxt;

  /* make sure we have enough memory for encryption */
  const size_t crypt_size = XB_CRYPT_CHUNK_SIZE + XB_CRYPT_HASH_LEN;
  const size_t n_chunks =
      (len / XB_CRYPT_CHUNK_SIZE * XB_CRYPT_CHUNK_SIZE == len)
          ? (len / XB_CRYPT_CHUNK_SIZE)
          : (len / XB_CRYPT_CHUNK_SIZE + 1);
  const size_t crypt_buf_size = crypt_size * n_chunks;
  const size_t iv_buf_size = encrypt_iv_len * n_chunks;
  if (crypt_file->crypt_buf_size < crypt_buf_size) {
    crypt_file->crypt_buf = static_cast<char *>(
        my_realloc(PSI_NOT_INSTRUMENTED, crypt_file->crypt_buf, crypt_buf_size,
                   MYF(MY_FAE | MY_ALLOW_ZERO_PTR)));
  }
  if (crypt_file->iv_buf_size < iv_buf_size) {
    crypt_file->iv_buf = static_cast<char *>(
        my_realloc(PSI_NOT_INSTRUMENTED, crypt_file->iv_buf, iv_buf_size,
                   MYF(MY_FAE | MY_ALLOW_ZERO_PTR)));
  }

  /* parallel encrypt using trhead pool */
  if (crypt_file->tasks.size() < n_chunks) {
    crypt_file->tasks.resize(n_chunks);
  }
  if (crypt_file->contexts.size() < n_chunks) {
    size_t n = crypt_file->contexts.size();
    crypt_file->contexts.reserve(n_chunks);
    for (size_t i = n; i < n_chunks; i++) {
      crypt_file->contexts.emplace_back();
    }
  }

  for (size_t i = 0; i < n_chunks; ++i) {
    size_t chunk_len =
        std::min(len - i * XB_CRYPT_CHUNK_SIZE, XB_CRYPT_CHUNK_SIZE);
    auto &thd = crypt_file->contexts[i];
    thd.from = ((const uchar *)buf) + XB_CRYPT_CHUNK_SIZE * i;
    thd.from_len = chunk_len;
    thd.to_size = crypt_size;
    thd.to = (uchar *)(crypt_file->crypt_buf) + crypt_size * i;
    thd.iv = (uchar *)(crypt_file->iv_buf) + encrypt_iv_len * i;

    crypt_file->tasks[i] =
        crypt_ctxt->thread_pool->add_task([&thd](size_t thread_id) {
          if (xb_crypt_encrypt(thd.cipher_handle, thd.from, thd.from_len,
                               thd.to, &thd.to_len, thd.iv)) {
            thd.to_len = 0;
          }
        });
  }

  bool error = false;

  for (size_t i = 0; i < n_chunks; ++i) {
    const auto &thd = crypt_file->contexts[i];

    /* reap */
    crypt_file->tasks[i].wait();

    if (error) continue;

    if (thd.to_len == 0) {
      msg("encrypt: encryption failed.\n");
      error = true;
      continue;
    }

    if (xb_crypt_write_chunk(crypt_file->xbcrypt_file, thd.to,
                             thd.from_len + XB_CRYPT_HASH_LEN, thd.to_len,
                             thd.iv, encrypt_iv_len)) {
      msg("encrypt: write to the destination file failed.\n");
      error = true;
      continue;
    }

    crypt_file->bytes_processed += thd.from_len;
  }

  return error ? 1 : 0;
}

static int encrypt_close(ds_file_t *file) {
  ds_encrypt_file_t *crypt_file;
  ds_file_t *dest_file;
  int rc = 0;

  crypt_file = (ds_encrypt_file_t *)file->ptr;
  dest_file = crypt_file->dest_file;

  rc = xb_crypt_write_close(crypt_file->xbcrypt_file);

  if (ds_close(dest_file)) {
    rc = 1;
  }

  my_free(crypt_file->crypt_buf);
  my_free(crypt_file->iv_buf);

  delete crypt_file;
  delete file;

  return rc;
}

static void encrypt_deinit(ds_ctxt_t *ctxt) {
  xb_ad(ctxt->pipe_ctxt != NULL);

  ds_encrypt_ctxt_t *crypt_ctxt = (ds_encrypt_ctxt_t *)ctxt->ptr;
  delete crypt_ctxt->thread_pool;
  delete crypt_ctxt;

  my_free(ctxt->root);
  delete ctxt;
}
