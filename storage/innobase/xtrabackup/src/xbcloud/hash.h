/******************************************************
Copyright (c) 2019 Percona LLC and/or its affiliates.

Hahsing helpers.

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

#include <gcrypt.h>
#include <vector>

namespace xbcloud {

template <typename T>
std::vector<unsigned char> md5(const T &s) {
  unsigned int len = gcry_md_get_algo_dlen(GCRY_MD_MD5);
  std::vector<unsigned char> md(len);

  gcry_md_hd_t h;
  gcry_md_open(&h, GCRY_MD_MD5, GCRY_MD_FLAG_SECURE);
  gcry_md_write(h, &s[0], s.size());
  memcpy(&md[0], gcry_md_read(h, GCRY_MD_MD5), len);
  gcry_md_close(h);

  return md;
}

template <typename T>
std::vector<unsigned char> sha256(const T &s) {
  unsigned int len = gcry_md_get_algo_dlen(GCRY_MD_SHA256);
  std::vector<unsigned char> md(len);

  gcry_md_hd_t h;
  gcry_md_open(&h, GCRY_MD_SHA256, GCRY_MD_FLAG_SECURE);
  gcry_md_write(h, &s[0], s.size());
  memcpy(&md[0], gcry_md_read(h, GCRY_MD_SHA256), len);
  gcry_md_close(h);

  return md;
}

template <typename K, typename D>
std::vector<unsigned char> hmac_sha256(const K &key, const D &data) {
  unsigned int len = gcry_md_get_algo_dlen(GCRY_MD_SHA256);
  std::vector<unsigned char> md(len);

  gcry_md_hd_t h;
  gcry_md_open(&h, GCRY_MD_SHA256, GCRY_MD_FLAG_SECURE | GCRY_MD_FLAG_HMAC);
  gcry_md_setkey(h, &key[0], key.size());
  gcry_md_write(h, &data[0], data.size());
  memcpy(&md[0], gcry_md_read(h, GCRY_MD_SHA256), len);
  gcry_md_close(h);

  return md;
}

template <typename K>
std::vector<unsigned char> hmac_sha256(const K &key, const char *data) {
  unsigned int len = gcry_md_get_algo_dlen(GCRY_MD_SHA256);
  std::vector<unsigned char> md(len);

  gcry_md_hd_t h;
  gcry_md_open(&h, GCRY_MD_SHA256, GCRY_MD_FLAG_SECURE | GCRY_MD_FLAG_HMAC);
  gcry_md_setkey(h, &key[0], key.size());
  gcry_md_write(h, data, strlen(data));
  memcpy(&md[0], gcry_md_read(h, GCRY_MD_SHA256), len);
  gcry_md_close(h);

  return md;
}

template <typename K, typename D>
std::vector<unsigned char> hmac_sha1(const K &key, const D &data) {
  unsigned int len = gcry_md_get_algo_dlen(GCRY_MD_SHA1);
  std::vector<unsigned char> md(len);

  gcry_md_hd_t h;
  gcry_md_open(&h, GCRY_MD_SHA1, GCRY_MD_FLAG_SECURE | GCRY_MD_FLAG_HMAC);
  gcry_md_setkey(h, &key[0], key.size());
  gcry_md_write(h, &data[0], data.size());
  memcpy(&md[0], gcry_md_read(h, GCRY_MD_SHA1), len);
  gcry_md_close(h);

  return md;
}

template <typename K>
std::vector<unsigned char> hmac_sha1(const K &key, const char *data) {
  unsigned int len = gcry_md_get_algo_dlen(GCRY_MD_SHA1);
  std::vector<unsigned char> md(len);

  gcry_md_hd_t h;
  gcry_md_open(&h, GCRY_MD_SHA1, GCRY_MD_FLAG_SECURE | GCRY_MD_FLAG_HMAC);
  gcry_md_setkey(h, &key[0], key.size());
  gcry_md_write(h, data, strlen(data));
  memcpy(&md[0], gcry_md_read(h, GCRY_MD_SHA1), len);
  gcry_md_close(h);

  return md;
}

}  // namespace xbcloud
