/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include <boost/scope_exit.hpp>
#include <boost/core/noncopyable.hpp>
#include <algorithm>
#include "my_rdtsc.h"
#include "sql_error.h"
#include "mysqld.h"
#include "mysql/service_thd_wait.h"
#include "vault_curl.h"
#include "secure_string.h"
#include "vault_base64.h"

namespace keyring
{

static const size_t max_response_size = 32000000;
static MY_TIMER_INFO curl_timer_info;
static ulonglong last_ping_time;
static bool was_thd_wait_started = false;
#ifndef NDEBUG
static const ulonglong slow_connection_threshold = 100; // [ms]
#endif

class Thd_wait_end_guard
{
  public:
    ~Thd_wait_end_guard()
    {
      DBUG_EXECUTE_IF("vault_network_lag", {
                    was_thd_wait_started = false;   
        });
      DBUG_ASSERT(!was_thd_wait_started);
      if (was_thd_wait_started)
      {
        // This should never be called as thd_wait_end should be called at the end of CURL I/O operation.
        // However, in production the call to thd_wait_end cannot be missed. Thus we limit our trust
        // to CURL lib and make sure thd_wait_end was called.
        thd_wait_end(current_thd);
        was_thd_wait_started = false;
      }
    }
};

class Curl_session_guard : private boost::noncopyable
{
public:
  Curl_session_guard(CURL *curl)
    : curl(curl)
  {}
  ~Curl_session_guard()
  {
    if (curl != NULL)
      curl_easy_cleanup(curl);
  }
private:
  CURL *curl;
};

static size_t write_response_memory(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  if (size != 0 && realsize / size != nmemb)
    return 0; // overflow
  Secure_ostringstream *read_data = static_cast<Secure_ostringstream*>(userp);
  size_t ss_pos = read_data->tellp();
  read_data->seekp(0, std::ios::end);
  size_t number_of_read_bytes = read_data->tellp();
  read_data->seekp(ss_pos);

  if (number_of_read_bytes + realsize > max_response_size)
    return 0; // response size limit exceeded

  read_data->write(static_cast<char*>(contents), realsize);
  if (!read_data->good())
    return 0;
  return realsize;
}

int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
  ulonglong curr_ping_time = my_timer_milliseconds();

  DBUG_EXECUTE_IF("vault_network_lag", {
                curr_ping_time = last_ping_time + slow_connection_threshold + 10; 
                dltotal = 1;
                dlnow = 0;
    });

  BOOST_SCOPE_EXIT(&curr_ping_time, &last_ping_time)
  {
    last_ping_time = curr_ping_time;
  } BOOST_SCOPE_EXIT_END

  //***To keep compiler happy, remove when PS-244 gets resolved
  (void)dltotal;
  (void)dlnow;
  //****

  // The calls to threadpool are disabled till bug PS-244 gets resolved.
  /* <--Uncomment when PS-244 gets resolved
  if (!was_thd_wait_started)
  { 
    if ((dlnow < dltotal || ulnow < ultotal) && last_ping_time - curr_ping_time > slow_connection_threshold)
    { 
      // there is a good chance that connection is slow, thus we can let know the threadpool that there is time
      // to start new thread(s)
      thd_wait_begin(current_thd, THD_WAIT_NET);
      was_thd_wait_started = true;
    }
  }
  else if ((dlnow == dltotal && ulnow == ultotal) || last_ping_time - curr_ping_time <= slow_connection_threshold)
  {
    // connection has speed up or we have finished transfering
    thd_wait_end(current_thd);
    was_thd_wait_started = false;
  }*/ // <--Uncomment when PS-244 gets resolved
  return 0;
}

std::string Vault_curl::get_error_from_curl(CURLcode curl_code)
{
  size_t len = strlen(curl_errbuf);
  std::ostringstream ss;
  if (curl_code != CURLE_OK)
  {
    ss << "CURL returned this error code: " << curl_code;
    ss << " with error message : ";
    if (len)
      ss << curl_errbuf;
    else
      ss << curl_easy_strerror(curl_code);
  }
  return ss.str();
}

bool Vault_curl::init(const Vault_credentials &vault_credentials)
{
  this->token_header = "X-Vault-Token:" + get_credential(vault_credentials, "token");
  this->vault_url = get_credential(vault_credentials, "vault_url") + "/v1/" + get_credential(vault_credentials, "secret_mount_point");
  this->vault_ca = get_credential(vault_credentials, "vault_ca");
  if (this->vault_ca.empty())
  {
    logger->log(MY_WARNING_LEVEL, "There is no vault_ca specified in keyring_vault's configuration file. "
                                  "Please make sure that Vault's CA certificate is trusted by the machine from "
                                  "which you intend to connect to Vault."); 
  }
  my_timer_init(&curl_timer_info);
  return false;
}

