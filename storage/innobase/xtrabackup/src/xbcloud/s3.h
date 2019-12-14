/******************************************************
Copyright (c) 2019 Percona LLC and/or its affiliates.

AWS S3 client implementation.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/

#ifndef __XBCLOUD_S3_H__
#define __XBCLOUD_S3_H__

#include "object_store.h"
#include "xbcloud/http.h"
#include "xbcloud/util.h"

#include <time.h>

namespace xbcloud {

class S3_response {
 private:
  bool error_{false};
  std::string error_message_;
  std::string error_code_;

 public:
  S3_response() {}
  bool parse_http_response(Http_response &http_response);
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
  virtual ~S3_signer(){};
};

class S3_signerV4 : public S3_signer {
 private:
  s3_bucket_lookup_t lookup;
  std::string region;
  std::string access_key;
  std::string secret_key;

  static std::string aws_date_format(time_t t);

  static std::string build_hashed_canonical_request(
      Http_request &request, std::string &signed_headers);

  std::string build_string_to_sign(Http_request &request,
                                   std::string &signed_headers);

 public:
  S3_signerV4(s3_bucket_lookup_t lookup, const std::string &region,
              const std::string &access_key, const std::string &secret_key)
      : lookup(lookup),
        region(region),
        access_key(access_key),
        secret_key(secret_key) {}

  void sign_request(const std::string &hostname, const std::string &bucket,
                    Http_request &req, time_t t);
};

class S3_signerV2 : public S3_signer {
 private:
  s3_bucket_lookup_t lookup;
  std::string region;
  std::string access_key;
  std::string secret_key;

  static std::string aws_date_format(time_t t);

  std::string build_string_to_sign(const std::string &hostname,
                                   Http_request &request);

 public:
  S3_signerV2(s3_bucket_lookup_t lookup, const std::string &region,
              const std::string &access_key, const std::string &secret_key)
      : lookup(lookup),
        region(region),
        access_key(access_key),
        secret_key(secret_key) {}

  void sign_request(const std::string &hostname, const std::string &bucket,
                    Http_request &req, time_t t);
};

class S3_client {
 public:
  using async_upload_callback_t = std::function<void(bool)>;
  using async_download_callback_t =
      std::function<void(bool, const Http_buffer &)>;
  using async_delete_callback_t = std::function<void(bool)>;

 private:
  const Http_client *http_client;

  s3_api_version_t api_version{S3_V_AUTO};

  std::string region;
  std::string endpoint;
  std::string host;
  std::string access_key;
  std::string secret_key;

  s3_bucket_lookup_t bucket_lookup{LOOKUP_AUTO};

  std::unique_ptr<S3_signer> signer;

  std::string base_url;

  Http_request::protocol_t protocol;

  std::string hostname(const std::string &bucket) const {
    if (bucket_lookup == LOOKUP_DNS) {
      return bucket + "." + host;
    } else {
      return host;
    }
  }

  std::string bucketname(const std::string &bucket) const {
    if (bucket_lookup == LOOKUP_PATH) {
      return "/" + bucket;
    } else {
      return "";
    }
  }

  static void upload_callback(S3_client *client, std::string bucket,
                              std::string name, Http_request *req,
                              Http_response *resp,
                              const Http_client *http_client, Event_handler *h,
                              S3_client::async_upload_callback_t callback,
                              CURLcode rc, const Http_connection *conn,
                              int count);

 public:
  S3_client(const Http_client *client, const std::string &region,
            const std::string &access_key, const std::string &secret_key)
      : http_client(client),
        region(region),
        access_key(access_key),
        secret_key(secret_key) {
    host = "s3." + region + ".amazonaws.com";
    endpoint = "https://" + host;
    protocol = Http_request::HTTPS;
    rtrim_slashes(host);
  }

  void set_endpoint(const std::string &ep) {
    endpoint = ep;
    if (endpoint.find("https://") == 0) {
      protocol = Http_request::HTTPS;
      host = endpoint.substr(8);
    } else if (endpoint.find("http://") == 0) {
      protocol = Http_request::HTTP;
      host = endpoint.substr(7);
    } else {
      protocol = Http_request::HTTPS;
      host = endpoint;
    }
    rtrim_slashes(host);
  }

  void set_bucket_lookup(s3_bucket_lookup_t val) { bucket_lookup = val; }

  void set_api_version(s3_api_version_t version) { api_version = version; }

  bool probe_api_version_and_lookup(const std::string &bucket);

  bool delete_object(const std::string &bucket, const std::string &name);

  bool create_bucket(const std::string &name);

  bool bucket_exists(const std::string &name, bool &exists);

  bool upload_object(const std::string &bucket, const std::string &name,
                     const Http_buffer &contents);

  Http_buffer download_object(const std::string &bucket,
                              const std::string &name, bool &success);

  bool async_upload_object(
      const std::string &bucket, const std::string &name,
      const Http_buffer &contents, Event_handler *h,
      async_upload_callback_t callback = {},
      const Http_request::headers_t &extra_http_headers = {});

  bool async_download_object(
      const std::string &bucket, const std::string &name, Event_handler *h,
      async_download_callback_t callback = {},
      const Http_request::headers_t &extra_http_headers = {});

  bool async_delete_object(const std::string &bucket, const std::string &name,
                           Event_handler *h,
                           const async_delete_callback_t callback);

  bool list_objects_with_prefix(const std::string &bucket,
                                const std::string &prefix,
                                std::vector<std::string> &objects);
};

class S3_object_store : public Object_store {
 private:
  S3_client s3_client;
  Http_request::headers_t extra_http_headers;

 public:
  S3_object_store(const Http_client *client, std::string &region,
                  const std::string &access_key, const std::string &secret_key,
                  const std::string &endpoint = std::string(),
                  s3_bucket_lookup_t bucket_lookup = LOOKUP_DNS,
                  s3_api_version_t api_version = S3_V_AUTO)
      : s3_client{client, region, access_key, secret_key} {
    if (!endpoint.empty()) s3_client.set_endpoint(endpoint);
    s3_client.set_bucket_lookup(bucket_lookup);
    s3_client.set_api_version(api_version);
  }
  void set_extra_http_headers(const Http_request::headers_t &headers) {
    extra_http_headers = headers;
  }
  bool probe_api_version_and_lookup(const std::string &bucket) {
    return s3_client.probe_api_version_and_lookup(bucket);
  };
  virtual bool create_container(const std::string &name) override {
    return s3_client.create_bucket(name);
  }
  virtual bool container_exists(const std::string &name,
                                bool &exists) override {
    return s3_client.bucket_exists(name, exists);
  }
  virtual bool list_objects_in_directory(
      const std::string &container, const std::string &directory,
      std::vector<std::string> &objects) override {
    return s3_client.list_objects_with_prefix(container, directory + "/",
                                              objects);
  }
  virtual bool upload_object(const std::string &container,
                             const std::string &object,
                             const Http_buffer &contents) override {
    return s3_client.upload_object(container, object, contents);
  }
  virtual bool async_upload_object(const std::string &container,
                                   const std::string &object,
                                   const Http_buffer &contents,
                                   Event_handler *h,
                                   std::function<void(bool)> f = {}) override {
    return s3_client.async_upload_object(container, object, contents, h,
                                         [f](bool success) {
                                           if (f) f(success);
                                         },
                                         extra_http_headers);
  }
  virtual bool async_download_object(
      const std::string &container, const std::string &object, Event_handler *h,
      const std::function<void(bool, const Http_buffer &contents)> f = {})
      override {
    return s3_client.async_download_object(
        container, object, h,
        [f](bool success, const Http_buffer &contents) {
          if (f) f(success, contents);
        },
        extra_http_headers);
  }
  virtual bool async_delete_object(const std::string &container,
                                   const std::string &object, Event_handler *h,
                                   std::function<void(bool)> f = {}) override {
    return s3_client.async_delete_object(container, object, h,
                                         [f](bool success) {
                                           if (f) f(success);
                                         });
  }
  virtual bool delete_object(const std::string &container,
                             const std::string &name) override {
    return s3_client.delete_object(container, name);
  }
  virtual Http_buffer download_object(const std::string &container,
                                      const std::string &name,
                                      bool &success) override {
    return s3_client.download_object(container, name, success);
  }
};

}  // namespace xbcloud

#endif
