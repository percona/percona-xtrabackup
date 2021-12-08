/******************************************************
Copyright (c) 2021 Percona LLC and/or its affiliates.

AZURE client implementation.

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

#ifndef __XBCLOUD_AZURE_H__
#define __XBCLOUD_AZURE_H__

#include <iostream>
#include "object_store.h"
#include "xbcloud/http.h"
#include "xbcloud/util.h"

#include <time.h>
namespace xbcloud {

class Azure_response {
 private:
  bool error_{false};
  std::string error_message_;
  std::string error_code_;

 public:
  Azure_response() {}
  bool parse_http_response(Http_response &http_response);
  bool error() const { return error_; }
  const std::string &error_message() { return error_message_; }
  const std::string &error_code() { return error_code_; }
};

class Azure_signer {
 private:
  std::string storage_account;
  std::string access_key;
  bool development_storage;
  std::string storage_class;

  static std::string azure_date_format(time_t t);

  std::string build_string_to_sign(Http_request &request,
                                   const std::string &container,
                                   const std::string &blob);

 public:
  Azure_signer(const std::string &storage_account,
               const std::string &access_key, bool development_storage,
               const std::string &storage_class = std::string())
      : storage_account(storage_account),
        access_key(access_key),
        development_storage(development_storage),
        storage_class(storage_class) {}
  void sign_request(const std::string &container, const std::string &blob,
                    Http_request &req, time_t t);
  virtual ~Azure_signer() {}
};

class Azure_client {
 public:
  using async_upload_callback_t =
      std::function<void(bool, const Http_buffer &)>;
  using async_download_callback_t =
      std::function<void(bool, const Http_buffer &)>;
  using async_delete_callback_t = std::function<void(bool)>;

  std::unique_ptr<Azure_signer> signer;

 private:
  const Http_client *http_client;

  std::string endpoint;
  std::string host;
  std::string storage_account;
  std::string access_key;

  std::string session_token;

  Http_request::protocol_t protocol;
  ulong max_retries;
  ulong max_backoff;

  static void upload_callback(Azure_client *client, std::string container,
                              std::string name, Http_request *req,
                              Http_response *resp,
                              const Http_client *http_client, Event_handler *h,
                              Azure_client::async_upload_callback_t callback,
                              CURLcode rc, const Http_connection *conn,
                              ulong count);

  static void download_callback(
      Azure_client *client, std::string container, std::string name,
      Http_request *req, Http_response *resp, const Http_client *http_client,
      Event_handler *h, Azure_client::async_download_callback_t callback,
      CURLcode rc, const Http_connection *conn, ulong count);

 public:
  Azure_client(const Http_client *client, const std::string &storage_account,
               const std::string &access_key, bool development_storage,
               const std::string &storage_class, const ulong max_retries,
               const ulong max_backoff);

  void set_endpoint(const std::string &ep, bool development_storage,
                    const std::string &storage_account);

  bool delete_object(const std::string &container, const std::string &name);

  bool create_container(const std::string &name);

  bool container_exists(const std::string &name, bool &exists);

  bool upload_object(const std::string &container, const std::string &name,
                     const Http_buffer &contents);

  Http_buffer download_object(const std::string &container,
                              const std::string &name, bool &success);

  bool async_upload_object(
      const std::string &container, const std::string &name,
      const Http_buffer &contents, Event_handler *h,
      async_upload_callback_t callback = {},
      const Http_request::headers_t &extra_http_headers = {});

  bool async_download_object(
      const std::string &container, const std::string &name, Event_handler *h,
      async_download_callback_t callback = {},
      const Http_request::headers_t &extra_http_headers = {});

  bool async_delete_object(const std::string &container,
                           const std::string &name, Event_handler *h,
                           const async_delete_callback_t callback);

  bool list_objects_with_prefix(const std::string &container,
                                const std::string &prefix,
                                std::vector<std::string> &objects);

  ulong get_max_retries() { return max_retries; }

  ulong get_max_backoff() { return max_backoff; }

  void retry_error(Http_response *resp, bool *retry) {}

  /* Not used */
  std::string hostname(const std::string &not_used) const { return host; }
};

class Azure_object_store : public Object_store {
 private:
  Azure_client azure_client;
  Http_request::headers_t extra_http_headers;

 public:
  Azure_object_store(const Http_client *client,
                     const std::string &storage_account,
                     const std::string &access_key, bool development_storage,
                     const std::string &storage_class, const ulong max_retries,
                     const ulong max_backoff,
                     const std::string &endpoint = std::string())
      : azure_client{
            client,        storage_account, access_key, development_storage,
            storage_class, max_retries,     max_backoff} {
    if (!endpoint.empty())
      azure_client.set_endpoint(endpoint, development_storage, storage_account);
  }
  void set_extra_http_headers(const Http_request::headers_t &headers) {
    extra_http_headers = headers;
  }
  virtual bool create_container(const std::string &name) override {
    return azure_client.create_container(name);
  }
  virtual bool container_exists(const std::string &name,
                                bool &exists) override {
    return azure_client.container_exists(name, exists);
  }
  virtual bool list_objects_in_directory(
      const std::string &container, const std::string &directory,
      std::vector<std::string> &objects) override {
    return azure_client.list_objects_with_prefix(container, directory + "/",
                                                 objects);
  }
  virtual bool upload_object(const std::string &container,
                             const std::string &object,
                             const Http_buffer &contents) override {
    return azure_client.upload_object(container, object, contents);
  }
  virtual bool async_upload_object(
      const std::string &container, const std::string &object,
      const Http_buffer &contents, Event_handler *h,
      std::function<void(bool, const Http_buffer &contents)> f = {}) override {
    return azure_client.async_upload_object(
        container, object, contents, h,
        [f](bool success, const Http_buffer &contents) {
          if (f) f(success, contents);
        },
        extra_http_headers);
  }
  virtual bool async_download_object(
      const std::string &container, const std::string &object, Event_handler *h,
      const std::function<void(bool, const Http_buffer &contents)> f = {})
      override {
    return azure_client.async_download_object(
        container, object, h,
        [f](bool success, const Http_buffer &contents) {
          if (f) f(success, contents);
        },
        extra_http_headers);
  }
  virtual bool async_delete_object(const std::string &container,
                                   const std::string &object, Event_handler *h,
                                   std::function<void(bool)> f = {}) override {
    return azure_client.async_delete_object(container, object, h,
                                            [f](bool success) {
                                              if (f) f(success);
                                            });
  }
  virtual bool delete_object(const std::string &container,
                             const std::string &name) override {
    return azure_client.delete_object(container, name);
  }
  virtual Http_buffer download_object(const std::string &container,
                                      const std::string &name,
                                      bool &success) override {
    return azure_client.download_object(container, name, success);
  }
};
}  // namespace xbcloud

#endif
