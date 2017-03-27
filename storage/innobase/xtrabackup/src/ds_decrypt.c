/******************************************************
Copyright (c) 2017 Percona LLC and/or its affiliates.

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
#include "common.h"
#include "datasink.h"

#if GCC_VERSION >= 4002
/* Workaround to avoid "gcry_ac_* is deprecated" warnings in gcrypt.h */
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <gcrypt.h>

#if GCC_VERSION >= 4002
#  pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif

#include "xbcrypt.h"

#if !defined(GCRYPT_VERSION_NUMBER) || (GCRYPT_VERSION_NUMBER < 0x010600)
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif

typedef struct {
	pthread_t		id;
	uint			num;
	pthread_mutex_t 	ctrl_mutex;
	pthread_cond_t		ctrl_cond;
	pthread_mutex_t		data_mutex;
	pthread_cond_t  	data_cond;
	my_bool			started;
	my_bool			data_avail;
	my_bool			cancelled;
	my_bool			failed;
	const uchar 		*from;
	size_t			from_len;
	uchar			*to;
	size_t			to_len;
	size_t			to_size;
	const uchar		*iv;
	size_t			iv_len;
	unsigned long long	offset;
	my_bool			hash_appended;
	gcry_cipher_hd_t	cipher_handle;
	xb_rcrypt_result_t	parse_result;
} crypt_thread_ctxt_t;

typedef struct {
	crypt_thread_ctxt_t	*threads;
	uint			nthreads;
	int			encrypt_algo;
	size_t			chunk_size;
	char			*encrypt_key;
	char			*encrypt_key_file;
} ds_decrypt_ctxt_t;

typedef struct {
	ds_decrypt_ctxt_t	*crypt_ctxt;
	size_t			bytes_processed;
	ds_file_t		*dest_file;
	uchar			*buf;
	size_t			buf_len;
	size_t			buf_size;
} ds_decrypt_file_t;

/* Encryption options */
char		*ds_decrypt_encrypt_key = NULL;
char		*ds_decrypt_encrypt_key_file = NULL;
int		ds_decrypt_encrypt_threads = 1;
ulong		ds_decrypt_encrypt_algo;

static ds_ctxt_t *decrypt_init(const char *root);
static ds_file_t *decrypt_open(ds_ctxt_t *ctxt, const char *path,
				MY_STAT *mystat);
static int decrypt_write(ds_file_t *file, const void *buf, size_t len);
static int decrypt_close(ds_file_t *file);
static void decrypt_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_decrypt = {
	&decrypt_init,
	&decrypt_open,
	&decrypt_write,
	&decrypt_close,
	&decrypt_deinit
};

static crypt_thread_ctxt_t *create_worker_threads(uint n);
static void destroy_worker_threads(crypt_thread_ctxt_t *threads, uint n);
static void *decrypt_worker_thread_func(void *arg);

static const uint encrypt_mode = GCRY_CIPHER_MODE_CTR;
static uint encrypt_key_len = 0;
static size_t encrypt_iv_len = 0;

