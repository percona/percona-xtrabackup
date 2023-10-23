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

#include "vault_curl.h"

#include "vault_base64.h"
#include "vault_parser_composer.h"

#include <components/keyrings/common/data/data.h>
#include <components/keyrings/common/data/meta.h>
#include <mysql/components/services/log_builtins.h> /* LogComponentErr */
#include <mysqld_error.h>

#include <boost/core/noncopyable.hpp>

#include <curl/curl.h>

#include <algorithm>
#include <iostream>

namespace {

const char data_subpath[] = "data";
const char metadata_subpath[] = "metadata";
const char config_subpath[] = "config";

const char mount_point_path_delimiter = '/';

}  // anonymous namespace

namespace keyring_vault::backend {

static const std::size_t max_response_size = 32000000;

class Curl_session_guard : private boost::noncopyable {
 public:
  explicit Curl_session_guard(CURL *curl) noexcept : m_curl{curl} {}
  ~Curl_session_guard() {
    if (m_curl != nullptr) curl_easy_cleanup(m_curl);
  }

 private:
  CURL *m_curl;
};

static size_t write_response_memory(void *contents, size_t size, size_t nmemb,
                                    void *userp) noexcept {
  size_t realsize = size * nmemb;
  if (size != 0 && realsize / size != nmemb) return 0;  // overflow
  auto *read_data = static_cast<pfs_secure_ostringstream *>(userp);
  size_t ss_pos = read_data->tellp();
  read_data->seekp(0, std::ios::end);
  size_t number_of_read_bytes = read_data->tellp();
  read_data->seekp(ss_pos);

  if (number_of_read_bytes + realsize > max_response_size)
    return 0;  // response size limit exceeded

  read_data->write(static_cast<char *>(contents), realsize);
  if (!read_data->good()) return 0;
  return realsize;
}

Keyring_vault_curl::~Keyring_vault_curl() {
  if (m_list != nullptr) curl_slist_free_all(m_list);
}

std::string Keyring_vault_curl::get_error_from_curl(CURLcode curl_code) {
  size_t len = strlen(m_curl_errbuf);
  std::ostringstream ss;
  if (curl_code != CURLE_OK) {
    ss << "CURL returned this error code: " << curl_code;
    ss << " with error message : ";
    if (len > 0)
      ss << m_curl_errbuf;
    else
      ss << curl_easy_strerror(curl_code);
  }
  return ss.str();
}

pfs_string Keyring_vault_curl::get_secret_url(const pfs_string &type_of_data) {
  pfs_secure_ostringstream oss_data;

  assert(!m_config->vault_url.empty());
  oss_data << m_config->vault_url << "/v1/";
  if (m_resolved_secret_mount_point_version == config::Vault_version_v2) {
    oss_data << m_mount_point_path << '/' << type_of_data;
    if (!m_directory_path.empty()) {
      oss_data << '/' << m_directory_path;
    }
  } else {
    oss_data << m_config->secret_mount_point;
  }

  return oss_data.str();
}

pfs_string Keyring_vault_curl::get_secret_url_metadata() {
  return get_secret_url(metadata_subpath);
}

pfs_string Keyring_vault_curl::get_secret_url_data() {
  return get_secret_url(data_subpath) + '/';
}

bool Keyring_vault_curl::init() {
  if (m_config->secret_mount_point_version == config::Vault_version_v1) {
    m_resolved_secret_mount_point_version =
        m_config->secret_mount_point_version;
    return false;
  }

  std::size_t max_versions{0};
  bool cas_required{false};
  pfs_optional_string delete_version_after;

  pfs_string::const_iterator bg = m_config->secret_mount_point.begin();
  pfs_string::const_iterator en = m_config->secret_mount_point.end();
  pfs_string::const_iterator delimiter_it = bg;
  pfs_string::const_iterator from_it;
  pfs_string json_response;

  Vault_version_type mp_version = config::Vault_version_v1;
  pfs_string partial_path;

  while (delimiter_it != en && mp_version == config::Vault_version_v1) {
    from_it = delimiter_it;
    ++from_it;
    delimiter_it = std::find(from_it, en, mount_point_path_delimiter);
    partial_path.assign(bg, delimiter_it);
    pfs_string err_msg = "Probing ";
    err_msg += partial_path;
    err_msg += " for being a mount point";

    if (probe_mount_point_config(partial_path, json_response)) {
      err_msg += " unsuccessful - skipped.";
      LogComponentErr(INFORMATION_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                      err_msg.c_str());
    } else if (Keyring_vault_parser_composer::parse_mount_point_config(
                   json_response, max_versions, cas_required,
                   delete_version_after)) {
      err_msg += " successful but response has unexpected format - skipped.";
      LogComponentErr(WARNING_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                      err_msg.c_str());
    } else {
      err_msg += " successful - identified kv-v2 secret engine.";
      LogComponentErr(INFORMATION_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                      err_msg.c_str());
      mp_version = config::Vault_version_v2;
    }
  }

  if (m_config->secret_mount_point_version == config::Vault_version_v2 &&
      mp_version != config::Vault_version_v2) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Auto-detected mount point version is not the same as "
                    "specified in 'secret_mount_point_version'.");
    return true;
  }

  pfs_string mount_point_path;
  pfs_string directory_path;

  if (mp_version == config::Vault_version_v2) {
    mount_point_path.swap(partial_path);
    if (delimiter_it != en) {
      ++delimiter_it;
      directory_path.assign(delimiter_it, en);
    }
  }

  m_resolved_secret_mount_point_version = mp_version;
  m_mount_point_path.swap(mount_point_path);
  m_directory_path.swap(directory_path);

  return false;
}

