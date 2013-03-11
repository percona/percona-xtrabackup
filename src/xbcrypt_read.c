/******************************************************
Copyright (c) 2011 Percona Ireland Ltd.

The xbcrypt format reader implementation.

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

#include "xbcrypt.h"

struct xb_rcrypt_struct {
	void				*userdata;
	xb_crypt_read_callback		*read;
	void				*buffer;
	size_t				bufsize;
	ulonglong			offset;
};

xb_rcrypt_t *
xb_crypt_read_open(void *userdata, xb_crypt_read_callback *onread)
{
	xb_rcrypt_t	*crypt;

	xb_ad(onread);

	crypt = (xb_rcrypt_t *) my_malloc(sizeof(xb_rcrypt_t), MYF(MY_FAE));

	crypt->userdata = userdata;
	crypt->read = onread;
	crypt->buffer = NULL;
	crypt->bufsize = 0;
	crypt->offset = 0;

	return crypt;
}

xb_rcrypt_result_t
xb_crypt_read_chunk(xb_rcrypt_t *crypt, void **buf, size_t *olen, size_t *elen)
{
	uchar		tmpbuf[XB_CRYPT_CHUNK_MAGIC_SIZE + 8 + 8 + 8 + 4];
	uchar		*ptr;
	ulonglong	tmp;
	ulong		checksum, checksum_exp;
	ssize_t		bytesread;
	xb_rcrypt_result_t result = XB_CRYPT_READ_CHUNK;

	if ((bytesread = crypt->read(crypt->userdata, tmpbuf, sizeof(tmpbuf)))
	    != sizeof(tmpbuf)) {
		if (bytesread == 0) {
			result = XB_CRYPT_READ_EOF;
			goto err;
		} else {
			msg("%s:%s: unable to read chunk header data at "
			    "offset 0x%llx.\n",
			    my_progname, __FUNCTION__, crypt->offset);
			result =  XB_CRYPT_READ_ERROR;
			goto err;
		}
	}

	ptr = tmpbuf;

	if (memcmp(ptr, XB_CRYPT_CHUNK_MAGIC, XB_CRYPT_CHUNK_MAGIC_SIZE)
	    != 0) {
		msg("%s:%s: wrong chunk magic at offset 0x%llx.\n",
		    my_progname, __FUNCTION__, crypt->offset);
		result = XB_CRYPT_READ_ERROR;
		goto err;
	}

	ptr += XB_CRYPT_CHUNK_MAGIC_SIZE;
	crypt->offset += XB_CRYPT_CHUNK_MAGIC_SIZE;

	tmp = uint8korr(ptr);	/* reserved */
	ptr += 8;
	crypt->offset += 8;

	tmp = uint8korr(ptr);	/* original size */
	ptr += 8;
	if (tmp > INT_MAX) {
		msg("%s:%s: invalid original size at offset 0x%llx.\n",
		    my_progname, __FUNCTION__, crypt->offset);
		result = XB_CRYPT_READ_ERROR;
		goto err;
	}
	crypt->offset += 8;
	*olen = (size_t)tmp;

	tmp = uint8korr(ptr);	/* encrypted size */
	ptr += 8;
	if (tmp > INT_MAX) {
		msg("%s:%s: invalid encrypted size at offset 0x%llx.\n",
		    my_progname, __FUNCTION__, crypt->offset);
		result = XB_CRYPT_READ_ERROR;
		goto err;
	}
	crypt->offset += 8;
	*elen = (size_t)tmp;

	checksum_exp = uint4korr(ptr);	/* checksum */
	ptr += 4;
	crypt->offset += 4;

	if (*olen > crypt->bufsize) {
		if (crypt->buffer) {
			crypt->buffer = my_realloc(crypt->buffer, *olen,
						   MYF(MY_WME));
			if (crypt->buffer == NULL) {
				msg("%s:%s: failed to increase buffer to "
				    "%llu bytes.\n", my_progname, __FUNCTION__,
				    (ulonglong)*olen);
				result = XB_CRYPT_READ_ERROR;
				goto err;
			}
		} else {
			crypt->buffer = my_malloc(*olen, MYF(MY_WME));
			if (crypt->buffer == NULL) {
				msg("%s:%s: failed to allocate buffer of "
				    "%llu bytes.\n", my_progname, __FUNCTION__,
				    (ulonglong)*olen);
				result = XB_CRYPT_READ_ERROR;
				goto err;
			}
		}
		crypt->bufsize = *olen;
	}

	if (*elen > 0) {
		if (crypt->read(crypt->userdata, crypt->buffer, *elen)
		    != (ssize_t)*elen) {
			msg("%s:%s: failed to read %lld bytes for chunk payload "
			    "at offset 0x%llx.\n", my_progname, __FUNCTION__,
			    (ulonglong)*elen, crypt->offset);
			result = XB_CRYPT_READ_ERROR;
			goto err;
		}
	}

	checksum = crc32(0, crypt->buffer, *elen);
	if (checksum != checksum_exp) {
		msg("%s:%s invalid checksum at offset 0x%llx, "
		    "expected 0x%lx, actual 0x%lx.\n", my_progname, __FUNCTION__,
		    crypt->offset, checksum_exp, checksum);
		result = XB_CRYPT_READ_ERROR;
		goto err;
	}

	crypt->offset += *elen;
	*buf = crypt->buffer;
	goto exit;

err:
	*buf = NULL;
	*olen = 0;
	*elen = 0;
exit:
	return result;
}

int xb_crypt_read_close(xb_rcrypt_t *crypt)
{
	if (crypt->buffer)
		MY_FREE(crypt->buffer);
	MY_FREE(crypt);

	return 0;
}

