/* Copyright (c) 2022, Percona and/or its affiliates.

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

#include "kms.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include <openssl/hmac.h>
#include <openssl/sha.h>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include "rapidjson/document.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

// Temporal fix for boost workaround.hpp 1.73.0 warnings
// See https://github.com/boostorg/config/pull/383/files
#ifndef __clang_major__
#define __clang_major___WORKAROUND_GUARD 1
#else
#define __clang_major___WORKAROUND_GUARD 0
#endif

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>

const constexpr auto AWS_DATE_HEADER = "X-Amz-Date";
const constexpr auto AWS_CONTENT_SHA256_HEADER = "X-Amz-Content-SHA256";
const constexpr auto AWS_SESSION_TOKEN_HEADER = "X-Amz-Security-Token";
const constexpr auto AWS_SIGNATURE_ALGORITHM = "AWS4-HMAC-SHA256";
const constexpr auto AWS_STORAGE_CLASS_HEADER = "X-Amz-Storage-Class";

namespace aws {

std::vector<unsigned char> sha256_ex(const unsigned char *ptr,
                                     std::size_t len) {
  std::vector<unsigned char> md(SHA256_DIGEST_LENGTH);

  SHA256(ptr, len, md.data());

  return md;
}

std::string hmac_sha256(std::string const &decodedKey, std::string const &msg) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
  unsigned int hashLen;

  HMAC(EVP_sha256(), decodedKey.data(), static_cast<int>(decodedKey.size()),
       reinterpret_cast<unsigned char const *>(msg.data()),
       static_cast<int>(msg.size()), hash.data(), &hashLen);

  return std::string{reinterpret_cast<char const *>(hash.data()), hashLen};
}

static std::string canonicalize_http_header_value(const std::string &s) {
  std::string r = s;

  /* replace multiple spaces with single space */
  auto new_end = std::unique(r.begin(), r.end(), [](char lhs, char rhs) {
    return rhs == ' ' && lhs == ' ';
  });
  r.erase(new_end, r.end());

  /* trim trailing and leading spaces */
  boost::algorithm::trim(r);

  return r;
}

class Global_curl {
 private:
  CURL *curl{nullptr};
  std::mutex mutex;

 public:
  Global_curl() {}

  bool create() {
    curl = curl_easy_init();
    return (curl != nullptr);
  }

  void cleanup() {
    curl_easy_cleanup(curl);
    curl = nullptr;
  }

  void lock() { mutex.lock(); }

  CURL *get() const { return curl; }

  void unlock() { mutex.unlock(); }
};

static Global_curl global_curl;

bool http_init() {
  curl_global_init(CURL_GLOBAL_ALL);
  return global_curl.create();
}

void http_cleanup() {
  curl_global_cleanup();
  global_curl.cleanup();
}

std::string uri_escape_string(const std::string &s) {
  std::lock_guard<Global_curl> lock(global_curl);
  char *escaped_string =
      curl_easy_escape(global_curl.get(), s.c_str(), s.length());
  std::string result(escaped_string);
  curl_free(escaped_string);
  return result;
}

std::string uri_escape_path(const std::string &path) {
  std::string result = uri_escape_string(path);
  boost::replace_all(result, "%2F", "/");
  return result;
}

void Http_request::add_param(const std::string &name,
                             const std::string &value) {
  std::stringstream param;
  param << uri_escape_string(name) << "=" + uri_escape_string(value);
  params_.push_back(param.str());
}

std::string Http_request::query_string() const {
  std::stringstream query_string;
  int idx = 0;
  /* we need to sort query string params for AWS canonical request */
  std::vector<std::string> sorted_params = params_;
  std::sort(sorted_params.begin(), sorted_params.end());
  for (auto &param : sorted_params) {
    if (idx++ > 0) {
      query_string << "&";
    }
    query_string << param;
  }
  return query_string.str();
}

std::string S3_signerV4::aws_date_format(time_t t) {
  struct tm tmp;
  char buf[40];
  strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", gmtime_r(&t, &tmp));
  return buf;
}

