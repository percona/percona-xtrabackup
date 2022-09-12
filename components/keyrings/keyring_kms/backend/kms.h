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

#ifndef __KMS_AWS_H__
#define __KMS_AWS_H__

// Code based on xbcloud

#include <curl/curl.h>
#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace aws {

std::string uri_escape_string(const std::string &s);
std::string uri_escape_path(const std::string &path);
bool http_init();
void http_cleanup();

using curl_easy_unique_ptr =
    std::unique_ptr<::CURL, decltype(&curl_easy_cleanup)>;
inline curl_easy_unique_ptr make_curl_easy() {
  return curl_easy_unique_ptr(curl_easy_init(), curl_easy_cleanup);
}

std::vector<unsigned char> sha256_ex(const unsigned char *ptr, std::size_t len);

template <typename T>
std::vector<unsigned char> sha256(const T &s) {
  return sha256_ex(reinterpret_cast<const unsigned char *>(&s[0]), s.size());
}

class Http_buffer {
 private:
  char *buf{nullptr};
  size_t buflen{0};
  size_t length{0};

  mutable std::vector<unsigned char> sha256_;

 public:
  using iterator = char *;
  using const_iterator = const char *;

  Http_buffer() {}
  Http_buffer(const Http_buffer &) = delete;
  Http_buffer(Http_buffer &&other) { *this = std::move(other); }
  Http_buffer &operator=(Http_buffer &&other) {
    buf = other.buf;
    buflen = other.buflen;
    length = other.length;
    sha256_ = std::move(other.sha256_);
    other.buf = nullptr;
    other.length = other.buflen = 0;
    return *this;
  }
  ~Http_buffer() {
    if (buf != nullptr) free(buf);
  }

  void append(const char *b, size_t n) {
    reserve(size() + n);
    memcpy(buf + length, b, n);
    length += n;
    if (!sha256_.empty()) sha256_.clear();
  }
  void append(const std::string &s) { append(s.c_str(), s.size()); }
  void append(const char *s) { append(s, strlen(s)); }
  void append(const std::vector<char> &v) { append(&v[0], v.size()); }
  void append(const Http_buffer &b) { append(b.begin(), b.size()); }
  size_t size() const noexcept { return length; }
  char &operator[](size_t i) { return buf[i]; }
  const char &operator[](size_t i) const { return buf[i]; }
  iterator begin() noexcept { return buf; }
  const_iterator begin() const noexcept { return buf; }
  iterator end() noexcept { return buf + length; }
  const_iterator end() const noexcept { return buf + length; }
  size_t capacity() const noexcept { return buflen; }
  void reserve(size_t n) {
    if (buflen < n) {
      buf = static_cast<char *>(realloc(buf, n));
      buflen = n;
    }
  }
  void clear() noexcept {
    length = 0;
    sha256_.clear();
  }
  void assign_buffer(char *buffer, size_t buffer_len, size_t len) {
    buf = buffer;
    buflen = buffer_len;
    length = len;
    sha256_.clear();
  }
  std::vector<unsigned char> sha256() const {
    if (sha256_.empty()) {
      sha256_ = aws::sha256(*this);
    }
    return sha256_;
  }
};

class Http_request {
 public:
  enum method_t { GET, PUT, POST, DELETE, HEAD };
  enum protocol_t { HTTP, HTTPS };

  using header_t = std::pair<std::string, std::string>;
  using headers_t = std::map<std::string, std::string>;

 private:
  method_t method_;
  protocol_t protocol_;
  std::string host_;
  std::string path_;
  headers_t headers_;
  std::vector<std::string> params_;
  Http_buffer payload_;

 public:
  Http_request(method_t method, protocol_t protocol, const std::string &host,
               const std::string &path)
      : method_(method),
        protocol_(protocol),
        host_(host),
        path_(uri_escape_path(path)) {}
  void add_header(const std::string &name, const std::string &value) {
    headers_[name] = value;
  }
  void remove_header(const std::string &name) { headers_.erase(name); }
  void add_param(const std::string &name, const std::string &value);
  void add_param(const std::string &name) { params_.push_back(name); }
  template <typename T>
  void append_payload(const T &payload) {
    payload_.append(payload);
  }
  void append_payload(const char *begin, size_t size) {
    payload_.append(begin, size);
  }
  std::string url() const {
    std::string qs = query_string();
    return (protocol_ == HTTP ? "http://" : "https://") + host_ + path_ +
           (!qs.empty() ? "?" + qs : "");
  }
  std::string const &path() const { return path_; }
  const headers_t &headers() const { return headers_; }
  bool has_header(const std::string &header_name) const {
    return headers_.count(header_name) > 0;
  }
  std::string header_value(const std::string &header_name) const {
    return headers_.at(header_name);
  }
  const std::vector<std::string> &params() const { return params_; }
  method_t method() const { return method_; }
  protocol_t protocol() const { return protocol_; }
  const Http_buffer &payload() const { return payload_; }
  std::string query_string() const;
};

class S3_response {
 private:
  bool error_{false};
  std::string error_message_;
  std::string error_code_;

 public:
  S3_response() {}
  // bool parse_http_response(Http_response &http_response);
  bool error() const { return error_; }
  const std::string &error_message() { return error_message_; }
  const std::string &error_code() { return error_code_; }
};

