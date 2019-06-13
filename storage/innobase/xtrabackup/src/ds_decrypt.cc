/******************************************************
Copyright (c) 2017, 2019 Percona LLC and/or its affiliates.

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
#include <my_byteorder.h>
#include <my_io.h>
#include <mysql/service_mysql_alloc.h>
#include <cinttypes>
#include "common.h"
#include "crc_glue.h"
#include "datasink.h"
#include "ds_istream.h"
#include "thread_pool.h"
#include "xbcrypt.h"
#include "xbcrypt_common.h"

struct decrypt_thread_ctxt_t {
  const uchar *from{nullptr};
  size_t from_len{0};
  uchar *to{nullptr};
  size_t to_len{0};
  size_t to_size{0};
  const uchar *iv{nullptr};
  size_t iv_len{0};
  bool hash_appended{false};
  gcry_cipher_hd_t cipher_handle{nullptr};
  bool failed{false};
  decrypt_thread_ctxt_t() {
    if (xb_crypt_cipher_open(&cipher_handle)) {
      xb_a(0);
    }
  }
  decrypt_thread_ctxt_t(const decrypt_thread_ctxt_t &thd) = delete;
  decrypt_thread_ctxt_t(decrypt_thread_ctxt_t &&thd) { *this = std::move(thd); }
  decrypt_thread_ctxt_t &operator=(decrypt_thread_ctxt_t &&thd) {
    from = thd.from;
    from_len = thd.from_len;
    to = thd.to;
    to_len = thd.to_len;
    to_size = thd.to_size;
    iv = thd.iv;
    iv_len = thd.iv_len;
    hash_appended = thd.hash_appended;
    cipher_handle = thd.cipher_handle;
    failed = thd.failed;
    thd.cipher_handle = nullptr;
    thd.to = nullptr;
    thd.to_size = 0;
    return *this;
  }
  ~decrypt_thread_ctxt_t() {
    if (cipher_handle != nullptr) {
      xb_crypt_cipher_close(cipher_handle);
    }
    my_free(to);
  }
  void resize_to(size_t n) {
    if (to_size < n) {
      to = static_cast<uchar *>(my_realloc(PSI_NOT_INSTRUMENTED, to, n,
                                           MYF(MY_FAE | MY_ALLOW_ZERO_PTR)));
      to_size = n;
    }
  }
};

class Xbcrypt_stream {
 public:

  void reset() { stream.reset(); }

  void set_buffer(const char *buf, size_t len) { stream.add_buffer(buf, len); }

  xb_rcrypt_result_t next_chunk(decrypt_thread_ctxt_t *thd);

  bool empty() const { return stream.empty(); };

 private:
  Datasink_istream stream;
};

typedef struct {
  Thread_pool *thread_pool;
  int encrypt_algo;
  size_t chunk_size;
} ds_decrypt_ctxt_t;

typedef struct {
  ds_decrypt_ctxt_t *crypt_ctxt;
  size_t bytes_processed;
  ds_file_t *dest_file;
  Xbcrypt_stream stream;
  std::vector<std::future<void>> tasks;
  std::vector<decrypt_thread_ctxt_t> contexts;
} ds_decrypt_file_t;

uint ds_decrypt_encrypt_threads = 1;
bool ds_decrypt_modify_file_extension = true;

static ds_ctxt_t *decrypt_init(const char *root);
static ds_file_t *decrypt_open(ds_ctxt_t *ctxt, const char *path,
                               MY_STAT *mystat);
static int decrypt_write(ds_file_t *file, const void *buf, size_t len);
static int decrypt_close(ds_file_t *file);
static void decrypt_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_decrypt = {&decrypt_init, &decrypt_open, &decrypt_write,
                               &decrypt_close, &decrypt_deinit};

static ds_ctxt_t *decrypt_init(const char *root) {
  if (xb_crypt_init(NULL)) {
    return NULL;
  }

  ds_decrypt_ctxt_t *decrypt_ctxt = new ds_decrypt_ctxt_t;
  decrypt_ctxt->thread_pool = new Thread_pool(ds_decrypt_encrypt_threads);

  ds_ctxt_t *ctxt = new ds_ctxt_t;
  ctxt->ptr = decrypt_ctxt;
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *decrypt_open(ds_ctxt_t *ctxt, const char *path,
                               MY_STAT *mystat) {
  char new_name[FN_REFLEN];
  const char *used_name = path;
  const char *xbcrypt_ext_pos;

  xb_ad(ctxt->pipe_ctxt != NULL);
  ds_ctxt_t *dest_ctxt = ctxt->pipe_ctxt;

  ds_decrypt_ctxt_t *crypt_ctxt = static_cast<ds_decrypt_ctxt_t *>(ctxt->ptr);

  /* xtrabackup and xbstream rely on fact that extension is appended on
     encryption and removed on decryption.
     That works well with piping compression and excryption datasinks,
     hinting on how to access contents of the file when it is needed.
     However, xbcrypt (and its users) does not expect such magic,
     and extension is set manually by the user or caller script.
     Here implicit extension modification causes more trouble than good.

     See also ds_encrypt_modify_file_extension . */
  if (ds_decrypt_modify_file_extension) {
    /* Remove the .xbcrypt extension from the filename */
    if ((xbcrypt_ext_pos = strstr(path, ".xbcrypt"))) {
      strncpy(new_name, path, xbcrypt_ext_pos - path);
      new_name[xbcrypt_ext_pos - path] = 0;
      used_name = new_name;
    }
  }

  ds_file_t *dest_file = ds_open(dest_ctxt, used_name, mystat);
  if (dest_file == NULL) {
    msg("decrypt: ds_open(\"%s\") failed.\n", used_name);
    return nullptr;
  }

  ds_decrypt_file_t *crypt_file = new ds_decrypt_file_t;
  crypt_file->dest_file = dest_file;
  crypt_file->crypt_ctxt = crypt_ctxt;
  const size_t max_tasks = ds_decrypt_encrypt_threads * 8;
  crypt_file->tasks.resize(max_tasks);
  crypt_file->contexts.reserve(max_tasks);
  for (size_t i = 0; i < max_tasks; i++) {
    crypt_file->contexts.emplace_back();
  }

  ds_file_t *file = new ds_file_t;
  file->ptr = crypt_file;
  file->path = crypt_file->dest_file->path;

  return file;
}