std::string S3_signerV4::build_hashed_canonical_request(
    Http_request &request, std::string &signed_headers) {
  std::stringstream canonical_request;
  /* canonical request method */
  switch (request.method()) {
    case Http_request::GET:
      canonical_request << "GET\n";
      break;
    case Http_request::POST:
      canonical_request << "POST\n";
      break;
    case Http_request::PUT:
      canonical_request << "PUT\n";
      break;
    case Http_request::DELETE:
      canonical_request << "DELETE\n";
      break;
    case Http_request::HEAD:
      canonical_request << "HEAD\n";
      break;
  }

  std::string content_sha256;
  boost::algorithm::hex(request.payload().sha256(),
                        std::back_inserter(content_sha256));
  boost::algorithm::to_lower(content_sha256);
  request.add_header(AWS_CONTENT_SHA256_HEADER, content_sha256);

  /* canonical URI */
  canonical_request << request.path() << "\n";

  /* canonical query string */
  canonical_request << request.query_string() << "\n";

  std::map<std::string, std::string> keys;

  for (const auto &header : request.headers()) {
    std::string canonical_key = header.first;
    boost::algorithm::to_lower(canonical_key);
    keys[canonical_key] = header.first;
  }

  /* canonical headers */
  for (const auto &key : keys) {
    std::string canonical_name = key.first;
    boost::algorithm::to_lower(canonical_name);
    std::string canonical_value = request.headers().at(key.second);
    canonicalize_http_header_value(canonical_value);
    canonical_request << canonical_name << ":" << canonical_value << "\n";
    if (!signed_headers.empty()) signed_headers.append(";");
    signed_headers.append(canonical_name);
  }
  canonical_request << "\n" << signed_headers << "\n";

  canonical_request << content_sha256;

  std::string ret;
  boost::algorithm::hex(sha256(canonical_request.str()),
                        std::back_inserter(ret));
  boost::algorithm::to_lower(ret);
  return ret;
}

std::string S3_signerV4::build_string_to_sign(Http_request &request,
                                              std::string &signed_headers) {
  std::string s = AWS_SIGNATURE_ALGORITHM + std::string("\n");
  std::string date = request.header_value(AWS_DATE_HEADER);
  s.append(date);
  s.append("\n");
  s.append(date.substr(0, 8));
  s.append("/");
  s.append(region);
  s.append("/kms/aws4_request\n");
  s.append(build_hashed_canonical_request(request, signed_headers));

  return s;
}

void S3_signerV4::sign_request(const std::string &hostname, const std::string &,
                               Http_request &req, time_t t) {
  auto date_time = aws_date_format(t);
  auto date = date_time.substr(0, 8);
  req.add_header("Host", hostname);
  req.add_header(AWS_DATE_HEADER, date_time);
  /* in case we updating already signed request */
  req.remove_header("Authorization");

  if (!session_token.empty()) {
    req.add_header(AWS_SESSION_TOKEN_HEADER, session_token);
  }

  if (!storage_class.empty()) {
    req.add_header(AWS_STORAGE_CLASS_HEADER, storage_class);
  }
  std::string signed_headers;
  auto string_to_sign = build_string_to_sign(req, signed_headers);

  auto k_date = hmac_sha256("AWS4" + secret_key, date);
  auto k_region = hmac_sha256(k_date, region);
  auto k_service = hmac_sha256(k_region, "kms");
  auto k_signing = hmac_sha256(k_service, "aws4_request");

  std::string signature;
  boost::algorithm::hex(hmac_sha256(k_signing, string_to_sign),
                        std::back_inserter(signature));
  boost::algorithm::to_lower(signature);

  std::string sig_header;
  sig_header.append(AWS_SIGNATURE_ALGORITHM);
  sig_header.append(" Credential=");
  sig_header.append(access_key);
  sig_header.append("/");
  sig_header.append(date);
  sig_header.append("/");
  sig_header.append(region);
  sig_header.append("/kms/aws4_request");

  sig_header.append(", SignedHeaders=");
  sig_header.append(signed_headers);

  sig_header.append(", Signature=");
  sig_header.append(signature);

  req.add_header("Authorization", sig_header);
}