bool Vault_curl::setup_curl_session(CURL *curl)
{
  CURLcode curl_res = CURLE_OK;
  read_data_ss.str("");
  read_data_ss.clear();
  curl_errbuf[0] = '\0';
  if (list != NULL)
  {
    curl_slist_free_all(list);
    list = NULL;
  }

  last_ping_time = my_timer_milliseconds();

  if ((list = curl_slist_append(list, token_header.c_str())) == NULL ||
      (list = curl_slist_append(list, "Content-Type: application/json")) == NULL ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_memory)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&read_data_ss))) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L)) != CURLE_OK ||
      (!vault_ca.empty() &&
       (curl_res = curl_easy_setopt(curl, CURLOPT_CAINFO, vault_ca.c_str())) != CURLE_OK
      ) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout)) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout)) != CURLE_OK
     )
  {
    logger->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  return false;
}

bool Vault_curl::list_keys(Secure_string *response)
{
  CURLcode curl_res = CURLE_OK;
  long http_code = 0;

  Thd_wait_end_guard thd_wait_end_guard;
  (void)thd_wait_end_guard; // silence unused variable error

  CURL *curl = curl_easy_init();
  if (curl == NULL)
  {
    logger->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, (vault_url + "?list=true").c_str())) != CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK ||
      (curl_res = curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code)) != CURLE_OK)
  {
    logger->log(MY_ERROR_LEVEL,
                get_error_from_curl(curl_res).c_str());
    return true;
  }
  if (http_code == 404)
  {
    *response = ""; // no keys found
    return false; 
  }
  *response = read_data_ss.str();
  return http_code / 100 != 2; // 2** are success return codes
}

bool Vault_curl::encode_key_signature(const Vault_key &key, Secure_string *encoded_key_signature)
{
  if (Vault_base64::encode(key.get_key_signature()->c_str(), key.get_key_signature()->length(), 
      encoded_key_signature, Vault_base64::SINGLE_LINE))
  {
    logger->log(MY_ERROR_LEVEL, "Could not encode key's signature in base64");
    return true;
  }
  return false;
}

bool Vault_curl::get_key_url(const Vault_key &key, Secure_string *key_url)
{
  Secure_string encoded_key_signature;
  if (encode_key_signature(key, &encoded_key_signature))
    return true;
  *key_url = vault_url + '/' + encoded_key_signature.c_str();
  return false;
}

bool Vault_curl::write_key(const Vault_key &key, Secure_string *response)
{
  Secure_string encoded_key_data;
  if (Vault_base64::encode(reinterpret_cast<const char*>(key.get_key_data()), key.get_key_data_size(),
      &encoded_key_data, Vault_base64::SINGLE_LINE))
  {
    logger->log(MY_ERROR_LEVEL, "Could not encode a key in base64");
    return true;
  }
  CURLcode curl_res = CURLE_OK;
  Secure_string postdata="{\"type\":\"";
  postdata += key.get_key_type()->c_str();
  postdata += "\",\"";
  postdata += "value\":\"" + encoded_key_data + "\"}";

  Secure_string key_url;
  if (get_key_url(key, &key_url))
    return true;

  Thd_wait_end_guard thd_wait_end_guard;
  (void)thd_wait_end_guard; //silence unused variable error

  CURL *curl = curl_easy_init();
  if (curl == NULL)
  {
    logger->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL,
                                   key_url.c_str())) != CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata.c_str())) != CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK)
  {
    logger->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  *response = read_data_ss.str();
  return false;
}

bool Vault_curl::read_key(const Vault_key &key, Secure_string *response)
{
  Secure_string key_url;
  if (get_key_url(key, &key_url))
    return true;
  CURLcode curl_res = CURLE_OK;

  Thd_wait_end_guard thd_wait_end_guard;
  (void)thd_wait_end_guard; // silence unused variable error

  CURL *curl = curl_easy_init();
  if (curl == NULL)
  {
    logger->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL,
                                   key_url.c_str())) != CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK)
  {
    logger->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  *response = read_data_ss.str();
  return false;
}

bool Vault_curl::delete_key(const Vault_key &key, Secure_string *response)
{
  Secure_string key_url;
  if (get_key_url(key, &key_url))
    return true;
  CURLcode curl_res = CURLE_OK;

  Thd_wait_end_guard thd_wait_end_guard;
  (void)thd_wait_end_guard; // silence unused variable error
  CURL *curl = curl_easy_init();
  if (curl == NULL)
  {
    logger->log(MY_ERROR_LEVEL, "Cannot initialize curl session");
    return true;
  }
  Curl_session_guard curl_session_guard(curl);

  if (setup_curl_session(curl) ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_URL, key_url.c_str())) !=
      CURLE_OK ||
      (curl_res = curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE")) != CURLE_OK ||
      (curl_res = curl_easy_perform(curl)) != CURLE_OK)
  {
    logger->log(MY_ERROR_LEVEL, get_error_from_curl(curl_res).c_str());
    return true;
  }
  *response = read_data_ss.str();
  return false;
}

} //namespace keyring
