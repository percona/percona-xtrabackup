/******************************************************
Copyright (c) 2011 Percona Ireland Ltd.

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/


#include <my_base.h>
#include <univ.i>
#include "common.h"
#include "datasink.h"
#include <gcrypt.h>
#include "xbcrypt.h"

GCRY_THREAD_OPTION_PTHREAD_IMPL;

#define XB_CRYPT_CHUNK_SIZE ((size_t) (xtrabackup_encrypt_chunk_size))

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
	const char 		*from;
	size_t			from_len;
	char			*to;
	size_t			to_len;
	gcry_cipher_hd_t	cipher_handle;
} crypt_thread_ctxt_t;

typedef struct {
	crypt_thread_ctxt_t	*threads;
	uint			nthreads;
} ds_encrypt_ctxt_t;

typedef struct {
	xb_wcrypt_t		*xbcrypt_file;
	ds_encrypt_ctxt_t	*crypt_ctxt;
	size_t			bytes_processed;
	ds_file_t		*dest_file;
} ds_encrypt_file_t;

/* Encryption options */
extern ulong		xtrabackup_encrypt_algo;
extern char		*xtrabackup_encrypt_key;
extern char		*xtrabackup_encrypt_key_file;
extern uint		xtrabackup_encrypt_threads;
extern ulonglong	xtrabackup_encrypt_chunk_size;

static ds_ctxt_t *encrypt_init(const char *root);
static ds_file_t *encrypt_open(ds_ctxt_t *ctxt, const char *path,
				MY_STAT *mystat);
static int encrypt_write(ds_file_t *file, const void *buf, size_t len);
static int encrypt_close(ds_file_t *file);
static void encrypt_deinit(ds_ctxt_t *ctxt);

datasink_t datasink_encrypt = {
	&encrypt_init,
	&encrypt_open,
	&encrypt_write,
	&encrypt_close,
	&encrypt_deinit
};

static crypt_thread_ctxt_t *create_worker_threads(uint n);
static void destroy_worker_threads(crypt_thread_ctxt_t *threads, uint n);
static void *encrypt_worker_thread_func(void *arg);

static uint encrypt_algos[] = { GCRY_CIPHER_NONE, GCRY_CIPHER_AES128,
				GCRY_CIPHER_AES192, GCRY_CIPHER_AES256 };
static uint encrypt_algo;
static const uint encrypt_mode = GCRY_CIPHER_MODE_CTR;
static uint encrypt_key_len = 0;
static const unsigned char encrypt_iv[] = "Percona Xtrabackup is Awesome!!!";
static size_t encrypt_iv_len = 0;

static
ssize_t
my_xb_crypt_write_callback(void *userdata, const void *buf, size_t len)
{
	ds_encrypt_file_t		*encrypt_file;

	encrypt_file = (ds_encrypt_file_t *) userdata;

	xb_ad(encrypt_file != NULL);
	xb_ad(encrypt_file->dest_file != NULL);

	if (!ds_write(encrypt_file->dest_file, buf, len)) {
		return len;
	}
	return -1;
}

static
ds_ctxt_t *
encrypt_init(const char *root)
{
	ds_ctxt_t		*ctxt;
	ds_encrypt_ctxt_t	*encrypt_ctxt;
	crypt_thread_ctxt_t	*threads;
	gcry_error_t 		gcry_error;

	/* Acording to gcrypt docs (and my testing), setting up the threading
	   callbacks must be done first, so, lets give it a shot */
	gcry_error = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	if (gcry_error) {
		msg("encrypt: unable to set libgcrypt thread cbs - "
		    "%s : %s\n",
		    gcry_strsource(gcry_error),
		    gcry_strerror(gcry_error));
		return NULL;
	}

	/* Version check should be the very next call because it
	makes sure that important subsystems are intialized. */
	if (!gcry_control(GCRYCTL_ANY_INITIALIZATION_P)) {
		const char	*gcrypt_version;
		gcrypt_version = gcry_check_version(NULL);
		/* No other library has already initialized libgcrypt. */
		if (!gcrypt_version) {
			msg("encrypt: failed to initialize libgcrypt\n");
			return NULL;
		} else {
			msg("encrypt: using gcrypt %s\n", gcrypt_version);
		}
	}

	/* Disable the gcry secure memory, not dealing with this for now */
	gcry_error = gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
	if (gcry_error) {
		msg("encrypt: unable to disable libgcrypt secmem - "
		    "%s : %s\n",
		    gcry_strsource(gcry_error),
		    gcry_strerror(gcry_error));
		return NULL;
	}

	/* Finalize gcry initialization. */
	gcry_error = gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
	if (gcry_error) {
		msg("encrypt: unable to finish libgcrypt initialization - "
		    "%s : %s\n",
		    gcry_strsource(gcry_error),
		    gcry_strerror(gcry_error));
		return NULL;
	}

	/* Determine the algorithm */
	encrypt_algo = encrypt_algos[xtrabackup_encrypt_algo];

	/* Set up the iv length */
	encrypt_iv_len = gcry_cipher_get_algo_blklen(encrypt_algo);
	ut_a(encrypt_iv_len > 0);
	ut_a(encrypt_iv_len <= sizeof(encrypt_iv));

	/* Now set up the key */
	if (xtrabackup_encrypt_key == NULL &&
	    xtrabackup_encrypt_key_file == NULL) {
		msg("encrypt: no encryption key or key file specified.\n");
		return NULL;
	} else if (xtrabackup_encrypt_key && xtrabackup_encrypt_key_file) {
		msg("encrypt: both encryption key and key file specified.\n");
		return NULL;
	} else if (xtrabackup_encrypt_key_file) {
		if (!xb_crypt_read_key_file(xtrabackup_encrypt_key_file,
					    (void**)&xtrabackup_encrypt_key,
					    &encrypt_key_len)) {
			msg("encrypt: unable to read encryption key file"
			    " \"%s\".\n", xtrabackup_encrypt_key_file);
			return NULL;
		}
	} else if (xtrabackup_encrypt_key) {
		encrypt_key_len = strlen(xtrabackup_encrypt_key);
	} else {
		msg("encrypt: no encryption key or key file specified.\n");
		return NULL;
	}

	/* Create and initialize the worker threads */
	threads = create_worker_threads(xtrabackup_encrypt_threads);
	if (threads == NULL) {
		msg("encrypt: failed to create worker threads.\n");
		return NULL;
	}

	ctxt = (ds_ctxt_t *) my_malloc(sizeof(ds_ctxt_t) +
				       sizeof(ds_encrypt_ctxt_t),
				       MYF(MY_FAE));

	encrypt_ctxt = (ds_encrypt_ctxt_t *) (ctxt + 1);
	encrypt_ctxt->threads = threads;
	encrypt_ctxt->nthreads = xtrabackup_encrypt_threads;

	ctxt->ptr = encrypt_ctxt;
	ctxt->root = my_strdup(root, MYF(MY_FAE));

	return ctxt;
}