bool Keyring_vault_curl::setup_curl_session(CURL *curl) {
  CURLcode curl_res = CURLE_OK;
  m_read_data_ss.str("");
  m_read_data_ss.clear();
  m_curl_errbuf[0] = '\0';

  if (m_list != nullptr) {
    curl_slist_free_all(m_list);
    m_list = nullptr;
  }

  pfs_string token_header = "X-Vault-Token:" + m_config->token;

  if ((m_list = curl_slist_append(m_list, token_header.c_str())) == nullptr ||
      (m_list = curl_slist_append(m_list, "Content-Type: application/json")) ==
          nullptr ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, m_curl_errbuf)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                   write_response_memory)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_WRITEDATA,
                                   static_cast<void *>(&m_read_data_ss))) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_list)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L)) !=
          CURLE_OK ||
      (!m_config->vault_ca.empty() &&
       (curl_res = curl_easy_setopt(curl, CURLOPT_CAINFO,
                                    m_config->vault_ca.c_str())) != CURLE_OK) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
                                   m_config->timeout)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_config->timeout)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_HTTP_VERSION,
                                   (long)CURL_HTTP_VERSION_1_1)) != CURLE_OK) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    get_error_from_curl(curl_res).c_str());
    return true;
  }

  return false;
}

bool Keyring_vault_curl::list_keys(pfs_string *response) {
  pfs_string url_to_list = get_secret_url_metadata() + "?list=true";
  CURLcode curl_res = CURLE_OK;
  long http_code = 0;

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, url_to_list.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK ||
      (curl_res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                    &http_code)) != CURLE_OK) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    get_error_from_curl(curl_res).c_str());
    return true;
  }
  if (http_code == 404) {
    *response = "";  // list returned empty list
    return false;
  }
  *response = m_read_data_ss.str();
  return http_code / 100 != 2;  // 2** are success return codes
}

void Keyring_vault_curl::create_key_signature(
    const Metadata &key, pfs_string *key_signature) noexcept {
  key_signature->clear();

  if (key.valid()) {
    // key_signature =
    // lengthof(key_id)||_||key_id||lengthof(user_id)||_||user_id
    pfs_secure_ostringstream key_signature_ss;
    key_signature_ss << key.key_id().length() << '_';
    key_signature_ss << key.key_id();
    key_signature_ss << key.owner_id().length() << '_';
    key_signature_ss << key.owner_id();
    key_signature->append(key_signature_ss.str());
  }
}