typedef enum { LOOKUP_AUTO, LOOKUP_DNS, LOOKUP_PATH } s3_bucket_lookup_t;
typedef enum { S3_V_AUTO, S3_V2, S3_V4 } s3_api_version_t;
class S3_signer {
 public:
  virtual void sign_request(const std::string &hostname,
                            const std::string &bucket, Http_request &req,
                            time_t t) = 0;
  virtual ~S3_signer() {}
};

class S3_signerV4 : public S3_signer {
 private:
  s3_bucket_lookup_t lookup;
  std::string region;
  std::string access_key;
  std::string secret_key;
  std::string session_token;
  std::string storage_class;

  static std::string aws_date_format(time_t t);

  static std::string build_hashed_canonical_request(
      Http_request &request, std::string &signed_headers);

  std::string build_string_to_sign(Http_request &request,
                                   std::string &signed_headers);

 public:
  S3_signerV4(s3_bucket_lookup_t lookup, const std::string &region,
              const std::string &access_key, const std::string &secret_key,
              const std::string &session_token = std::string(),
              const std::string &storage_class = std::string())
      : lookup(lookup),
        region(region),
        access_key(access_key),
        secret_key(secret_key),
        session_token(session_token),
        storage_class(storage_class) {}

  void sign_request(const std::string &hostname, const std::string &bucket,
                    Http_request &req, time_t t) override;
};

class Http_response {
 private:
  long code{0};
  Http_buffer body_;
  std::map<std::string, std::string> headers_;

 public:
  Http_response() {}
  const Http_buffer &body() const { return body_; }
  Http_buffer move_body() { return std::move(body_); }
  const std::map<std::string, std::string> &headers() const { return headers_; }
  bool ok() const { return code >= 200 && code < 300; }
  long http_code() const { return code; }
  void set_http_code(long http_code) { code = http_code; }
  static size_t header_appender(char *ptr, size_t size, size_t nmemb,
                                void *data);
  static size_t body_appender(char *ptr, size_t size, size_t nmemb,
                              void *data) {
    Http_response *response = reinterpret_cast<Http_response *>(data);
    response->body_.append(reinterpret_cast<char *>(ptr), size * nmemb);
    return size * nmemb;
  }
  void reset_body() { body_.clear(); }
};

class Http_connection {
 public:
  struct upload_state_t {
    const char *data;
    size_t len;
  };
  using callback_t = std::function<void(CURLcode, Http_connection *)>;

 private:
  curl_easy_unique_ptr curl_;
  curl_slist *headers_;
  char error_[CURL_ERROR_SIZE];
  upload_state_t upload_state_;
  Http_response &response_;

  callback_t callback_;

 public:
  Http_connection(curl_easy_unique_ptr curl, const Http_request &,
                  Http_response &response, callback_t callback = {})
      : curl_(std::move(curl)), response_(response), callback_(callback) {
    curl_easy_setopt(curl_.get(), CURLOPT_PRIVATE, this);
    curl_easy_setopt(curl_.get(), CURLOPT_ERRORBUFFER, error_);
  }

  ~Http_connection() { curl_slist_free_all(headers_); }

  CURL *curl_easy() const { return curl_.get(); }

  const char *error() const { return error_; }

  void set_headers(curl_slist *headers) { headers_ = headers; }

  void finalize(CURLcode rc) {
    long http_code;
    curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &http_code);
    response_.set_http_code(http_code);
    if (callback_) {
      callback_(rc, this);
    }
  }

  upload_state_t *upload_state() { return &upload_state_; }

  Http_response &response() const { return response_; }
};

class Http_client {
 public:
  // using async_callback_t = std::function<void(CURLcode, Http_connection *)>;
 private:
  bool insecure{false};
  bool verbose{false};
  std::string cacert;
  mutable curl_easy_unique_ptr curl{nullptr, curl_easy_cleanup};

  void setup_request(CURL *curl, const Http_request &request,
                     Http_response &response, curl_slist *&headers,
                     Http_connection::upload_state_t *upload_state) const;

  static int upload_callback(char *ptr, size_t size, size_t nmemb, void *data);

 public:
  Http_client() {}
  Http_client(const Http_client &) = delete;
  virtual ~Http_client() {}

  virtual bool make_request(const Http_request &request,
                            Http_response &response) const;

  void set_verbose(bool val) { verbose = val; }
  void set_insecure(bool val) { insecure = val; }
  void set_cacaert(const std::string &val) { cacert = val; }
  void reset() const { curl = nullptr; }
  virtual bool get_verbose() const { return verbose; }
};

struct Kms_error {
  bool okay;
  std::string error_message;

  explicit operator bool() { return !okay; }
};

class Kms_client {
 public:
  Kms_client(std::string const &region, std::string const &key_id,
             std::string const &secret_access_key);

  Kms_client(Kms_client const &) = delete;
  Kms_client &operator=(Kms_client const &) = delete;

  // expects / returns base64
  Kms_error encrypt(std::string const &plaintext, std::string const &key,
                    std::string &encrypted);
  // expects / returns base64
  Kms_error decrypt(std::string const &encrypted,
                    std::string &plaintext);  // key is in encrypted
  // expects / returns base64
  Kms_error recrypt(std::string const &old, std::string const &key,
                    std::string &encrypted);

 private:
  Kms_error kms_call(std::string const &amzTarget, std::string const &payload,
                     std::string const &resultKey, std::string &result);

  Http_response do_request(std::string const &amzTarget,
                           std::string const &payload);

  std::string region;
  std::string key_id;
  std::string secret_access_key;

  Http_client client;
};

}  // namespace aws

#endif  // __KMS_AWS_H__
