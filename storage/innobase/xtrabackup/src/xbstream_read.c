/******************************************************
Copyright (c) 2011-2017 Percona LLC and/or its affiliates.

The xbstream format reader implementation.

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

#include <mysql_version.h>
#include <my_base.h>
#include <zlib.h>
#include "common.h"
#include "xbstream.h"
#include "crc_glue.h"

/* Allocate 1 MB for the payload buffer initially */
#define INIT_BUFFER_LEN (1024 * 1024)

#ifndef MY_OFF_T_MAX
#define MY_OFF_T_MAX (~(my_off_t)0UL)
#endif

struct xb_rstream_struct {
	my_off_t	offset;
	File 		fd;
};

xb_rstream_t *
xb_stream_read_new(void)
{
	xb_rstream_t *stream;

	stream = (xb_rstream_t *) my_malloc(PSI_NOT_INSTRUMENTED,
					    sizeof(xb_rstream_t), MYF(MY_FAE));

	stream->fd = fileno(stdin);
	stream->offset = 0;

#ifdef __WIN__
	setmode(stream->fd, _O_BINARY);
#endif

	return stream;
}

static inline
xb_chunk_type_t
validate_chunk_type(uchar code)
{
	switch ((xb_chunk_type_t) code) {
	case XB_CHUNK_TYPE_PAYLOAD:
	case XB_CHUNK_TYPE_EOF:
		return (xb_chunk_type_t) code;
	default:
		return XB_CHUNK_TYPE_UNKNOWN;
	}
}

int
xb_stream_validate_checksum(xb_rstream_chunk_t *chunk)
{
	ulong	checksum;

	checksum = crc32_iso3309(0, chunk->data, chunk->length);
	if (checksum != chunk->checksum) {
		msg("xb_stream_read_chunk(): invalid checksum at offset "
		    "0x%llx: expected 0x%lx, read 0x%lx.\n",
		    (ulonglong) chunk->checksum_offset, chunk->checksum,
		    checksum);
		return XB_STREAM_READ_ERROR;
	}

	return XB_STREAM_READ_CHUNK;
}

#define F_READ(buf,len)                                       	\
	do {                                                      	\
		if (xb_read_full(fd, buf, len) < len) {	\
			msg("xb_stream_read_chunk(): my_read() failed.\n"); \
			goto err;                                 	\
		}							\
	} while (0)

