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

#include "vault_environment.h"

#include <curl/curl.h>

#include "generate_credential_file.h"

/*static*/ Vault_environment *Vault_environment::instance_= NULL;

Vault_environment::Vault_environment()
    : uuid_(generate_uuid()),
      key1_id_(uuid_ + "key1"),
      key2_id_(uuid_ + "key2"),
      default_conf_file_name_(get_conf_file_name("keyring_vault")),
      invalid_conf_file_name_(get_conf_file_name("invalid_token")),
      non_existing_conf_file_name_(get_conf_file_name("non_existing")),
      mount_point_path_("mtr/" + uuid_),
      admin_token_(extract_admin_token())
{
}

/*static*/ std::string Vault_environment::get_key_signature_ex(
    const std::string &uuid, const std::string &key_id,
    const std::string &user)
{
  std::string        id= uuid + key_id;
  std::ostringstream signature;
  signature << id.length() << '_' << id << user.length() << '_' << user;
  return signature.str();
}

/*virtual*/ void Vault_environment::SetUp()
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

// Override this to define how to tear down the environment.
/*virtual*/ void Vault_environment::TearDown() { curl_global_cleanup(); }
