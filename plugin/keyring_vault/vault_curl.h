/* Copyright (c) 2018, 2021 Percona LLC and/or its affiliates. All rights reserved.

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

#include <sstream>

#include <boost/core/noncopyable.hpp>

#include <curl/curl.h>

#include "i_vault_curl.h"
#include "secure_string.h"
#include "vault_credentials.h"
#include "vault_key.h"

namespace keyring {

class ILogger;
class IVault_parser_composer;

class Vault_curl : public IVault_curl, private boost::noncopyable {
 public:
  Vault_curl(ILogger *logger, IVault_parser_composer *parser, uint timeout)
      : logger_(logger),
        parser_(parser),
        list(NULL),
        timeout(timeout),
        vault_credentials_(),
        mount_point_path_(),
        directory_path_(),
        resolved_secret_mount_point_version_(Vault_version_unknown)
  {
  }

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
  virtual void set_timeout(uint timeout) { this->timeout= timeout; }

  const Vault_credentials &get_vault_credentials() const
  {
    return vault_credentials_;
  }
  const Secure_string &get_mount_point_path() const
  {
    return mount_point_path_;
  }
  const Secure_string &get_directory_path() const { return directory_path_; }
  virtual Vault_version_type get_resolved_secret_mount_point_version() const
  {
    return resolved_secret_mount_point_version_;
  }

 private:
  bool        setup_curl_session(CURL *curl);
  std::string get_error_from_curl(CURLcode curl_code);
  bool        encode_key_signature(const Vault_key &key,
                                   Secure_string *  encoded_key_signature);
  bool        get_key_url(const Vault_key &key, Secure_string *key_url);
  bool        probe_mount_point_config(const Secure_string &partial_path,
                                       Secure_string &      response);

  Secure_string get_secret_url_metadata();
  Secure_string get_secret_url_data();
  Secure_string get_secret_url(const Secure_string &type_of_data);

  ILogger *               logger_;
  IVault_parser_composer *parser_;
  char                    curl_errbuf[CURL_ERROR_SIZE];  //error from CURL
  Secure_ostringstream    read_data_ss;
  struct curl_slist *     list;
  uint                    timeout;

  Vault_credentials  vault_credentials_;
  Secure_string      mount_point_path_;
  Secure_string      directory_path_;
  Vault_version_type resolved_secret_mount_point_version_;
};

}  //namespace keyring

#endif  //MYSQL_VAULT_CURL_H