xb_rcrypt_result_t Xbcrypt_stream::next_chunk(decrypt_thread_ctxt_t *thd) {
  const char *ptr;
  uint version;
  uint32_t checksum, checksum_exp;
  uint64_t tmp;

  stream.save_pos();

  ptr = stream.ptr(XB_CRYPT_CHUNK_MAGIC_SIZE);
  if (ptr == nullptr) {
    stream.restore_pos();
    return XB_CRYPT_READ_INCOMPLETE;
  }

  if (memcmp(ptr, XB_CRYPT_CHUNK_MAGIC3, XB_CRYPT_CHUNK_MAGIC_SIZE) == 0) {
    version = 3;
  } else if (memcmp(ptr, XB_CRYPT_CHUNK_MAGIC2, XB_CRYPT_CHUNK_MAGIC_SIZE) ==
             0) {
    version = 2;
  } else if (memcmp(ptr, XB_CRYPT_CHUNK_MAGIC1, XB_CRYPT_CHUNK_MAGIC_SIZE) ==
             0) {
    version = 1;
  } else {
    msg("%s:%s: wrong chunk magic.\n", my_progname, __FUNCTION__);
    return XB_CRYPT_READ_ERROR;
  }

  /* reserved */
  if (!stream.read_u64_le(&tmp)) {
    stream.restore_pos();
    return XB_CRYPT_READ_INCOMPLETE;
  }

  /* original size */
  if (!stream.read_u64_le(&tmp)) {
    stream.restore_pos();
    return XB_CRYPT_READ_INCOMPLETE;
  }
  if (tmp > INT_MAX) {
    msg("%s:%s: invalid original size.\n", my_progname, __FUNCTION__);
    return XB_CRYPT_READ_ERROR;
  }
  thd->to_len = (size_t)tmp;

  /* encrypted size */
  if (!stream.read_u64_le(&tmp)) {
    stream.restore_pos();
    return XB_CRYPT_READ_INCOMPLETE;
  }
  if (tmp > INT_MAX) {
    msg("%s:%s: invalid encrypted size.\n", my_progname, __FUNCTION__);
    return XB_CRYPT_READ_ERROR;
  }
  thd->from_len = (size_t)tmp;

  xb_a(thd->from_len <= thd->to_len + XB_CRYPT_HASH_LEN);

  /* checksum */
  if (!stream.read_u32_le(&checksum_exp)) {
    stream.restore_pos();
    return XB_CRYPT_READ_INCOMPLETE;
  }

  /* iv size */
  if (version == 1) {
    thd->iv_len = 0;
    thd->iv = NULL;
  } else {
    if (!stream.read_u64_le(&tmp)) {
      stream.restore_pos();
      return XB_CRYPT_READ_INCOMPLETE;
    }
    if (tmp > INT_MAX) {
      msg("%s:%s: invalid iv size.\n", my_progname, __FUNCTION__);
      return XB_CRYPT_READ_ERROR;
    }
    thd->iv_len = (size_t)tmp;
  }

  if (thd->iv_len > 0) {
    thd->iv = reinterpret_cast<const uchar *>(stream.ptr(thd->iv_len));
    if (thd->iv == nullptr) {
      stream.restore_pos();
      return XB_CRYPT_READ_INCOMPLETE;
    }
  }

  /* for version 2 we need to read in the iv data but do not init
  CTR with it */
  if (version == 2) {
    thd->iv_len = 0;
    thd->iv = 0;
  }

  if (thd->from_len > 0) {
    thd->from = reinterpret_cast<const uchar *>(stream.ptr(thd->from_len));
    if (thd->from == nullptr) {
      stream.restore_pos();
      return XB_CRYPT_READ_INCOMPLETE;
    }
  }

  xb_ad(thd->from_len <= thd->to_len);

  checksum = crc32_iso3309(0, thd->from, thd->from_len);
  if (checksum != checksum_exp) {
    msg("%s:%s invalid checksum, expected 0x%" PRIx32 ", actual 0x%" PRIx32
        ".\n",
        my_progname, __FUNCTION__, checksum_exp, checksum);
    return XB_CRYPT_READ_ERROR;
  }

  thd->hash_appended = version > 2;
  thd->resize_to(thd->to_len + XB_CRYPT_HASH_LEN);

  return XB_CRYPT_READ_CHUNK;
}