void Http_client::setup_request(
    CURL *curl, const Http_request &request, Http_response &response,
    curl_slist *&headers, Http_connection::upload_state_t *upload_state) const {
  for (auto &header : request.headers()) {
    headers = curl_slist_append(headers,
                                (header.first + ": " + header.second).c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, request.url().c_str());

  switch (request.method()) {
    case Http_request::GET:
      break;
    case Http_request::PUT:
      curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
      upload_state->data = &request.payload()[0];
      upload_state->len = request.payload().size();
      curl_easy_setopt(curl, CURLOPT_READFUNCTION,
                       Http_client::upload_callback);
      curl_easy_setopt(curl, CURLOPT_READDATA, upload_state);
      curl_easy_setopt(curl, CURLOPT_INFILESIZE, upload_state->len);
      break;
    case Http_request::POST:
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      break;
    case Http_request::DELETE:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
      break;
    case Http_request::HEAD:
      curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
      break;
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  if (request.method() == Http_request::POST) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, &request.payload()[0]);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                     (long)request.payload().size());
  }

  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                   Http_response::header_appender);

  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Http_response::body_appender);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

#if LIBCURL_VERSION_NUM <= 0x071506
  curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");
#else
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
#endif

  if (verbose) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }

  if (insecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  if (!cacert.empty()) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, cacert.c_str());
  }
}

bool Http_client::make_request(const Http_request &request,
                               Http_response &response) const {
  curl_slist *headers = nullptr;
  Http_connection::upload_state_t upload_state;

  if (!curl) {
    curl_easy_unique_ptr tmp = make_curl_easy();
    curl = std::move(tmp);
  }
  if (!curl) {
    return false;
  }

  setup_request(curl.get(), request, response, headers, &upload_state);

  auto res = curl_easy_perform(curl.get());
  if (res != CURLE_OK) {
    curl_slist_free_all(headers);
    return false;
  }

  long http_code;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_reset(curl.get());

  response.set_http_code(http_code);
  return true;
}

size_t Http_response::header_appender(char *ptr, size_t size, size_t nmemb,
                                      void *data) {
  size_t buflen = size * nmemb;
  size_t colon_pos = buflen;
  for (size_t i = 0; i < buflen; i++) {
    if (ptr[i] == ':') {
      colon_pos = i;
      break;
    }
  }

  if (colon_pos == buflen) {
    return buflen;
  }

  std::string name(ptr, colon_pos);
  std::string val(ptr + colon_pos + 1, buflen - colon_pos - 1);

  boost::algorithm::trim(name);
  boost::algorithm::trim(val);

  boost::algorithm::to_lower(name);

  if (name.empty()) {
    return buflen;
  }

  Http_response *response = reinterpret_cast<Http_response *>(data);
  response->headers_[name] = val;

  if (name == "content-length") {
    long size = atol(val.c_str());
    if (size > 0 && response->body_.capacity() < (size_t)size) {
      response->body_.reserve(size);
    }
  }

  return buflen;
}

int Http_client::upload_callback(char *ptr, size_t size, size_t nmemb,
                                 void *data) {
  Http_connection::upload_state_t *upload =
      reinterpret_cast<Http_connection::upload_state_t *>(data);
  size_t len = std::min(size * nmemb, upload->len);

  memcpy(ptr, upload->data, len);
  upload->data += len;
  upload->len -= len;

  return len;
}

Kms_client::Kms_client(std::string const &region, std::string const &key_id,
                       std::string const &secret_access_key)
    : region(region), key_id(key_id), secret_access_key(secret_access_key) {}

