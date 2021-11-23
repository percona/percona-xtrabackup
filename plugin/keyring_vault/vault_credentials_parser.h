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

#ifndef MYSQL_VAULT_CREDENTIALS_PARSER_H
#define MYSQL_VAULT_CREDENTIALS_PARSER_H

#include <cstddef>

#include <string>

namespace keyring {
class ILogger;
class Vault_credentials;

class Vault_credentials_parser {
 public:
  Vault_credentials_parser(ILogger *logger) : logger_(logger) {}

  bool parse(const std::string &conf_file_path,
             Vault_credentials &vault_credentials) const;

 private:
  ILogger *logger_;

  static const std::size_t max_file_size= 16384;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_CREDENTIALS_PARSER_H