xb_rstream_result_t
xb_stream_read_chunk(xb_rstream_t *stream, xb_rstream_chunk_t *chunk)
{
	uint		pathlen;
	size_t		tbytes;
	ulonglong	ullval;
	File		fd = stream->fd;
	uchar		*ptr;

	const uint chunk_length_min =
		CHUNK_HEADER_CONSTANT_LEN + FN_REFLEN + 8 + 8 + 4;

	/* Reallocate the buffer if needed */
	if (chunk_length_min > chunk->buflen) {
		chunk->raw_data =
		my_realloc(PSI_NOT_INSTRUMENTED, chunk->raw_data,
			chunk_length_min, MYF(MY_WME | MY_ALLOW_ZERO_PTR));
		if (chunk->raw_data == NULL) {
			msg("xb_stream_read_chunk(): failed to increase buffer"
				" to %lu bytes.\n", (ulong)chunk_length_min);
			goto err;
		}
		chunk->buflen = chunk_length_min;
	}

	ptr = (uchar *)(chunk->raw_data);

	/* This is the only place where we expect EOF, so read with
	xb_read_full() rather than F_READ() */
	tbytes = xb_read_full(fd, ptr, CHUNK_HEADER_CONSTANT_LEN);
	if (tbytes == 0) {
		return XB_STREAM_READ_EOF;
	} else if (tbytes < CHUNK_HEADER_CONSTANT_LEN) {
		msg("xb_stream_read_chunk(): unexpected end of stream at "
		    "offset 0x%llx.\n", stream->offset);
		goto err;
	}

	/* Chunk magic value */
	if (memcmp(ptr, XB_STREAM_CHUNK_MAGIC, 8)) {
		msg("xb_stream_read_chunk(): wrong chunk magic at offset "
		    "0x%llx.\n", (ulonglong) stream->offset);
		goto err;
	}
	ptr += 8;
	stream->offset += 8;

	/* Chunk flags */
	chunk->flags = *ptr++;
	stream->offset++;

	/* Chunk type, ignore unknown ones if ignorable flag is set */
	chunk->type = validate_chunk_type(*ptr);
	if (chunk->type == XB_CHUNK_TYPE_UNKNOWN &&
	    !(chunk->flags & XB_STREAM_FLAG_IGNORABLE)) {
		msg("xb_stream_read_chunk(): unknown chunk type 0x%lu at "
		    "offset 0x%llx.\n", (ulong) *ptr,
		    (ulonglong) stream->offset);
		goto err;
	}
	ptr++;
	stream->offset++;

	/* Path length */
	pathlen = uint4korr(ptr);
	if (pathlen >= FN_REFLEN) {
		msg("xb_stream_read_chunk(): path length (%lu) is too large at "
		    "offset 0x%llx.\n", (ulong) pathlen, stream->offset);
		goto err;
	}
	chunk->pathlen = pathlen;
	stream->offset += 4;
	ptr += 4;

	xb_ad((ptr - (uchar *)(chunk->raw_data)) ==
	      CHUNK_HEADER_CONSTANT_LEN);

	/* Path */
	if (chunk->pathlen > 0) {
		F_READ(ptr, pathlen);
		memcpy(chunk->path, ptr, pathlen);
		stream->offset += pathlen;
		ptr += pathlen;
	}
	chunk->path[pathlen] = '\0';

	if (chunk->type == XB_CHUNK_TYPE_EOF) {
		chunk->raw_length = ptr - (uchar*)(chunk->raw_data);
		return XB_STREAM_READ_CHUNK;
	}

	/* Payload length */
	F_READ(ptr, 16);
	ullval = uint8korr(ptr);
	if (ullval > (ulonglong)SIZE_T_MAX) {
		msg("xb_stream_read_chunk(): chunk length is too large at "
		    "offset 0x%llx: 0x%llx.\n", (ulonglong) stream->offset,
		    ullval);
		goto err;
	}
	chunk->length = (size_t) ullval;
	stream->offset += 8;
	ptr += 8;

	/* Payload offset */
	ullval = uint8korr(ptr);
	if (ullval > (ulonglong)MY_OFF_T_MAX) {
		msg("xb_stream_read_chunk(): chunk offset is too large at "
		    "offset 0x%llx: 0x%llx.\n", (ulonglong) stream->offset,
		    ullval);
		goto err;
	}
	chunk->offset = (my_off_t)ullval;
	stream->offset += 8;
	ptr += 8;

	/* Checksum */
	F_READ(ptr, 4);
	chunk->checksum = uint4korr(ptr);
	chunk->checksum_offset = stream->offset;
	stream->offset += 4;
	ptr += 4;

	chunk->raw_length =
		chunk->length + (ptr - (uchar *)(chunk->raw_data));

	/* Reallocate the buffer if needed */
	if (chunk->raw_length > chunk->buflen) {
		size_t ptr_offs = ptr - (uchar *)(chunk->raw_data);
		chunk->raw_data =
			my_realloc(PSI_NOT_INSTRUMENTED, chunk->raw_data,
				chunk->raw_length,
				MYF(MY_WME | MY_ALLOW_ZERO_PTR));
		if (chunk->raw_data == NULL) {
			msg("xb_stream_read_chunk(): failed to increase "
			    "buffer to %lu bytes.\n", (ulong)chunk->raw_length);
			goto err;
		}
		chunk->buflen = chunk->raw_length;
		ptr = (uchar *)(chunk->raw_data) + ptr_offs;
	}

	/* Payload */
	if (chunk->length > 0) {
		F_READ(ptr, chunk->length);
		stream->offset += chunk->length;
		chunk->data = ptr;
	}

	return XB_STREAM_READ_CHUNK;

err:
	return XB_STREAM_READ_ERROR;
}

int
xb_stream_read_done(xb_rstream_t *stream)
{
	my_free(stream);

	return 0;
}