Kms_error Kms_client::kms_call(std::string const &amzTarget,
                               std::string const &payload,
                               std::string const &resultKey,
                               std::string &result) {
  auto response = do_request(amzTarget, payload);

  std::string r(response.body().begin(), response.body().end());

  if (!response.ok() && r.empty()) {
    return {false, "Error during HTTP request"};
  }

  using namespace rapidjson;
  Document d;
  d.Parse(r.c_str());

  if (!response.ok()) {
    std::string errorMessage = "Error: ";
    errorMessage += d["__type"].GetString();
    if (d.HasMember("message")) {
      errorMessage += ": ";
      errorMessage += d["message"].GetString();
    }

    return {false, errorMessage};
  }

  result = d[resultKey.c_str()].GetString();

  return {true, ""};
}

static std::string buildEncryptPayload(std::string const &key,
                                       std::string const &text) {
  using namespace rapidjson;

  Document d;  // Null
  d.SetObject();

  Value keyV;
  keyV.SetString(key.c_str(), key.size());
  d.AddMember("KeyId", keyV, d.GetAllocator());

  Value plainV;
  plainV.SetString(text.c_str(), text.size());
  d.AddMember("Plaintext", plainV, d.GetAllocator());

  StringBuffer strbuf;
  strbuf.Clear();

  Writer<StringBuffer> writer(strbuf);
  d.Accept(writer);

  return strbuf.GetString();
}

Kms_error Kms_client::encrypt(std::string const &plaintext,
                              std::string const &key, std::string &encrypted) {
  const std::string payload = buildEncryptPayload(key, plaintext);
  return kms_call("TrentService.Encrypt", payload, "CiphertextBlob", encrypted);
}

static std::string buildDecryptPayload(std::string const &text) {
  using namespace rapidjson;

  Document d;  // Null
  d.SetObject();

  Value plainV;
  plainV.SetString(text.c_str(), text.size());
  d.AddMember("CiphertextBlob", plainV, d.GetAllocator());

  StringBuffer strbuf;
  strbuf.Clear();

  Writer<StringBuffer> writer(strbuf);
  d.Accept(writer);

  return strbuf.GetString();
}

Kms_error Kms_client::decrypt(std::string const &encrypted,
                              std::string &plaintext) {
  const std::string payload = buildDecryptPayload(encrypted);
  return kms_call("TrentService.Decrypt", payload, "Plaintext", plaintext);
}

static std::string buildRecryptPayload(std::string key, std::string text) {
  using namespace rapidjson;

  Document d;  // Null
  d.SetObject();

  Value keyV;
  keyV.SetString(key.c_str(), key.size());
  d.AddMember("DestinationKeyId", keyV, d.GetAllocator());

  Value plainV;
  plainV.SetString(text.c_str(), text.size());
  d.AddMember("CiphertextBlob", plainV, d.GetAllocator());

  StringBuffer strbuf;
  strbuf.Clear();

  Writer<StringBuffer> writer(strbuf);
  d.Accept(writer);

  return strbuf.GetString();
}

Kms_error Kms_client::recrypt(std::string const &old, std::string const &key,
                              std::string &encrypted) {
  const std::string payload = buildRecryptPayload(key, old);
  return kms_call("TrentService.ReEncrypt", payload, "CiphertextBlob",
                  encrypted);
}

Http_response Kms_client::do_request(std::string const &amzTarget,
                                     std::string const &payload) {
  const std::string host = "kms." + region + ".amazonaws.com";

  Http_request req(Http_request::POST, Http_request::HTTPS, host.c_str(), "/");
  req.add_header("Content-Length", std::to_string(payload.size()));
  req.add_header("Content-Type", "application/x-amz-json-1.1");
  req.add_header("X-Amz-Target", amzTarget);
  req.append_payload(payload.c_str(), payload.size());

  S3_signerV4 signer(LOOKUP_DNS, region, key_id, secret_access_key);

  time_t timev;
  time(&timev);

  signer.sign_request("localhost", "", req, timev);

  Http_response resp;
  client.make_request(req, resp);

  return resp;
}

}  // namespace aws
