/******************************************************
Copyright (c) 2019, 2021 Percona LLC and/or its affiliates.

Openstack Swift client implementation.

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

#ifndef __XBCLOUD_SWIFT_H__
#define __XBCLOUD_SWIFT_H__

#include "object_store.h"
#include "xbcloud/http.h"
#include "xbcloud/util.h"

namespace xbcloud {

static inline void split_url(const std::string &url,
                             Http_request::protocol_t &protocol,
                             std::string &host, std::string &path) {
  std::string tmp = url;
  if (tmp.find("https://") == 0) {
    protocol = Http_request::HTTPS;
    tmp = tmp.substr(8);
  } else if (tmp.find("http://") == 0) {
    protocol = Http_request::HTTP;
    tmp = url.substr(7);
  } else {
    return;
  }
  auto host_endp = tmp.find('/');
  if (host_endp != std::string::npos) {
    host = tmp.substr(0, host_endp);
    path = tmp.substr(host_endp);
  } else {
    host = tmp;
    path = "/";
  }
  /* make sure there is exactly one trailing slash */
  rtrim_slashes(path);
  path.append("/");
}

class Keystone_client {
 public:
  struct auth_info_t {
    std::string url;
    std::string token;
  };

 private:
  const Http_client *http_client;
  Http_request::protocol_t protocol;
  std::string auth_url;
  std::string host;
  std::string path;

  std::string user;
  std::string key;
  std::string password;
  std::string tenant;
  std::string tenant_id;

  std::string project;
  std::string project_id;

  std::string domain;
  std::string domain_id;

  std::string project_domain;
  std::string project_domain_id;

 public:
  Keystone_client(const Http_client *client, const std::string &auth_url)
      : http_client(client), auth_url(auth_url) {
    split_url(auth_url, protocol, host, path);
  }
  bool temp_auth(auth_info_t &auth_info);
  bool auth_v2(const std::string &swift_region, auth_info_t &auth_info);
  bool auth_v3(const std::string &swift_region, auth_info_t &auth_info);

  void set_user(const std::string &val) { user = val; }
  void set_key(const std::string &val) { key = val; }
  void set_password(const std::string &val) { password = val; }
  void set_tenant(const std::string &val) { tenant = val; }
  void set_tenant_id(const std::string &val) { tenant_id = val; }

  void set_project(const std::string &val) { project = val; }
  void set_project_id(const std::string &val) { project_id = val; }

  void set_domain(const std::string &val) { domain = val; }
  void set_domain_id(const std::string &val) { domain_id = val; }

  void set_project_domain(const std::string &val) { project_domain = val; }
  void set_project_domain_id(const std::string &val) {
    project_domain_id = val;
  }
};

/* Not used */
class Swift_signer {
 public:
  void sign_request(std::string, std::string &, Http_request &, time_t) {}
  ~Swift_signer() {}
};

class Swift_client {
 public:
  using async_upload_callback_t =
      std::function<void(bool, const Http_buffer &)>;
  using async_download_callback_t =
      std::function<void(bool, const Http_buffer &)>;
  using async_delete_callback_t = std::function<void(bool)>;
  std::unique_ptr<Swift_signer> signer; /* Not used */

 private:
  const Http_client *http_client;

  std::string url;
  std::string token;

  Http_request::protocol_t protocol;
  std::string host;
  std::string path;

  ulong max_retries;
  ulong max_backoff;
  static bool validate_response(const Http_request &req,
                                const Http_response &resp);

  static void upload_callback(Swift_client *client, std::string container,
                              std::string name, Http_request *req,
                              Http_response *resp,
                              const Http_client *http_client, Event_handler *h,
                              Swift_client::async_upload_callback_t callback,
                              CURLcode rc, const Http_connection *conn,
                              ulong count);

  static void download_callback(
      Swift_client *client, std::string container, std::string name,
      Http_request *req, Http_response *resp, const Http_client *http_client,
      Event_handler *h, Swift_client::async_download_callback_t callback,
      CURLcode rc, const Http_connection *conn, ulong count);

 public:
  Swift_client(const Http_client *client, const std::string &url,
               const std::string &token, const ulong max_retries,
               const ulong max_backoff)
      : http_client(client),
        url(url),
        token(token),
        max_retries(max_retries),
        max_backoff(max_backoff) {
    split_url(url, protocol, host, path);
  }

  bool delete_object(const std::string &container, const std::string &name);

  Http_buffer download_object(const std::string &container,
                              const std::string &name, bool &success);

  bool create_container(const std::string &name);

  bool container_exists(const std::string &name, bool &exists);

  bool upload_object(const std::string &container, const std::string &name,
                     const Http_buffer &contents);

  bool async_upload_object(const std::string &container,
                           const std::string &name, const Http_buffer &contents,
                           Event_handler *h,
                           async_upload_callback_t callback = {});

  bool async_download_object(const std::string &container,
                             const std::string &name, Event_handler *h,
                             async_download_callback_t callback = {});

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

class Swift_object_store : public Object_store {
 private:
  Swift_client swift_client;

 public:
  Swift_object_store(const Http_client *http_client, const std::string &url,
                     const std::string &token, const ulong max_retries,
                     const ulong max_backoff)
      : swift_client{http_client, url, token, max_retries, max_backoff} {}
  virtual bool create_container(const std::string &name) override {
    return swift_client.create_container(name);
  }
  virtual bool container_exists(const std::string &name,
                                bool &exists) override {
    return swift_client.container_exists(name, exists);
  }
  virtual bool list_objects_in_directory(
      const std::string &container, const std::string &directory,
      std::vector<std::string> &objects) override {
    return swift_client.list_objects_with_prefix(container, directory + "/",
                                                 objects);
  }
  virtual bool upload_object(const std::string &container,
                             const std::string &object,
                             const Http_buffer &contents) override {
    return swift_client.upload_object(container, object, contents);
  }
  virtual bool async_upload_object(
      const std::string &container, const std::string &object,
      const Http_buffer &contents, Event_handler *h,
      std::function<void(bool, const Http_buffer &contents)> f = {}) override {
    return swift_client.async_upload_object(
        container, object, contents, h,
        [f](bool success, const Http_buffer &contents) {
          if (f) f(success, contents);
        });
  }
  virtual bool async_download_object(
      const std::string &container, const std::string &object, Event_handler *h,
      std::function<void(bool, const Http_buffer &contents)> f = {}) override {
    return swift_client.async_download_object(
        container, object, h, [f](bool success, const Http_buffer &contents) {
          if (f) f(success, contents);
        });
  }
  virtual bool async_delete_object(const std::string &container,
                                   const std::string &object, Event_handler *h,
                                   std::function<void(bool)> f = {}) override {
    return swift_client.async_delete_object(container, object, h,
                                            [f](bool success) {
                                              if (f) f(success);
                                            });
  }
  virtual bool delete_object(const std::string &container,
                             const std::string &name) override {
    return swift_client.delete_object(container, name);
  }
  virtual Http_buffer download_object(const std::string &container,
                                      const std::string &name,
                                      bool &success) override {
    return swift_client.download_object(container, name, success);
  }
};

}  // namespace xbcloud

#endif
