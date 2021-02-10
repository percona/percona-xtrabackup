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

#include "generate_credential_file.h"

#include <cstdlib>
#include <ostream>
#include <fstream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace {
const char vault_address_env_var_name[]= "MTR_VAULT_ADDRESS";
const char vault_admin_token_env_var_name[]= "MTR_VAULT_ADMIN_TOKEN";
const char vault_plugin_token_env_var_name[]= "MTR_VAULT_PLUGIN_TOKEN";
const char vault_ca_env_var_name[]= "MTR_VAULT_CA";
}  // anonymous namespace

std::ostream &operator<<(std::ostream &           os,
                         mount_point_version_type mount_point_version)
{
  const char *label= "";
  switch (mount_point_version)
  {
    case mount_point_version_empty: label= "<EMPTY>"; break;
    case mount_point_version_v1: label= "1"; break;
    case mount_point_version_v2: label= "2"; break;
    case mount_point_version_auto: label= "AUTO"; break;
  }
  return os << label;
}

std::string generate_uuid()
{
  return boost::uuids::to_string(boost::uuids::random_generator()());
}

bool generate_credential_file(
    const std::string &      credential_file_path,
    const std::string &      secret_mount_point,
    mount_point_version_type mount_point_version,
    credentials_validity_type
        generate_credetials /*= credentials_validity_correct*/)
{
  std::remove(credential_file_path.c_str());

  const char *imported_vault_conf_address=
      std::getenv(vault_address_env_var_name);
  if (imported_vault_conf_address == NULL)
    return true;

  const char *imported_vault_conf_token=
      std::getenv(vault_plugin_token_env_var_name);
  if (imported_vault_conf_token == NULL)
    return true;
  const char *imported_vault_conf_ca= std::getenv(vault_ca_env_var_name);

  std::ofstream credentials_ofs(credential_file_path.c_str());
  if (!credentials_ofs.is_open())
    return true;

  credentials_ofs << "vault_url = " << imported_vault_conf_address << '\n';
  credentials_ofs << "secret_mount_point = " << secret_mount_point << '\n';
  credentials_ofs << "token = "
                  << (generate_credetials ==
                              credentials_validity_invalid_token
                          ? "token = 123-123-123"
                          : imported_vault_conf_token)
                  << '\n';
  if (imported_vault_conf_ca != NULL)
  {
    credentials_ofs << "vault_ca = " << imported_vault_conf_ca << '\n';
  }
  if (mount_point_version != mount_point_version_empty)
  {
    credentials_ofs << "secret_mount_point_version = " << mount_point_version
                    << '\n';
  }

  return false;
}

bool is_vault_environment_configured()
{
  return std::getenv(vault_address_env_var_name) != NULL &&
         std::getenv(vault_admin_token_env_var_name) != NULL &&
         std::getenv(vault_plugin_token_env_var_name) != NULL;
}

std::string extract_admin_token()
{
  const char *imported_vault_admin_token=
      std::getenv(vault_admin_token_env_var_name);
  return imported_vault_admin_token == NULL
             ? std::string()
             : std::string(imported_vault_admin_token);
}