bool Keyring_vault_curl::encode_key_signature(
    const Metadata &key, pfs_string *encoded_key_signature) {
  pfs_string key_signature;
  create_key_signature(key, &key_signature);

  if (key_signature.empty()) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Could not compose key's signature");
    return true;
  }

  if (Vault_base64::encode(key_signature.c_str(), key_signature.length(),
                           encoded_key_signature,
                           Vault_base64::Format::SINGLE_LINE)) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Could not encode key's signature in base64");
    return true;
  }

  return false;
}

bool Keyring_vault_curl::get_key_url(const Metadata &key, pfs_string *key_url) {
  pfs_string encoded_key_signature;
  if (encode_key_signature(key, &encoded_key_signature)) return true;
  *key_url = get_secret_url_data() + encoded_key_signature;
  return false;
}

bool Keyring_vault_curl::probe_mount_point_config(
    const pfs_string &partial_path, pfs_string &response) {
  pfs_string config_url = m_config->vault_url;
  config_url += "/v1/";
  config_url += partial_path;
  config_url += '/';
  config_url += config_subpath;

  CURLcode curl_res = CURLE_OK;
  long http_code = 0;

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, config_url.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK ||
      (curl_res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                    &http_code)) != CURLE_OK) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    get_error_from_curl(curl_res).c_str());
    return true;
  }

  response = m_read_data_ss.str();

  return http_code / 100 != 2;
}

bool Keyring_vault_curl::write_key(const Metadata &key, const Data &data,
                                   pfs_string *response) {
  pfs_string encoded_key_data;
  auto decoded_data = data.data().decode();

  if (Vault_base64::encode(decoded_data.c_str(), decoded_data.length(),
                           &encoded_key_data,
                           Vault_base64::Format::SINGLE_LINE)) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Could not encode a key in base64");
    return true;
  }

  CURLcode curl_res = CURLE_OK;
  pfs_string post_data;
  if (Keyring_vault_parser_composer::compose_write_key_postdata(
          data, encoded_key_data, m_resolved_secret_mount_point_version,
          post_data))
    return true;

  pfs_string key_url;
  if (get_key_url(key, &key_url)) return true;

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, key_url.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                                   post_data.c_str())) != CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    get_error_from_curl(curl_res).c_str());
    return true;
  }

  *response = m_read_data_ss.str();

  return false;
}

bool Keyring_vault_curl::read_key(const Metadata &key, pfs_string *response) {
  pfs_string key_url;
  if (get_key_url(key, &key_url)) return true;
  CURLcode curl_res = CURLE_OK;
  CURL *curl = curl_easy_init();

  if (curl == nullptr) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Cannot initialize curl session");
    return true;
  }

  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, key_url.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    get_error_from_curl(curl_res).c_str());
    return true;
  }

  *response = m_read_data_ss.str();

  return false;
}

bool Keyring_vault_curl::delete_key(const Metadata &key, pfs_string *response) {
  pfs_string key_url;
  if (get_key_url(key, &key_url)) return true;
  CURLcode curl_res = CURLE_OK;
  CURL *curl = curl_easy_init();

  if (curl == nullptr) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    "Cannot initialize curl session");
    return true;
  }

  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, key_url.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE")) !=
          CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_LOGGER_ERROR_MSG,
                    get_error_from_curl(curl_res).c_str());
    return true;
  }

  *response = m_read_data_ss.str();

  return false;
}

pfs_string Keyring_vault_curl::get_errors_from_response(
    const pfs_string &json_response) {
  if (json_response.empty()) return pfs_string();

  pfs_string errors_from_response;
  pfs_string err_msg;

  if (Keyring_vault_parser_composer::parse_errors(json_response,
                                                  &errors_from_response)) {
    err_msg = " Error while parsing error messages";
  } else if (!errors_from_response.empty()) {
    err_msg =
        " Vault has returned the following error(s): " + errors_from_response;
  }

  return err_msg;
}

}  // namespace keyring_vault::backend
