/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MYSQL_VAULT_CURL_H
#define MYSQL_VAULT_CURL_H

#include <my_global.h>
#include <boost/core/noncopyable.hpp>
#include <curl/curl.h>
#include <sstream>
#include "vault_key.h"
#include "i_vault_curl.h"
#include "logger.h"
#include "vault_credentials.h"
#include "secure_string.h"
#include "i_keyring_key.h"

namespace keyring
{

class Vault_curl : public IVault_curl, private boost::noncopyable
{
public:
  Vault_curl(ILogger *logger, uint timeout)
    : logger(logger)
    , list(NULL)
    , timeout(timeout)
  {}

  ~Vault_curl()
  {
    if (list != NULL)
      curl_slist_free_all(list);
  }

  virtual bool init(const Vault_credentials &vault_credentials);
  virtual bool list_keys(Secure_string *response);
  virtual bool write_key(const Vault_key &key, Secure_string *response);
  virtual bool read_key(const Vault_key &key, Secure_string *response);
  virtual bool delete_key(const Vault_key &key, Secure_string *response);
  virtual void set_timeout(uint timeout)
  {
    this->timeout = timeout; 
  }

private:

  bool setup_curl_session(CURL *curl);
  std::string get_error_from_curl(CURLcode curl_code);
  bool encode_key_signature(const Vault_key &key, Secure_string *encoded_key_signature);
  bool get_key_url(const Vault_key &key, Secure_string *key_url);

  ILogger *logger;
  Secure_string token_header;
  Secure_string vault_url;
  char curl_errbuf[CURL_ERROR_SIZE]; //error from CURL
  Secure_ostringstream read_data_ss;
  struct curl_slist *list;
  Secure_string vault_ca;
  uint timeout;
};

} //namespace keyring

#endif //MYSQL_VAULT_CURL_H
