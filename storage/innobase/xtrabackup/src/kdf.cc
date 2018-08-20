/******************************************************
Copyright (c) 2018 Percona LLC and/or its affiliates.

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

#include <gcrypt.h>
#include "kdf.h"

const int XB_KDF_ITERATIONS = 32768;

#if !defined(GCRYPT_VERSION_NUMBER) || (GCRYPT_VERSION_NUMBER < 0x010500)

/* kdf.c  - Key Derivation Functions
 * Copyright (C) 1998, 2011 Free Software Foundation, Inc.
 * Copyright (C) 2013 g10 Code GmbH
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser general Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Transform a passphrase into a suitable key of length KEYSIZE and
   store this key in the caller provided buffer KEYBUFFER.  The caller
   must provide PRFALGO which indicates the pseudorandom function to
   use: This shall be the algorithms id of a hash algorithm; it is
   used in HMAC mode.  SALT is a salt of length SALTLEN and ITERATIONS
   gives the number of iterations.  */
gcry_error_t
_gcry_kdf_pkdf2 (const void *passphrase, size_t passphraselen,
                 int hashalgo,
                 const void *salt, size_t saltlen,
                 unsigned long iterations,
                 size_t keysize, void *keybuffer)
{
  gcry_error_t ec;
  gcry_md_hd_t md;
  int secmode;
  unsigned long dklen = keysize;
  char *dk = (char *) keybuffer;
  unsigned int hlen;   /* Output length of the digest function.  */
  unsigned int l;      /* Rounded up number of blocks.  */
  unsigned int r;      /* Number of octets in the last block.  */
  char *sbuf;          /* Malloced buffer to concatenate salt and iter
                          as well as space to hold TBUF and UBUF.  */
  char *tbuf;          /* Buffer for T; ptr into SBUF, size is HLEN. */
  char *ubuf;          /* Buffer for U; ptr into SBUF, size is HLEN. */
  unsigned int lidx;   /* Current block number.  */
  unsigned long iter;  /* Current iteration number.  */
  unsigned int i;

  /* We allow for a saltlen of 0 here to support scrypt.  It is not
     clear whether rfc2898 allows for this this, thus we do a test on
     saltlen > 0 only in gcry_kdf_derive.  */
  if (!salt || !iterations || !dklen)
    return GPG_ERR_INV_VALUE;

  hlen = gcry_md_get_algo_dlen (hashalgo);
  if (!hlen)
    return GPG_ERR_DIGEST_ALGO;

  secmode = gcry_is_secure (passphrase) || gcry_is_secure (keybuffer);

  /* Step 1 */
  /* If dkLen > (2^32 - 1) * hLen, output "derived key too long" and
   * stop.  We use a stronger inequality but only if our type can hold
   * a larger value.  */

#if SIZEOF_UNSIGNED_LONG > 4
  if (dklen > 0xffffffffU)
    return GPG_ERR_INV_VALUE;
#endif


  /* Step 2 */
  l = ((dklen - 1)/ hlen) + 1;
  r = dklen - (l - 1) * hlen;

  /* Setup buffers and prepare a hash context.  */
  sbuf = (char *) (secmode
          ? gcry_malloc_secure (saltlen + 4 + hlen + hlen)
          : gcry_malloc (saltlen + 4 + hlen + hlen));
  if (!sbuf)
    return gpg_err_code_from_syserror ();
  tbuf = sbuf + saltlen + 4;
  ubuf = tbuf + hlen;

  ec = gcry_md_open (&md, hashalgo, (GCRY_MD_FLAG_HMAC
                                     | (secmode?GCRY_MD_FLAG_SECURE:0)));
  if (ec)
    {
      gcry_free (sbuf);
      return ec;
    }

  ec = gcry_md_setkey (md, passphrase, passphraselen);
  if (ec)
    {
      gcry_md_close (md);
      gcry_free (sbuf);
      return ec;
    }

  /* Step 3 and 4. */
  memcpy (sbuf, salt, saltlen);
  for (lidx = 1; lidx <= l; lidx++)
    {
      for (iter = 0; iter < iterations; iter++)
        {
          gcry_md_reset (md);
          if (!iter) /* Compute U_1:  */
            {
              sbuf[saltlen]     = (lidx >> 24);
              sbuf[saltlen + 1] = (lidx >> 16);
              sbuf[saltlen + 2] = (lidx >> 8);
              sbuf[saltlen + 3] = lidx;
              gcry_md_write (md, sbuf, saltlen + 4);
              memcpy (ubuf, gcry_md_read (md, 0), hlen);
              memcpy (tbuf, ubuf, hlen);
            }
          else /* Compute U_(2..c):  */
            {
              gcry_md_write (md, ubuf, hlen);
              memcpy (ubuf, gcry_md_read (md, 0), hlen);
              for (i=0; i < hlen; i++)
                tbuf[i] ^= ubuf[i];
            }
        }
      if (lidx == l)  /* Last block.  */
        memcpy (dk, tbuf, r);
      else
        {
          memcpy (dk, tbuf, hlen);
          dk += hlen;
        }
    }

  gcry_md_close (md);
  gcry_free (sbuf);
  return 0;
}

/** Derive key from the passphrase using PBKDF2.
@param[in]	passphrase	passphrase.
@param[in]	passphraselen	passphrase length.
@param[in]	salt		random salt.
@param[in]	saltlen		random salt length.
@param[in]	keysize		desired derived key length.
@param[out]	keybuffer	buffer to store derived key.
@return	true on success. */
bool
xb_derive_key(const char *passphrase, size_t passphraselen,
	      const byte *salt, size_t saltlen, size_t keysize, byte *keybuffer)
{
	gpg_error_t gpg_error;

	gpg_error = _gcry_kdf_pkdf2(passphrase, passphraselen,
			GCRY_MD_SHA256, salt, saltlen,
			XB_KDF_ITERATIONS, keysize, keybuffer);

	if (gpg_error != 0) {
		return(false);
	}

	return(true);
}

#else

/** Derive key from the passphrase using PBKDF2.
@param[in]	passphrase	passphrase.
@param[in]	passphraselen	passphrase length.
@param[in]	salt		random salt.
@param[in]	saltlen		random salt length.
@param[in]	keysize		desired derived key length.
@param[out]	keybuffer	buffer to store derived key.
@return	true on success. */
bool
xb_derive_key(const char *passphrase, size_t passphraselen,
	      const byte *salt, size_t saltlen, size_t keysize, byte *keybuffer)
{
	gpg_error_t gpg_error;

	gpg_error = gcry_kdf_derive(passphrase, passphraselen,
			GCRY_KDF_PBKDF2, GCRY_MD_SHA256, salt, saltlen,
			XB_KDF_ITERATIONS, keysize, keybuffer);

	if (gpg_error != 0) {
		return(false);
	}

	return(true);
}

#endif

