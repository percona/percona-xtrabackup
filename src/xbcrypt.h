/******************************************************
Copyright (c) 2011 Percona Ireland Ltd.

Encryption interface for XtraBackup.

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

#ifndef XBCRYPT_H
#define XBCRYPT_H

#include <my_base.h>
#include "common.h"

#define XB_CRYPT_CHUNK_MAGIC "XBCRYP01"
#define XB_CRYPT_CHUNK_MAGIC_SIZE (sizeof(XB_CRYPT_CHUNK_MAGIC)-1)

/******************************************************************************
Write interface */
typedef struct xb_wcrypt_struct xb_wcrypt_t;

/* Callback on write for i/o, must return # of bytes written or -1 on error */
typedef ssize_t xb_crypt_write_callback(void *userdata,
					const void *buf, size_t len);

xb_wcrypt_t *xb_crypt_write_open(void *userdata,
				 xb_crypt_write_callback *onwrite);

/* Takes buffer, original length and encrypted length, formats output buffer
   and calls write callback.
   Returns 0 on success, 1 on error */
int xb_crypt_write_chunk(xb_wcrypt_t *crypt, const void *buf, size_t olen,
			 size_t elen);

/* Returns 0 on success, 1 on error */
int xb_crypt_write_close(xb_wcrypt_t *crypt);

/******************************************************************************
Read interface */
typedef struct xb_rcrypt_struct xb_rcrypt_t;

/* Callback on write for i/o, must return # of bytes read or -1 on error */
typedef ssize_t xb_crypt_read_callback(void *userdata,
				       void *buf, size_t len);

xb_rcrypt_t *xb_crypt_read_open(void *userdata,
				  xb_crypt_read_callback *onread);

typedef enum {
	XB_CRYPT_READ_CHUNK,
	XB_CRYPT_READ_EOF,
	XB_CRYPT_READ_ERROR
} xb_rcrypt_result_t;

xb_rcrypt_result_t xb_crypt_read_chunk(xb_rcrypt_t *crypt, void **buf,
				       size_t *olen, size_t *elen);

int xb_crypt_read_close(xb_rcrypt_t *crypt);

/******************************************************************************
Utility interface */
my_bool xb_crypt_read_key_file(const char *filename,
			       void** key, uint *keylength);

#endif
