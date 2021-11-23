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

#ifndef MYSQL_VAULT_IO_H
#define MYSQL_VAULT_IO_H

#include <my_global.h>

#include <boost/core/noncopyable.hpp>

#include "i_vault_io.h"
#include "vault_key_serializer.h"
#include "secure_string.h"

namespace keyring {

class ILogger;
class IVault_curl;
class IVault_parser_composer;

class Vault_io : public IVault_io, private boost::noncopyable {
 public:
  Vault_io(ILogger *logger, IVault_curl *vault_curl,
           IVault_parser_composer *vault_parser)
      : logger(logger), vault_curl(vault_curl), vault_parser(vault_parser)
  {
  }

  ~Vault_io();

  virtual my_bool retrieve_key_type_and_data(IKey *key);

  virtual my_bool init(const std::string *keyring_storage_url);
  virtual my_bool flush_to_backup(ISerialized_object *serialized_object)
  {
    return FALSE;  // we do not have backup storage in vault
  }
  virtual my_bool flush_to_storage(ISerialized_object *serialized_object);

  virtual ISerializer *get_serializer();
  virtual my_bool      get_serialized_object(
           ISerialized_object **serialized_object);
  virtual my_bool has_next_serialized_object() { return FALSE; }
  virtual void    set_curl_timeout(uint timeout);

 private:
  bool          write_key(const Vault_key &key);
  bool          delete_key(const Vault_key &key);
  Secure_string get_errors_from_response(const Secure_string &json_response);

  ILogger *               logger;
  IVault_curl *           vault_curl;
  IVault_parser_composer *vault_parser;
  Vault_key_serializer    vault_key_serializer;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_IO_H