static int decrypt_write(ds_file_t *file, const void *buf, size_t len) {
  ds_decrypt_file_t *crypt_file = (ds_decrypt_file_t *)file->ptr;
  ds_decrypt_ctxt_t *crypt_ctxt = crypt_file->crypt_ctxt;
  xb_rcrypt_result_t r = XB_CRYPT_READ_CHUNK;
  bool err = false;

  crypt_file->stream.set_buffer(static_cast<const char *>(buf), len);

  uint i = 0;

  do {
    auto &thd = crypt_file->contexts[i];

    r = crypt_file->stream.next_chunk(&thd);

    if (r == XB_CRYPT_READ_CHUNK) {
      crypt_file->tasks[i] =
          crypt_ctxt->thread_pool->add_task([&thd](size_t n) {
            if (xb_crypt_decrypt(thd.cipher_handle, thd.from, thd.from_len,
                                 thd.to, &thd.to_len, thd.iv, thd.iv_len,
                                 thd.hash_appended)) {
              thd.failed = true;
            }
          });

      ++i;
    } else if (r == XB_CRYPT_READ_INCOMPLETE) {
      /* do nothing */
    } else /* r == XB_CRYPT_READ_ERROR */ {
      err = true;
    }

    if (i >= crypt_file->tasks.size() || r != XB_CRYPT_READ_CHUNK) {
      for (uint k = 0; k < i; ++k) {
        auto &thd = crypt_file->contexts[k];

        /* reap */
        crypt_file->tasks[k].wait();

        if (thd.failed) {
          msg("decrypt: failed to decrypt chunk.\n");
          err = true;
        }

        if (!err) {
          xb_a(thd.to_len > 0);

          if (!err && ds_write(crypt_file->dest_file, thd.to, thd.to_len)) {
            msg("decrypt: write to destination failed.\n");
            err = true;
          }

          crypt_file->bytes_processed += thd.from_len;
        }
      }
      i = 0;
    }
  } while (r == XB_CRYPT_READ_CHUNK);

  crypt_file->stream.reset();

  return err ? 1 : 0;
}

static int decrypt_close(ds_file_t *file) {
  ds_decrypt_file_t *crypt_file;
  ds_file_t *dest_file;
  int rc = 0;

  crypt_file = (ds_decrypt_file_t *)file->ptr;
  dest_file = crypt_file->dest_file;

  if (!crypt_file->stream.empty()) {
    msg("decrypt: unprocessed data left in the buffer.\n");
    rc = 2;
  }

  if (ds_close(dest_file)) {
    msg("decrypt: failed to close dest file.\n");
    rc = 1;
  }

  delete crypt_file;
  delete file;

  return rc;
}

static void decrypt_deinit(ds_ctxt_t *ctxt) {
  xb_ad(ctxt->pipe_ctxt != nullptr);

  ds_decrypt_ctxt_t *crypt_ctxt = static_cast<ds_decrypt_ctxt_t *>(ctxt->ptr);

  delete crypt_ctxt->thread_pool;

  my_free(ctxt->root);

  delete crypt_ctxt;
  delete ctxt;
}
