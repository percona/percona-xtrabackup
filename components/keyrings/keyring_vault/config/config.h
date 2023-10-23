/* Copyright (c) 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef KEYRING_VAULT_CONFIG_INCLUDED
#define KEYRING_VAULT_CONFIG_INCLUDED

#include "components/keyrings/common/data/pfs_string.h"

#include <memory>
#include <string>
#include <vector>

namespace keyring_vault {
namespace config {

/* Component path */
extern char *g_component_path;

/* Instance path */
extern char *g_instance_path;

/* Config details */
enum Vault_version_type {
  Vault_version_unknown,
  Vault_version_v1,
  Vault_version_v2,
  Vault_version_auto
};

class Config_pod {
 public:
  uint timeout;
  pfs_string vault_url;
  pfs_string secret_mount_point;
  pfs_string vault_ca;
  pfs_string token;
  Vault_version_type secret_mount_point_version = Vault_version_unknown;
};

/**
  Get path to global configuration file

  @param [out] full_path Configuration file path

  @returns status of read operation
    @retval false Success
    @retval true  Failure
*/
bool get_global_config_path(std::string &full_path);

/**
  Get path to instance configuration file

  @param [out] full_path Configuration file path

  @returns status of read operation
    @retval false Success
    @retval true  Failure
*/
bool get_local_config_path(std::string &full_path);

/**
  Read configuration file

  @param [out] config_pod Configuration details

  @returns status of read operation
    @retval false Success
    @retval true  Failure
*/
bool find_and_read_config_file(std::unique_ptr<Config_pod> &config_pod);

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
}  // namespace config
}  // namespace keyring_vault

#endif  // !KEYRING_VAULT_CONFIG_INCLUDED
