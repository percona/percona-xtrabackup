/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.
   Copyright (c) 2026 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PERCONA_KEYRING_ENCRYPTED_FILE_CONFIG_INCLUDED
#define PERCONA_KEYRING_ENCRYPTED_FILE_CONFIG_INCLUDED

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace percona_keyring_encrypted_file::config {

/* Component path */
extern char *g_component_path;

/* Instance path */
extern char *g_instance_path;

/* Config details */
class Config_pod {
 public:
  std::string config_file_path_;
  std::string password_;
  std::string password_file_;
  bool read_only_;
  uint32_t iterations_{600000};
};

/**
  Read configuration file

  @param [out] config_pod Configuration details
  @param [out] err        Error message

  @returns status of read operation
    @retval false Success
    @retval true  Failure
*/
bool find_and_read_config_file(std::unique_ptr<Config_pod> &config_pod,
                               std::string &err);

/**
  Create configuration vector

  @param [out] metadata Configuration data

  @returns status of read operation
    @retval false Success
    @retval true  Failure
*/
bool create_config(
    std::unique_ptr<std::vector<std::pair<std::string, std::string>>>
        &metadata);
}  // namespace percona_keyring_encrypted_file::config

#endif  // !PERCONA_KEYRING_ENCRYPTED_FILE_CONFIG_INCLUDED
