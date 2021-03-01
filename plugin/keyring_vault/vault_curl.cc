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

#include <algorithm>

#include <boost/core/noncopyable.hpp>
#include <boost/optional.hpp>

#include <curl/curl.h>

#include "i_vault_parser_composer.h"
#include "plugin/keyring/common/logger.h"
#include "vault_base64.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace {

const char data_subpath[] = "data";
const char metadata_subpath[] = "metadata";
const char config_subpath[] = "config";

const char mount_point_path_delimiter = '/';

}  // anonymous namespace

namespace keyring {

static const std::size_t max_response_size = 32000000;

class Curl_session_guard : private boost::noncopyable {
 public:
  Curl_session_guard(CURL *curl) noexcept : curl(curl) {}
  ~Curl_session_guard() {
    if (curl != nullptr) curl_easy_cleanup(curl);
  }

 private:
  CURL *curl;
};

static size_t write_response_memory(void *contents, size_t size, size_t nmemb,
                                    void *userp) noexcept {
  size_t realsize = size * nmemb;
  if (size != 0 && realsize / size != nmemb) return 0;  // overflow
  auto *read_data = static_cast<Secure_ostringstream *>(userp);
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

std::string Vault_curl::get_error_from_curl(CURLcode curl_code) {
  size_t len = strlen(curl_errbuf);
  std::ostringstream ss;
  if (curl_code != CURLE_OK) {
    ss << "CURL returned this error code: " << curl_code;
    ss << " with error message : ";
    if (len)
      ss << curl_errbuf;
    else
      ss << curl_easy_strerror(curl_code);
  }
  return ss.str();
}

Secure_string Vault_curl::get_secret_url(const Secure_string &type_of_data) {
  Secure_ostringstream oss_data;

  assert(!vault_credentials_.get_vault_url().empty());
  oss_data << vault_credentials_.get_vault_url() << "/v1/";
  if (resolved_secret_mount_point_version_ == Vault_version_v2) {
    oss_data << mount_point_path_ << '/' << type_of_data;
    if (!directory_path_.empty()) {
      oss_data << '/' << directory_path_;
    }
  } else {
    oss_data << vault_credentials_.get_secret_mount_point();
  }

  return oss_data.str();
}

Secure_string Vault_curl::get_secret_url_metadata() {
  return get_secret_url(metadata_subpath);
}

Secure_string Vault_curl::get_secret_url_data() {
  return get_secret_url(data_subpath) + '/';
}

bool Vault_curl::init(const Vault_credentials &vault_credentials) {
  vault_credentials_ = vault_credentials;
  if (vault_credentials.get_secret_mount_point_version() == Vault_version_v1) {
    resolved_secret_mount_point_version_ =
        vault_credentials_.get_secret_mount_point_version();
    return false;
  }

  std::size_t max_versions;
  bool cas_required;
  Optional_secure_string delete_version_after;

  Secure_string::const_iterator bg =
      vault_credentials_.get_secret_mount_point().begin();
  Secure_string::const_iterator en =
      vault_credentials_.get_secret_mount_point().end();
  Secure_string::const_iterator delimiter_it = bg;
  Secure_string::const_iterator from_it;
  Secure_string json_response;

  Vault_version_type mp_version = Vault_version_v1;
  Secure_string partial_path;

  while (delimiter_it != en && mp_version == Vault_version_v1) {
    from_it = delimiter_it;
    ++from_it;
    delimiter_it = std::find(from_it, en, mount_point_path_delimiter);
    partial_path.assign(bg, delimiter_it);
    Secure_string err_msg = "Probing ";
    err_msg += partial_path;
    err_msg += " for being a mount point";

    if (probe_mount_point_config(partial_path, json_response)) {
      err_msg += " unsuccessful - skipped.";
      logger_->log(MY_INFORMATION_LEVEL, err_msg.c_str());
    } else if (parser_->parse_mount_point_config(json_response, max_versions,
                                                 cas_required,
                                                 delete_version_after)) {
      err_msg += " successful but response has unexpected format - skipped.";
      logger_->log(MY_WARNING_LEVEL, err_msg.c_str());
    } else {
      err_msg += " successful - identified kv-v2 secret engine.";
      logger_->log(MY_INFORMATION_LEVEL, err_msg.c_str());
      mp_version = Vault_version_v2;
    }
  }

  if (vault_credentials.get_secret_mount_point_version() == Vault_version_v2 &&
      mp_version != Vault_version_v2) {
    logger_->log(MY_ERROR_LEVEL,
                 "Auto-detected mount point version is not the same as "
                 "specified in 'secret_mount_point_version'.");
    return true;
  }
  Secure_string mount_point_path;
  Secure_string directory_path;
  if (mp_version == Vault_version_v2) {
    mount_point_path.swap(partial_path);
    if (delimiter_it != en) {
      ++delimiter_it;
      directory_path.assign(delimiter_it, en);
    }
  }

  resolved_secret_mount_point_version_ = mp_version;
  mount_point_path_.swap(mount_point_path);
  directory_path_.swap(directory_path);

  return false;
}

bool Vault_curl::setup_curl_session(CURL *curl) {
  CURLcode curl_res = CURLE_OK;
  read_data_ss.str("");
  read_data_ss.clear();
  curl_errbuf[0] = '\0';
  if (list != nullptr) {
    curl_slist_free_all(list);
    list = nullptr;
  }

  Secure_string token_header =
      "X-Vault-Token:" + vault_credentials_.get_token();
  if ((list = curl_slist_append(list, token_header.c_str())) == nullptr ||
      (list = curl_slist_append(list, "Content-Type: application/json")) ==
          nullptr ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                   write_response_memory)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_WRITEDATA,
                                   static_cast<void *>(&read_data_ss))) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L)) !=
          CURLE_OK ||
      (!vault_credentials_.get_vault_ca().empty() &&
       (curl_res = curl_easy_setopt(
            curl, CURLOPT_CAINFO, vault_credentials_.get_vault_ca().c_str())) !=
           CURLE_OK) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout)) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_HTTP_VERSION,
                                   (long)CURL_HTTP_VERSION_1_1)) != CURLE_OK) {
    logger_->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  return false;
}

