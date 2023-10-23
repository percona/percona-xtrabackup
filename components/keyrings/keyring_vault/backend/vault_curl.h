/* Copyright (c) 2018, 2021 Percona LLC and/or its affiliates. All rights
   reserved.

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

#ifndef KEYRING_VAULT_CURL_INCLUDED
#define KEYRING_VAULT_CURL_INCLUDED

#include "i_vault_curl.h"

#include <components/keyrings/common/data/pfs_string.h>
#include <components/keyrings/keyring_vault/config/config.h>

#include <curl/curl.h>

namespace keyring_common {

namespace meta {
class Metadata;
}  // namespace meta

namespace data {
class Data;
}  // namespace data

}  // namespace keyring_common

namespace keyring_vault {
namespace backend {

using keyring_common::data::Comp_keyring_alloc;
using keyring_common::data::Data;
using keyring_common::meta::Metadata;
using keyring_vault::config::Vault_version_type;

class Keyring_vault_curl final : public IKeyring_vault_curl {
 public:
  Keyring_vault_curl(config::Config_pod *config)
      : m_list{nullptr},
        m_config{config},
        m_mount_point_path{},
        m_directory_path{},
        m_resolved_secret_mount_point_version{config::Vault_version_unknown} {}

  ~Keyring_vault_curl() override;

  bool init() override;
  bool list_keys(pfs_string *response) override;
  bool write_key(const Metadata &key, const Data &data,
                 pfs_string *response) override;
  bool read_key(const Metadata &key, pfs_string *response) override;
  bool delete_key(const Metadata &key, pfs_string *response) override;

  Vault_version_type get_resolved_secret_mount_point_version() const override {
    return m_resolved_secret_mount_point_version;
  }

  static pfs_string get_errors_from_response(const pfs_string &json_response);

 private:
  bool setup_curl_session(CURL *curl);
  std::string get_error_from_curl(CURLcode curl_code);
  static void create_key_signature(const Metadata &key,
                                   pfs_string *key_signature) noexcept;
  static bool encode_key_signature(const Metadata &key,
                                   pfs_string *encoded_key_signature);
  bool get_key_url(const Metadata &key, pfs_string *key_url);
  bool probe_mount_point_config(const pfs_string &partial_path,
                                pfs_string &response);

  pfs_string get_secret_url_metadata();
  pfs_string get_secret_url_data();
  pfs_string get_secret_url(const pfs_string &type_of_data);

  char m_curl_errbuf[CURL_ERROR_SIZE];  // error from CURL
  pfs_secure_ostringstream m_read_data_ss;
  struct curl_slist *m_list;

  config::Config_pod *m_config;
  pfs_string m_mount_point_path;
  pfs_string m_directory_path;
  Vault_version_type m_resolved_secret_mount_point_version;
};

}  // namespace backend
}  // namespace keyring_vault

#endif  // KEYRING_VAULT_CURL_INCLUDED