static
ds_ctxt_t *
decrypt_init(const char *root)
{
	ds_ctxt_t		*ctxt;
	ds_decrypt_ctxt_t	*decrypt_ctxt;
	crypt_thread_ctxt_t	*threads;
	gcry_error_t 		gcry_error;

	/* Acording to gcrypt docs (and my testing), setting up the threading
	   callbacks must be done first, so, lets give it a shot */
#if !defined(GCRYPT_VERSION_NUMBER) || (GCRYPT_VERSION_NUMBER < 0x010600)
	gcry_error = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	if (gcry_error) {
		msg("decrypt: unable to set libgcrypt thread cbs - "
		    "%s : %s\n",
		    gcry_strsource(gcry_error),
		    gcry_strerror(gcry_error));
		return NULL;
	}
#endif

	/* Version check should be the very next call because it
	makes sure that important subsystems are intialized. */
	if (!gcry_control(GCRYCTL_ANY_INITIALIZATION_P)) {
		const char	*gcrypt_version;
		gcrypt_version = gcry_check_version(NULL);
		/* No other library has already initialized libgcrypt. */
		if (!gcrypt_version) {
			msg("decrypt: failed to initialize libgcrypt\n");
			return NULL;
		} else {
			msg("decrypt: using gcrypt %s\n", gcrypt_version);
		}
	}

	/* Disable the gcry secure memory, not dealing with this for now */
	gcry_error = gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
	if (gcry_error) {
		msg("decrypt: unable to disable libgcrypt secmem - "
		    "%s : %s\n",
		    gcry_strsource(gcry_error),
		    gcry_strerror(gcry_error));
		return NULL;
	}

	/* Finalize gcry initialization. */
	gcry_error = gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
	if (gcry_error) {
		msg("decrypt: unable to finish libgcrypt initialization - "
		    "%s : %s\n",
		    gcry_strsource(gcry_error),
		    gcry_strerror(gcry_error));
		return NULL;
	}

	/* Set up the iv length */
	encrypt_iv_len = gcry_cipher_get_algo_blklen(ds_decrypt_encrypt_algo);
	xb_a(encrypt_iv_len > 0);

	/* Now set up the key */
	if (ds_decrypt_encrypt_key == NULL &&
	    ds_decrypt_encrypt_key_file == NULL) {
		msg("decrypt: no encryption key or key file specified.\n");
		return NULL;
	} else if (ds_decrypt_encrypt_key && ds_decrypt_encrypt_key_file) {
		msg("decrypt: both encryption key and key file specified.\n");
		return NULL;
	} else if (ds_decrypt_encrypt_key_file) {
		if (!xb_crypt_read_key_file(ds_decrypt_encrypt_key_file,
					    (void**)&ds_decrypt_encrypt_key,
					    &encrypt_key_len)) {
			msg("decrypt: unable to read encryption key file"
			    " \"%s\".\n", ds_decrypt_encrypt_key_file);
			return NULL;
		}
	} else if (ds_decrypt_encrypt_key) {
		encrypt_key_len = strlen(ds_decrypt_encrypt_key);
	} else {
		msg("decrypt: no encryption key or key file specified.\n");
		return NULL;
	}

	/* Create and initialize the worker threads */
	threads = create_worker_threads(ds_decrypt_encrypt_threads);
	if (threads == NULL) {
		msg("decrypt: failed to create worker threads.\n");
		return NULL;
	}

	ctxt = (ds_ctxt_t *) my_malloc(sizeof(ds_ctxt_t) +
				       sizeof(ds_decrypt_ctxt_t),
				       MYF(MY_FAE));

	decrypt_ctxt = (ds_decrypt_ctxt_t *) (ctxt + 1);
	decrypt_ctxt->threads = threads;
	decrypt_ctxt->nthreads = ds_decrypt_encrypt_threads;

	ctxt->ptr = decrypt_ctxt;
	ctxt->root = my_strdup(root, MYF(MY_FAE));

	return ctxt;
}