bool Vault_curl::list_keys(Secure_string *response) {
  Secure_string url_to_list = get_secret_url_metadata() + "?list=true";
  CURLcode curl_res = CURLE_OK;
  long http_code = 0;

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    logger_->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, url_to_list.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK ||
      (curl_res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                    &http_code)) != CURLE_OK) {
    logger_->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  if (http_code == 404) {
    *response = "";  // list returned empty list
    return false;
  }
  *response = read_data_ss.str();
  return http_code / 100 != 2;  // 2** are success return codes
}

bool Vault_curl::encode_key_signature(const Vault_key &key,
                                      Secure_string *encoded_key_signature) {
  if (Vault_base64::encode(
          key.get_key_signature()->c_str(), key.get_key_signature()->length(),
          encoded_key_signature, Vault_base64::Format::SINGLE_LINE)) {
    logger_->log(MY_ERROR_LEVEL, "Could not encode key's signature in base64");
    return true;
  }
  return false;
}

bool Vault_curl::get_key_url(const Vault_key &key, Secure_string *key_url) {
  Secure_string encoded_key_signature;
  if (encode_key_signature(key, &encoded_key_signature)) return true;
  *key_url = get_secret_url_data() + encoded_key_signature;
  return false;
}

bool Vault_curl::probe_mount_point_config(const Secure_string &partial_path,
                                          Secure_string &response) {
  Secure_string config_url = vault_credentials_.get_vault_url();
  config_url += "/v1/";
  config_url += partial_path;
  config_url += '/';
  config_url += config_subpath;

  CURLcode curl_res = CURLE_OK;
  long http_code = 0;

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    logger_->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, config_url.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK ||
      (curl_res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                    &http_code)) != CURLE_OK) {
    logger_->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  response = read_data_ss.str();
  return http_code / 100 != 2;
}

bool Vault_curl::write_key(const Vault_key &key, Secure_string *response) {
  Secure_string encoded_key_data;
  if (Vault_base64::encode(reinterpret_cast<const char *>(key.get_key_data()),
                           key.get_key_data_size(), &encoded_key_data,
                           Vault_base64::Format::SINGLE_LINE)) {
    logger_->log(MY_ERROR_LEVEL, "Could not encode a key in base64");
    return true;
  }
  CURLcode curl_res = CURLE_OK;
  Secure_string postdata;
  if (parser_->compose_write_key_postdata(key, encoded_key_data,
                                          resolved_secret_mount_point_version_,
                                          postdata))
    return true;
  Secure_string key_url;
  if (get_key_url(key, &key_url)) return true;

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    logger_->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, key_url.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                                   postdata.c_str())) != CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK) {
    logger_->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  *response = read_data_ss.str();
  return false;
}

bool Vault_curl::read_key(const Vault_key &key, Secure_string *response) {
  Secure_string key_url;
  if (get_key_url(key, &key_url)) return true;
  CURLcode curl_res = CURLE_OK;

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    logger_->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, key_url.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK) {
    logger_->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  *response = read_data_ss.str();
  return false;
}

bool Vault_curl::delete_key(const Vault_key &key, Secure_string *response) {
  Secure_string key_url;
  if (get_key_url(key, &key_url)) return true;
  CURLcode curl_res = CURLE_OK;

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    logger_->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, key_url.c_str())) !=
          CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE")) !=
          CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK) {
    logger_->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  *response = read_data_ss.str();
  return false;
}

}  // namespace keyring