static
ds_file_t *
encrypt_open(ds_ctxt_t *ctxt, const char *path, MY_STAT *mystat)
{
	ds_ctxt_t		*dest_ctxt;

	ds_encrypt_ctxt_t	*crypt_ctxt;
	ds_encrypt_file_t	*crypt_file;

	char			new_name[FN_REFLEN];
	ds_file_t		*file;

	xb_ad(ctxt->pipe_ctxt != NULL);
	dest_ctxt = ctxt->pipe_ctxt;

	crypt_ctxt = (ds_encrypt_ctxt_t *) ctxt->ptr;


	file = (ds_file_t *) my_malloc(sizeof(ds_file_t) +
				       sizeof(ds_encrypt_file_t),
				       MYF(MY_FAE|MY_ZEROFILL));

	crypt_file = (ds_encrypt_file_t *) (file + 1);

	/* Append the .xbcrypt extension to the filename */
	fn_format(new_name, path, "", ".xbcrypt", MYF(MY_APPEND_EXT));
	crypt_file->dest_file = ds_open(dest_ctxt, new_name, mystat);
	if (crypt_file->dest_file == NULL) {
		msg("encrypt: ds_open(\"%s\") failed.\n", new_name);
		goto err;
	}

	crypt_file->crypt_ctxt = crypt_ctxt;
	crypt_file->xbcrypt_file = xb_crypt_write_open(crypt_file,
					   my_xb_crypt_write_callback);

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
	MY_FREE(file);
	return NULL;
}

static
int
encrypt_write(ds_file_t *file, const void *buf, size_t len)
{
	ds_encrypt_file_t	*crypt_file;
	ds_encrypt_ctxt_t	*crypt_ctxt;
	crypt_thread_ctxt_t	*threads;
	crypt_thread_ctxt_t	*thd;
	uint			nthreads;
	uint			i;
	const char		*ptr;

	crypt_file = (ds_encrypt_file_t *) file->ptr;
	crypt_ctxt = crypt_file->crypt_ctxt;

	threads = crypt_ctxt->threads;
	nthreads = crypt_ctxt->nthreads;

	ptr = (const char *) buf;
	while (len > 0) {
		uint max_thread;

		/* Send data to worker threads for encryption */
		for (i = 0; i < nthreads; i++) {
			size_t chunk_len;

			thd = threads + i;

			pthread_mutex_lock(&thd->ctrl_mutex);

			chunk_len = (len > XB_CRYPT_CHUNK_SIZE) ?
				XB_CRYPT_CHUNK_SIZE : len;
			thd->from = ptr;
			thd->from_len = chunk_len;

			pthread_mutex_lock(&thd->data_mutex);
			thd->data_avail = TRUE;
			pthread_cond_signal(&thd->data_cond);
			pthread_mutex_unlock(&thd->data_mutex);

			len -= chunk_len;
			if (len == 0) {
				break;
			}
			ptr += chunk_len;
		}

		max_thread = (i < nthreads) ? i :  nthreads - 1;

		/* Reap and stream the encrypted data */
		for (i = 0; i <= max_thread; i++) {
			thd = threads + i;

			pthread_mutex_lock(&thd->data_mutex);
			while (thd->data_avail == TRUE) {
				pthread_cond_wait(&thd->data_cond,
						  &thd->data_mutex);
			}

			ut_a(threads[i].to_len > 0);

			if (xb_crypt_write_chunk(crypt_file->xbcrypt_file,
						 threads[i].to,
						 threads[i].from_len,
						 threads[i].to_len)) {
				msg("encrypt: write to the destination file "
				    "failed.\n");
				return 1;
			}

			crypt_file->bytes_processed += threads[i].from_len;

			pthread_mutex_unlock(&threads[i].data_mutex);
			pthread_mutex_unlock(&threads[i].ctrl_mutex);
		}
	}

	return 0;
}