static
ds_file_t *
decrypt_open(ds_ctxt_t *ctxt, const char *path, MY_STAT *mystat)
{
	ds_ctxt_t		*dest_ctxt;

	ds_decrypt_ctxt_t	*crypt_ctxt;
	ds_decrypt_file_t	*crypt_file;

	char			new_name[FN_REFLEN];
	ds_file_t		*file;

	xb_ad(ctxt->pipe_ctxt != NULL);
	dest_ctxt = ctxt->pipe_ctxt;

	crypt_ctxt = (ds_decrypt_ctxt_t *) ctxt->ptr;


	file = (ds_file_t *) my_malloc(sizeof(ds_file_t) +
				       sizeof(ds_decrypt_file_t),
				       MYF(MY_FAE|MY_ZEROFILL));

	crypt_file = (ds_decrypt_file_t *) (file + 1);

	/* Remove the .xbcrypt extension from the filename */
	strcpy(new_name, path);
	new_name[strlen(new_name) - 8] = 0;
	crypt_file->dest_file = ds_open(dest_ctxt, new_name, mystat);
	if (crypt_file->dest_file == NULL) {
		msg("decrypt: ds_open(\"%s\") failed.\n", new_name);
		goto err;
	}

	crypt_file->crypt_ctxt = crypt_ctxt;
	crypt_file->buf = NULL;
	crypt_file->buf_size = 0;
	crypt_file->buf_len = 0;

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

#define CHECK_BUF_SIZE(ptr, size, buf, len) \
	if (ptr + size - buf > (ssize_t) len) { \
		result = XB_CRYPT_READ_INCOMPLETE; \
		goto exit; \
	}

static
xb_rcrypt_result_t
parse_xbcrypt_chunk(crypt_thread_ctxt_t *thd, const uchar *buf, size_t len,
		    size_t *bytes_processed)
{
	const uchar *ptr;
	uint version;
	ulong checksum, checksum_exp;
	ulonglong tmp;
	xb_rcrypt_result_t result = XB_CRYPT_READ_CHUNK;

	*bytes_processed = 0;
	ptr = buf;

	CHECK_BUF_SIZE(ptr, XB_CRYPT_CHUNK_MAGIC_SIZE, buf, len);
	if (memcmp(ptr, XB_CRYPT_CHUNK_MAGIC3,
		   XB_CRYPT_CHUNK_MAGIC_SIZE) == 0) {
		version = 3;
	} else if (memcmp(ptr, XB_CRYPT_CHUNK_MAGIC2,
			  XB_CRYPT_CHUNK_MAGIC_SIZE) == 0) {
		version = 2;
	} else if (memcmp(ptr, XB_CRYPT_CHUNK_MAGIC1,
			  XB_CRYPT_CHUNK_MAGIC_SIZE) == 0) {
		version = 1;
	} else {
		msg("%s:%s: wrong chunk magic at offset 0x%llx.\n",
		    my_progname, __FUNCTION__, thd->offset);
		result = XB_CRYPT_READ_ERROR;
		goto exit;
	}

	ptr += XB_CRYPT_CHUNK_MAGIC_SIZE;
	thd->offset += XB_CRYPT_CHUNK_MAGIC_SIZE;

	CHECK_BUF_SIZE(ptr, 8, buf, len);
	tmp = uint8korr(ptr);	/* reserved */
	ptr += 8;
	thd->offset += 8;

	CHECK_BUF_SIZE(ptr, 8, buf, len);
	tmp = uint8korr(ptr);	/* original size */
	ptr += 8;
	if (tmp > INT_MAX) {
		msg("%s:%s: invalid original size at offset 0x%llx.\n",
		    my_progname, __FUNCTION__, thd->offset);
		result = XB_CRYPT_READ_ERROR;
		goto exit;
	}
	thd->offset += 8;
	thd->to_len = (size_t)tmp;

	if (thd->to_size < thd->to_len) {
		thd->to = (uchar *) my_realloc(
				thd->to,
				thd->to_len,
				MYF(MY_FAE | MY_ALLOW_ZERO_PTR));
		thd->to_size = thd->to_len;
	}

	CHECK_BUF_SIZE(ptr, 8, buf, len);
	tmp = uint8korr(ptr);	/* encrypted size */
	ptr += 8;
	if (tmp > INT_MAX) {
		msg("%s:%s: invalid encrypted size at offset 0x%llx.\n",
		    my_progname, __FUNCTION__, thd->offset);
		result = XB_CRYPT_READ_ERROR;
		goto exit;
	}
	thd->offset += 8;
	thd->from_len = (size_t)tmp;

	CHECK_BUF_SIZE(ptr, 4, buf, len);
	checksum_exp = uint4korr(ptr);	/* checksum */
	ptr += 4;
	thd->offset += 4;

	/* iv size */
	if (version == 1) {
		thd->iv_len = 0;
		thd->iv = NULL;
	} else {
		CHECK_BUF_SIZE(ptr, 8, buf, len);

		tmp = uint8korr(ptr);
		if (tmp > INT_MAX) {
			msg("%s:%s: invalid iv size at offset 0x%llx.\n",
			    my_progname, __FUNCTION__, thd->offset);
			result = XB_CRYPT_READ_ERROR;
			goto exit;
		}
		ptr += 8;
		thd->offset += 8;
		thd->iv_len = (size_t)tmp;
	}

	if (thd->iv_len > 0) {
		CHECK_BUF_SIZE(ptr, thd->iv_len, buf, len);
		thd->iv = ptr;
		ptr += thd->iv_len;
	}

	/* for version euqals 2 we need to read in the iv data but do not init
	CTR with it */
	if (version == 2) {
		thd->iv_len = 0;
		thd->iv = 0;
	}

	if (thd->from_len > 0) {
		CHECK_BUF_SIZE(ptr, thd->from_len, buf, len);
		thd->from = ptr;
		ptr += thd->from_len;
	}

	xb_ad(thd->from_len <= thd->to_len);

	checksum = crc32(0, thd->from, thd->from_len);
	if (checksum != checksum_exp) {
		msg("%s:%s invalid checksum at offset 0x%llx, "
		    "expected 0x%lx, actual 0x%lx.\n", my_progname,
		    __FUNCTION__, thd->offset, checksum_exp, checksum);
		result = XB_CRYPT_READ_ERROR;
		goto exit;
	}

	thd->offset += thd->from_len;

	thd->hash_appended = version > 2;

exit:

	*bytes_processed = (size_t) (ptr - buf);

	return result;
}

static
int
decrypt_write(ds_file_t *file, const void *buf, size_t len)
{
	ds_decrypt_file_t	*crypt_file;
	ds_decrypt_ctxt_t	*crypt_ctxt;
	crypt_thread_ctxt_t	*threads;
	crypt_thread_ctxt_t	*thd;
	uint			nthreads;
	uint			i;
	size_t			bytes_processed;
	xb_rcrypt_result_t	parse_result = XB_CRYPT_READ_CHUNK;
	my_bool			err = FALSE;

	crypt_file = (ds_decrypt_file_t *) file->ptr;
	crypt_ctxt = crypt_file->crypt_ctxt;

	threads = crypt_ctxt->threads;
	nthreads = crypt_ctxt->nthreads;

	if (crypt_file->buf_len > 0) {
		thd = threads;

		pthread_mutex_lock(&thd->ctrl_mutex);

		do {
			if (parse_result == XB_CRYPT_READ_INCOMPLETE) {
				crypt_file->buf_size = crypt_file->buf_size * 2;
				crypt_file->buf = (uchar *) my_realloc(
						crypt_file->buf,
						crypt_file->buf_size,
						MYF(MY_FAE|MY_ALLOW_ZERO_PTR));
			}

			memcpy(crypt_file->buf + crypt_file->buf_len,
			       buf, MY_MIN(crypt_file->buf_size -
					   crypt_file->buf_len, len));

			parse_result = parse_xbcrypt_chunk(
				thd, crypt_file->buf,
				crypt_file->buf_size, &bytes_processed);

			if (parse_result == XB_CRYPT_READ_ERROR) {
				pthread_mutex_unlock(&thd->ctrl_mutex);
				return 1;
			}

		} while (parse_result == XB_CRYPT_READ_INCOMPLETE &&
			 crypt_file->buf_size < len);

		if (parse_result != XB_CRYPT_READ_CHUNK) {
			msg("decrypt: incomplete data.\n");
			pthread_mutex_unlock(&thd->ctrl_mutex);
			return 1;
		}

		pthread_mutex_lock(&thd->data_mutex);
		thd->data_avail = TRUE;
		pthread_cond_signal(&thd->data_cond);
		pthread_mutex_unlock(&thd->data_mutex);

		len -= bytes_processed - crypt_file->buf_len;
		buf += bytes_processed - crypt_file->buf_len;

		/* reap */

		pthread_mutex_lock(&thd->data_mutex);
		while (thd->data_avail == TRUE) {
			pthread_cond_wait(&thd->data_cond,
					  &thd->data_mutex);
		}

		if (thd->failed) {
			msg("decrypt: failed to decrypt chunk.\n");
			err = TRUE;
		}

		xb_a(thd->to_len > 0);

		if (!err &&
		    ds_write(crypt_file->dest_file, thd->to, thd->to_len)) {
			msg("decrypt: write to destination failed.\n");
			err = TRUE;
		}

		crypt_file->bytes_processed += thd->from_len;

		pthread_mutex_unlock(&thd->data_mutex);
		pthread_mutex_unlock(&thd->ctrl_mutex);

		crypt_file->buf_len = 0;

		if (err) {
			return 1;
		}
	}

	while (parse_result == XB_CRYPT_READ_CHUNK && len > 0) {
		uint max_thread;

		for (i = 0; i < nthreads; i++) {
			thd = threads + i;

			pthread_mutex_lock(&thd->ctrl_mutex);

			parse_result = parse_xbcrypt_chunk(
				thd, buf, len, &bytes_processed);

			if (parse_result == XB_CRYPT_READ_ERROR) {
				pthread_mutex_unlock(&thd->ctrl_mutex);
				err = TRUE;
				break;
			}

			thd->parse_result = parse_result;

			if (parse_result != XB_CRYPT_READ_CHUNK) {
				pthread_mutex_unlock(&thd->ctrl_mutex);
				break;
			}

			pthread_mutex_lock(&thd->data_mutex);
			thd->data_avail = TRUE;
			pthread_cond_signal(&thd->data_cond);
			pthread_mutex_unlock(&thd->data_mutex);

			len -= bytes_processed;
			buf += bytes_processed;
		}

		max_thread = (i < nthreads) ? i :  nthreads - 1;

		/* Reap and write decrypted data */
		for (i = 0; i <= max_thread; i++) {
			thd = threads + i;

			if (thd->parse_result != XB_CRYPT_READ_CHUNK) {
				break;
			}

			pthread_mutex_lock(&thd->data_mutex);
			while (thd->data_avail == TRUE) {
				pthread_cond_wait(&thd->data_cond,
						  &thd->data_mutex);
			}

			if (thd->failed) {
				msg("decrypt: failed to decrypt chunk.\n");
				err = TRUE;
			}

			xb_a(thd->to_len > 0);

			if (!err && ds_write(crypt_file->dest_file, thd->to,
				     thd->to_len)) {
				msg("decrypt: write to destination failed.\n");
				err = TRUE;
			}

			crypt_file->bytes_processed += thd->from_len;

			pthread_mutex_unlock(&thd->data_mutex);
			pthread_mutex_unlock(&thd->ctrl_mutex);
		}

		if (err) {
			return 1;
		}
	}

	if (parse_result == XB_CRYPT_READ_INCOMPLETE && len > 0) {
		crypt_file->buf_len = len;
		if (crypt_file->buf_size < len) {
			crypt_file->buf = (uchar *) my_realloc(
					crypt_file->buf,
					crypt_file->buf_len,
					MYF(MY_FAE | MY_ALLOW_ZERO_PTR));
			crypt_file->buf_size = len;
		}
		memcpy(crypt_file->buf, buf, len);
	}

	return 0;
}

static
int
decrypt_close(ds_file_t *file)
{
	ds_decrypt_file_t	*crypt_file;
	ds_file_t		*dest_file;
	int			rc = 0;

	crypt_file = (ds_decrypt_file_t *) file->ptr;
	dest_file = crypt_file->dest_file;

	if (ds_close(dest_file)) {
		rc = 1;
	}

	my_free(crypt_file->buf);
	my_free(file);

	return rc;
}

static
void
decrypt_deinit(ds_ctxt_t *ctxt)
{
	ds_decrypt_ctxt_t 	*crypt_ctxt;

	xb_ad(ctxt->pipe_ctxt != NULL);

	crypt_ctxt = (ds_decrypt_ctxt_t *) ctxt->ptr;

	destroy_worker_threads(crypt_ctxt->threads, crypt_ctxt->nthreads);

	my_free(ctxt->root);
	my_free(ctxt);
}

static
crypt_thread_ctxt_t *
create_worker_threads(uint n)
{
	crypt_thread_ctxt_t	*threads;
	uint 			i;

	threads = (crypt_thread_ctxt_t *)
		my_malloc(sizeof(crypt_thread_ctxt_t) * n,
			  MYF(MY_FAE | MY_ZEROFILL));

	for (i = 0; i < n; i++) {
		crypt_thread_ctxt_t *thd = threads + i;

		thd->num = i + 1;

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

		if (ds_decrypt_encrypt_algo != GCRY_CIPHER_NONE) {
			gcry_error_t 		gcry_error;

			gcry_error = gcry_cipher_open(&thd->cipher_handle,
						      ds_decrypt_encrypt_algo,
						      encrypt_mode, 0);
			if (gcry_error) {
				msg("decrypt: unable to open libgcrypt"
				    " cipher - %s : %s\n",
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				gcry_cipher_close(thd->cipher_handle);
				goto err;
			}

			gcry_error = gcry_cipher_setkey(thd->cipher_handle,
							ds_decrypt_encrypt_key,
							encrypt_key_len);
			if (gcry_error) {
				msg("decrypt: unable to set libgcrypt"
				    " cipher key - %s : %s\n",
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				gcry_cipher_close(thd->cipher_handle);
				goto err;
			}
		}

		pthread_mutex_lock(&thd->ctrl_mutex);

		if (pthread_create(&thd->id, NULL, decrypt_worker_thread_func,
				   thd)) {
			msg("decrypt: pthread_create() failed: "
			    "errno = %d\n", errno);
			goto err;
		}
	}

	/* Wait for the threads to start */
	for (i = 0; i < n; i++) {
		crypt_thread_ctxt_t *thd = threads + i;

		while (thd->started == FALSE)
			pthread_cond_wait(&thd->ctrl_cond, &thd->ctrl_mutex);
		pthread_mutex_unlock(&thd->ctrl_mutex);
	}

	return threads;

err:
	return NULL;
}

static
void
destroy_worker_threads(crypt_thread_ctxt_t *threads, uint n)
{
	uint i;

	for (i = 0; i < n; i++) {
		crypt_thread_ctxt_t *thd = threads + i;

		pthread_mutex_lock(&thd->data_mutex);
		threads[i].cancelled = TRUE;
		pthread_cond_signal(&thd->data_cond);
		pthread_mutex_unlock(&thd->data_mutex);

		pthread_join(thd->id, NULL);

		pthread_cond_destroy(&thd->data_cond);
		pthread_mutex_destroy(&thd->data_mutex);
		pthread_cond_destroy(&thd->ctrl_cond);
		pthread_mutex_destroy(&thd->ctrl_mutex);

		if (ds_decrypt_encrypt_algo != GCRY_CIPHER_NONE)
			gcry_cipher_close(thd->cipher_handle);

		my_free(thd->to);
	}

	my_free(threads);
}

static
void *
decrypt_worker_thread_func(void *arg)
{
	crypt_thread_ctxt_t *thd = (crypt_thread_ctxt_t *) arg;

	pthread_mutex_lock(&thd->ctrl_mutex);

	pthread_mutex_lock(&thd->data_mutex);

	thd->started = TRUE;
	pthread_cond_signal(&thd->ctrl_cond);

	pthread_mutex_unlock(&thd->ctrl_mutex);

	while (1) {
		thd->data_avail = FALSE;
		pthread_cond_signal(&thd->data_cond);

		while (!thd->data_avail && !thd->cancelled) {
			pthread_cond_wait(&thd->data_cond, &thd->data_mutex);
		}

		if (thd->cancelled)
			break;

		if (ds_decrypt_encrypt_algo != GCRY_CIPHER_NONE) {

			gcry_error_t	gcry_error;

			gcry_error = gcry_cipher_reset(thd->cipher_handle);
			if (gcry_error) {
				msg("%s:decrypt: unable to reset libgcrypt"
				    " cipher - %s : %s\n", my_progname,
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				thd->failed = TRUE;
				continue;
			}

			if (thd->iv_len > 0) {
				gcry_error = gcry_cipher_setctr(
					thd->cipher_handle,
					thd->iv,
					thd->iv_len);
			}
			if (gcry_error) {
				msg("%s:decrypt: unable to set cipher iv - "
				    "%s : %s\n", my_progname,
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				thd->failed = TRUE;
				continue;
			}

			/* Try to decrypt it */
			gcry_error = gcry_cipher_decrypt(thd->cipher_handle,
							 thd->to,
							 thd->to_len,
							 thd->from,
							 thd->from_len);
			if (gcry_error) {
				msg("%s:decrypt: unable to decrypt chunk - "
				    "%s : %s\n", my_progname,
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				gcry_cipher_close(thd->cipher_handle);
				thd->failed = TRUE;
				continue;
			}

		} else {
			memcpy(thd->to, thd->from, thd->to_len);
		}

		if (thd->hash_appended) {
			uchar hash[XB_CRYPT_HASH_LEN];

			thd->to_len -= XB_CRYPT_HASH_LEN;

			/* ensure that XB_CRYPT_HASH_LEN is the correct length
			of XB_CRYPT_HASH hashing algorithm output */
			xb_a(gcry_md_get_algo_dlen(XB_CRYPT_HASH) ==
			     XB_CRYPT_HASH_LEN);
			gcry_md_hash_buffer(XB_CRYPT_HASH, hash, thd->to,
					    thd->to_len);
			if (memcmp(hash, (char *) thd->to + thd->to_len,
				   XB_CRYPT_HASH_LEN) != 0) {
				msg("%s:%s invalid plaintext hash. "
				    "Wrong encrytion key specified?\n",
				    my_progname, __FUNCTION__);
				thd->failed = TRUE;
				continue;
			}
		}

	}

	pthread_mutex_unlock(&thd->data_mutex);

	return NULL;
}
