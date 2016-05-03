/*****************************************************************************

Copyright (C) 2016, MariaDB Corporation. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/
/**************************************************//**
@file mariadb.cc
Helper functions to check MariaDB 10.1 extended features.

Jan Lindström jan.lindstrom@mariadb.com
*******************************************************/

#include "univ.i"
#include "log0log.h"
#include "common.h"
#include "fil0fil.h"
#include "fsp0fsp.h"

static const byte redo_log_purpose_byte = 0x02;

#define MY_AES_BLOCK_SIZE 16
#define LOG_CRYPT_ENTRY_SIZE            (4 + 4 + 2 * MY_AES_BLOCK_SIZE)
#define LOG_CRYPT_VER		        308

#define FIL_PAGE_PAGE_COMPRESSED_ENCRYPTED 37401 /*!< Page is compressed and
						 then encrypted */
#define FIL_PAGE_PAGE_COMPRESSED 34354  /*!< Page compressed page */

#define FIL_PAGE_FILE_FLUSH_LSN_OR_KEY_VERSION 26 /*!< for the first page
					in a system tablespace data file
					(ibdata*, not *.ibd): the file has
					been flushed to disk at least up
					to this lsn
					for other pages: a 32-bit key version
					used to encrypt the page + 32-bit checksum
					or 64 bits of zero if no encryption
					*/
/**
* Magic pattern in start of crypt data on page 0
*/
#define MAGIC_SZ 6

static const unsigned char CRYPT_MAGIC[MAGIC_SZ] = {
	's', 0xE, 0xC, 'R', 'E', 't' };

static const unsigned char EMPTY_PATTERN[MAGIC_SZ] = {
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

#define CRYPT_SCHEME_1 1
#define CRYPT_SCHEME_1_IV_LEN 16
#define CRYPT_SCHEME_UNENCRYPTED 0

extern void xtrabackup_io_throttling(void);

/********************************************************//**
Read from checkpoint is redo log encrypted.
@return	true if log is encrypted, false if not. */
bool
mariadb_check_encryption(
/*=====================*/
	log_group_t*	group)
{
	ib_uint64_t	checkpoint_no;
	ulint		field;
	bool		encrypted = false;

	for (field = LOG_CHECKPOINT_1; field <= LOG_CHECKPOINT_2;
	     field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {

		xtrabackup_io_throttling();

		mutex_enter(&log_sys->mutex);
		log_group_read_checkpoint_info(group, field);

		byte* buf = log_sys->checkpoint_buf;
		checkpoint_no = mach_read_from_8(
				buf + LOG_CHECKPOINT_NO);
		buf+=LOG_CRYPT_VER;
		byte scheme = buf[0];

		if (scheme == redo_log_purpose_byte) {
			buf++;
			size_t n = buf[0];
			buf++;
			for (size_t i = 0; i < n; i++) {
				ulint key_version = mach_read_from_4(buf + 4);
				buf += LOG_CRYPT_ENTRY_SIZE;

				if (key_version != 0) {
					msg("xtrabackup: Redo log encrypted on checkpoint %lu key_version %lu\n",
						checkpoint_no, key_version);
					encrypted = true;
				}
			}
		}
		mutex_exit(&log_sys->mutex);
	}

	return(encrypted);
}

/********************************************************//**
Check from page type is page compressed.
@return	true if page is compressed, false if not. */
bool
mariadb_check_compression(
/*======================*/
	const byte*	page)
{
	ulint page_type = mach_read_from_2(page+FIL_PAGE_TYPE);

	return (page_type == FIL_PAGE_PAGE_COMPRESSED ||
		page_type == FIL_PAGE_PAGE_COMPRESSED_ENCRYPTED);
}

/**********************************************************************//**
Compute offset after xdes where crypt data can be stored
@return	offset */
static
ulint
fsp_header_get_crypt_offset(
/*========================*/
	ulint   zip_size, /*!< in: zip_size */
	ulint*  max_size) /*!< out: free space available for crypt data */
{
	ulint pageno = 0;
	/* compute first page_no that will have xdes stored on page != 0*/
	for (ulint i = 0;
	     (pageno = xdes_calc_descriptor_page(zip_size, i)) == 0; )
		i++;

	/* use pageno prior to this...i.e last page on page 0 */
	ut_ad(pageno > 0);
	pageno--;

	ulint iv_offset = XDES_ARR_OFFSET +
		XDES_SIZE * (1 + xdes_calc_descriptor_index(zip_size, pageno));

	if (max_size != NULL) {
		/* return how much free space there is available on page */
		*max_size = (zip_size ? zip_size : UNIV_PAGE_SIZE) -
			(FSP_HEADER_OFFSET + iv_offset + FIL_PAGE_DATA_END);
	}

	return FSP_HEADER_OFFSET + iv_offset;
}

/********************************************************//**
Check from tablespace page 0 is tablespace encrypted.
@return	true if tablespace is encrypted, false if not. */
bool
mariadb_check_tablespace_encryption(
/*================================*/
	os_file_t	fd,
	const char*	name,
	ulint		zip_size)
{
	byte*		buf;
	byte*		page;
	ulint		maxsize=0;
	os_offset_t	offset = fsp_header_get_crypt_offset(zip_size, &maxsize);

	/* Allocate memory for buffer and read page 0 from datafile */
	buf = static_cast<byte*>(ut_malloc(2 * UNIV_PAGE_SIZE));
	page = static_cast<byte*>(ut_align(buf, UNIV_PAGE_SIZE));
	os_file_read(fd, page, 0, UNIV_PAGE_SIZE);

	if (memcmp(page + offset, EMPTY_PATTERN, MAGIC_SZ) == 0 ||
	    memcmp(page + offset, CRYPT_MAGIC, MAGIC_SZ) != 0) {
		return false;
	}

	ulint type = mach_read_from_1(page + offset + MAGIC_SZ + 0);

	if (! (type == CRYPT_SCHEME_UNENCRYPTED ||
	       type == CRYPT_SCHEME_1)) {
		return false;
	}

	ulint iv_length = mach_read_from_1(page + offset + MAGIC_SZ + 1);
	if (iv_length != CRYPT_SCHEME_1_IV_LEN) {
		return false;
	}
	uint min_key_version = mach_read_from_4
		(page + offset + MAGIC_SZ + 2 + iv_length);

	uint key_id = mach_read_from_4
		(page + offset + MAGIC_SZ + 2 + iv_length + 4);

	if (type == CRYPT_SCHEME_1) {
		msg("xtrabackup: Tablespace %s encrypted key_version %u key_id %u\n",
			name, min_key_version, key_id);
	}

	ut_free(buf);
	return (type == CRYPT_SCHEME_1);
}