static
int
encrypt_close(ds_file_t *file)
{
	ds_encrypt_file_t	*crypt_file;
	ds_file_t		*dest_file;
	int			rc = 0;

	crypt_file = (ds_encrypt_file_t *) file->ptr;
	dest_file = crypt_file->dest_file;

	rc = xb_crypt_write_close(crypt_file->xbcrypt_file);

	ds_close(dest_file);

	MY_FREE(file);

	return rc;
}

static
void
encrypt_deinit(ds_ctxt_t *ctxt)
{
	ds_encrypt_ctxt_t 	*crypt_ctxt;

	xb_ad(ctxt->pipe_ctxt != NULL);

	crypt_ctxt = (ds_encrypt_ctxt_t *) ctxt->ptr;

	destroy_worker_threads(crypt_ctxt->threads, crypt_ctxt->nthreads);

	MY_FREE(ctxt->root);
	MY_FREE(ctxt);
	if (xtrabackup_encrypt_key)
		MY_FREE(xtrabackup_encrypt_key);
	if (xtrabackup_encrypt_key_file)
		MY_FREE(xtrabackup_encrypt_key_file);
}

static
crypt_thread_ctxt_t *
create_worker_threads(uint n)
{
	crypt_thread_ctxt_t	*threads;
	uint 			i;

	threads = (crypt_thread_ctxt_t *)
		my_malloc(sizeof(crypt_thread_ctxt_t) * n, MYF(MY_FAE));

	for (i = 0; i < n; i++) {
		crypt_thread_ctxt_t *thd = threads + i;

		thd->num = i + 1;
		thd->started = FALSE;
		thd->cancelled = FALSE;
		thd->data_avail = FALSE;

		thd->to = (char *) my_malloc(XB_CRYPT_CHUNK_SIZE,
						   MYF(MY_FAE));

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

		if (encrypt_algo != GCRY_CIPHER_NONE) {
			gcry_error_t 		gcry_error;

			gcry_error = gcry_cipher_open(&thd->cipher_handle,
						      encrypt_algo,
						      encrypt_mode, 0);
			if (gcry_error) {
				msg("encrypt: unable to open libgcrypt"
				    " cipher - %s : %s\n",
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				gcry_cipher_close(thd->cipher_handle);
				goto err;
			}

			gcry_error = gcry_cipher_setkey(thd->cipher_handle,
							xtrabackup_encrypt_key,
							encrypt_key_len);
			if (gcry_error) {
				msg("encrypt: unable to set libgcrypt"
				    " cipher key - %s : %s\n",
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				gcry_cipher_close(thd->cipher_handle);
				goto err;
			}
		}

		pthread_mutex_lock(&thd->ctrl_mutex);

		if (pthread_create(&thd->id, NULL, encrypt_worker_thread_func,
				   thd)) {
			msg("encrypt: pthread_create() failed: "
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

		if (encrypt_algo != GCRY_CIPHER_NONE)
			gcry_cipher_close(thd->cipher_handle);

		MY_FREE(thd->to);
	}

	MY_FREE(threads);
}

static
void *
encrypt_worker_thread_func(void *arg)
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

		thd->to_len = thd->from_len;

		if (encrypt_algo != GCRY_CIPHER_NONE) {
			gcry_error_t 		gcry_error;

			gcry_error = gcry_cipher_reset(thd->cipher_handle);
			if (gcry_error) {
				msg("encrypt: unable to reset cipher - "
				    "%s : %s\n",
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				thd->to_len = 0;
				continue;
			}

			gcry_error = gcry_cipher_setiv(thd->cipher_handle,
							encrypt_iv,
							encrypt_iv_len);
			if (gcry_error) {
				msg("encrypt: unable to set cipher ctr - "
				    "%s : %s\n",
				    gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				thd->to_len = 0;
				continue;
			}

			gcry_error = gcry_cipher_encrypt(thd->cipher_handle,
							 thd->to,
							 thd->to_len,
							 thd->from,
							 thd->from_len);
			if (gcry_error) {
				msg("encrypt: unable to encrypt buffer - "
				    "%s : %s\n", gcry_strsource(gcry_error),
				    gcry_strerror(gcry_error));
				thd->to_len = 0;
			}
		} else {
			memcpy(thd->to, thd->from, thd->from_len);
		}
	}

	pthread_mutex_unlock(&thd->data_mutex);

	return NULL;
}
