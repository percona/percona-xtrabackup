#ifndef __KDF_H__
#define __KDF_H__

#include <univ.i>

const int XB_KDF_SALT_SIZE = 64;

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
	      const byte *salt, size_t saltlen, size_t keysize, byte *keybuffer);

#endif
