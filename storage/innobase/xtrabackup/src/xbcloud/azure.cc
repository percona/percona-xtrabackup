/******************************************************
Copyright (c)  2021 Percona LLC and/or its affiliates.

Azure client implementation.

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
#include "xbcloud/azure.h"
#include "xbcloud/http.h"
#include "xbcloud/util.h"

#include <iomanip>
#include <sstream>

#include <gcrypt.h>

#include <rapidxml/rapidxml.hpp>

#include <my_sys.h>
#include <iostream>

#include "common.h"
#include "msg.h"

namespace xbcloud {

const std::string AZURE_DATE_HEADER = "x-ms-date";
const std::string AZURE_VERSION_HEADER = "x-ms-version";
const std::string AZURE_BLOB_TYPE_HEADER = "x-ms-blob-type";
const std::string AZURE_STORAGE_CLASS_HEADER = "x-ms-access-tier";
const std::string AZURE_VERSION_DATE = "2020-06-12";
const std::string AZURE_DEVELOPMENT_HOST = "127.0.0.1:10000";
const std::string AZURE_HOST = ".blob.core.windows.net";

bool Azure_response::parse_http_response(Http_response &http_response) {
  using namespace rapidxml;

  if (http_response.body().size() == 0) {
    return false;
  }

  /* we need to make a copy of response body because rapidxml needs mutable
  zero-terminated string */
  std::string http_body(http_response.body().begin(),
                        http_response.body().end());

  if (http_response.headers().count("content-type") == 0 ||
      http_response.headers().at("content-type") != "application/xml") {
    error_ = true;
    error_message_ = http_body;
    return true;
  }

  xml_document<> doc;
  doc.parse<0>(&http_body[0]);

  auto root = doc.first_node();

  if (root != nullptr && strcmp(root->name(), "Error") == 0) {
    /* error response */
    error_ = true;

    auto node = root->first_node("Message");
    if (node != nullptr && node->value() != nullptr) {
      error_message_ = node->value();
    }

    node = root->first_node("Code");
    if (node != nullptr && node->value() != nullptr) {
      error_code_ = node->value();
    }
  }
  return true;
}

std::string Azure_signer::azure_date_format(time_t t) {
  struct tm tmp;
  char buf[40];
  strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", gmtime_r(&t, &tmp));
  return buf;
}

std::string Azure_signer::build_string_to_sign(Http_request &request,
                                               const std::string &container,
                                               const std::string &blob) {
  std::string signature;

  switch (request.method()) {
    case Http_request::GET:
      signature.append("GET\n");
      break;
    case Http_request::POST:
      signature.append("POST\n");
      break;
    case Http_request::PUT:
      signature.append("PUT\n");
      break;
    case Http_request::DELETE:
      signature.append("DELETE\n");
      break;
    case Http_request::HEAD:
      signature.append("HEAD\n");
      break;
  }

  if (request.has_header("Content-Encoding")) {
    signature.append(request.header_value("Content-Encoding"));
  }
  signature.append("\n");

  if (request.has_header("Content-Language")) {
    signature.append(request.header_value("Content-Language"));
  }
  signature.append("\n");

  if (request.has_header("Content-Length")) {
    signature.append(request.header_value("Content-Length"));
  }
  signature.append("\n");

  if (request.has_header("Content-MD5")) {
    signature.append(request.header_value("Content-MD5"));
  }
  signature.append("\n");

  if (request.has_header("Content-Type")) {
    signature.append(request.header_value("Content-Type"));
  }
  signature.append("\n");

  if (request.has_header("Date")) {
    signature.append(request.header_value("Date"));
  }
  signature.append("\n");

  /* 5 new lines for If-Modified-Since, If-Match, If-None-Match,
   If-Unmodified-Since, Range */
  signature.append("\n\n\n\n\n");

  /* append cannonical header that start with x-ms */
  for (const auto &header : request.headers()) {
    std::string name = header.first;
    if (name.find("x-ms") == 0) {
      std::string val = header.second;
      signature.append(name);
      signature.append(":");
      signature.append(val);
      signature.append("\n");
    }
  }

  /* append cannonical resource */

  if (development_storage) signature.append("/" + storage_account);

  signature.append("/" + storage_account);
  signature.append("/" + container);
  if (!blob.empty()) {
    signature.append("/" + uri_escape_path(blob));
  }

  for (auto &param : request.params()) {
    signature.append("\n" + param.first + ":" + param.second);
  }

  return signature;
}

void Azure_signer::sign_request(const std::string &container,
                                const std::string &blob, Http_request &req,
                                time_t t) {
  auto date_time = azure_date_format(t);
  req.add_header(AZURE_DATE_HEADER, date_time);

  req.add_header(AZURE_VERSION_HEADER, AZURE_VERSION_DATE);

  if (!storage_class.empty()) {
    req.add_header(AZURE_STORAGE_CLASS_HEADER, storage_class);
  }

  /* in case we updating already signed request */
  req.remove_header("Authorization");

  auto string_to_sign = build_string_to_sign(req, container, blob);

  std::string decoded_access_key = base64_decode(access_key);

  auto signature =
      base64_encode(hmac_sha256(decoded_access_key, string_to_sign));

  req.add_header("Authorization",
                 "SharedKey " + storage_account + ":" + signature);
}
void Azure_client::set_endpoint(const std::string &ep, bool development_storage,
                                const std::string &storage_account) {
  endpoint = ep;
  if (development_storage) {
    endpoint += "/" + storage_account;
  }
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

bool Azure_client::delete_object(const std::string &container,
                                 const std::string &name) {
  Http_request req(Http_request::DELETE, protocol, host,
                   "/" + container + "/" + name);
  signer->sign_request(container, name, req, time(0));

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    return true;
  }

  Azure_response azure_resp;
  if (!azure_resp.parse_http_response(resp)) {
    msg_ts("%s: Failed to delete object. Failed to parse XML response.\n",
           my_progname);
    return false;
  }

  if (azure_resp.error()) {
    msg_ts("%s: Failed to delete object. Error message: %s\n", my_progname,
           azure_resp.error_message().c_str());
    return false;
  }

  return true;
}

bool Azure_client::async_delete_object(const std::string &container,
                                       const std::string &name,
                                       Event_handler *h,
                                       const async_delete_callback_t callback) {
  Http_request *req = new Http_request(Http_request::DELETE, protocol, host,
                                       "/" + container + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to delere object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    return false;
  }
  signer->sign_request(container, name, *req, time(0));

  Http_response *resp = new Http_response();
  if (resp == nullptr) {
    msg_ts("%s: Failed to delete object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    delete req;
    return false;
  }

  auto f = [callback, container, name, req, resp](
               CURLcode rc, const Http_connection *conn) mutable -> void {
    if (rc == CURLE_OK && !resp->ok()) {
      Azure_response azure_resp;
      if (!azure_resp.parse_http_response(*resp)) {
        msg_ts(
            "%s: Failed to delete object %s/%s. Failed to parse XML "
            "response.\n",
            my_progname, container.c_str(), name.c_str());
      } else if (azure_resp.error()) {
        msg_ts("%s: Failed to delete object %s/%s. Error message: %s\n",
               my_progname, container.c_str(), name.c_str(),
               azure_resp.error_message().c_str());
      }
    }
    if (callback) {
      callback(rc == CURLE_OK && resp->ok());
    }
    delete req;
    delete resp;
  };

  http_client->make_async_request(*req, *resp, h, f);

  return true;
}

Http_buffer Azure_client::download_object(const std::string &container,
                                          const std::string &name,
                                          bool &success) {
  Http_request req(Http_request::GET, protocol, host, container + "/" + name);
  signer->sign_request(container, name, req, time(0));

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    success = false;
    return Http_buffer();
  }

  if (!resp.ok()) {
    success = false;
    return Http_buffer();
  }

  success = true;
  return resp.move_body();
}

bool Azure_client::create_container(const std::string &name) {
  Http_request req(Http_request::PUT, protocol, host, "/" + name);
  req.add_param("restype", "container");
  signer->sign_request(name, "", req, time(0));

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    return true;
  }

  Azure_response azure_resp;
  if (!azure_resp.parse_http_response(resp)) {
    msg_ts("%s: Failed to create container. Failed to parse XML response.\n",
           my_progname);
    return false;
  }

  if (azure_resp.error()) {
    msg_ts("%s: Failed to create container. Error message: %s\n", my_progname,
           azure_resp.error_message().c_str());
    return false;
  }

  return true;
}

bool Azure_client::container_exists(const std::string &name, bool &exists) {
  Http_request req(Http_request::HEAD, protocol, host, "/" + name);
  req.add_param("comp", "metadata");
  req.add_param("restype", "container");
  signer->sign_request(name, "", req, time(0));

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    exists = true;
    return true;
  }

  if (resp.http_code() == 404) {
    exists = false;
    return true;
  }

  return false;
}

bool Azure_client::upload_object(const std::string &container,
                                 const std::string &name,
                                 const Http_buffer &contents) {
  Http_request req(Http_request::PUT, protocol, host, container + "/" + name);
  req.add_header("Content-Length", std::to_string(contents.size()));
  req.add_header(AZURE_BLOB_TYPE_HEADER, "BlockBlob");
  req.append_payload(contents);
  signer->sign_request(container, name, req, time(0));

  Http_response resp;

  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    return true;
  }

  Azure_response azure_resp;
  if (!azure_resp.parse_http_response(resp)) {
    msg_ts("%s: Failed to upload object. Failed to parse XML response.\n",
           my_progname);
    return false;
  }

  if (azure_resp.error()) {
    msg_ts("%s: Failed to upload object. Error message: %s\n", my_progname,
           azure_resp.error_message().c_str());
    return false;
  }

  msg_ts("%s: Failed to upload object. Http error code: %lu\n", my_progname,
         resp.http_code());

  return false;
}

void Azure_client::upload_callback(
    Azure_client *client, std::string container, std::string name,
    Http_request *req, Http_response *resp, const Http_client *http_client,
    Event_handler *h, Azure_client::async_upload_callback_t callback,
    CURLcode rc, const Http_connection *conn, ulong count) {
  http_client->callback(client, container, name, req, resp, http_client, h,
                        callback, rc, conn, count);
}

bool Azure_client::async_upload_object(
    const std::string &container, const std::string &name,
    const Http_buffer &contents, Event_handler *h,
    async_upload_callback_t callback,
    const Http_request::headers_t &extra_http_headers) {
  Http_request *req = new Http_request(Http_request::PUT, protocol, host,
                                       "/" + container + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to upload object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    return false;
  }
  req->add_header("Content-Length", std::to_string(contents.size()));
  req->add_header("Content-Type", "application/octet-stream");
  req->add_header(AZURE_BLOB_TYPE_HEADER, "BlockBlob");

  for (const auto &h : extra_http_headers) {
    req->add_header(h.first, h.second);
  }
  req->append_payload(contents);
  signer->sign_request(container, name, *req, time(0));

  Http_response *resp = new Http_response();
  if (resp == nullptr) {
    msg_ts("%s: Failed to upload object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    delete req;
    return false;
  }

  http_client->make_async_request(
      *req, *resp, h,
      std::bind(Azure_client::upload_callback, this, container, name, req, resp,
                http_client, h, callback, std::placeholders::_1,
                std::placeholders::_2, 1));

  return true;
}

void Azure_client::download_callback(
    Azure_client *client, std::string container, std::string name,
    Http_request *req, Http_response *resp, const Http_client *http_client,
    Event_handler *h, Azure_client::async_download_callback_t callback,
    CURLcode rc, const Http_connection *conn, ulong count) {
  http_client->callback(client, container, name, req, resp, http_client, h,
                        callback, rc, conn, count);
}

bool Azure_client::async_download_object(
    const std::string &container, const std::string &name, Event_handler *h,
    const async_download_callback_t callback,
    const Http_request::headers_t &extra_http_headers) {
  Http_request *req = new Http_request(Http_request::GET, protocol, host,
                                       "/" + container + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to download object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    return false;
  }
  for (const auto &h : extra_http_headers) {
    req->add_header(h.first, h.second);
  }
  signer->sign_request(container, name, *req, time(0));

  Http_response *resp = new Http_response();
  if (resp == nullptr) {
    msg_ts("%s: Failed to download object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    delete req;
    return false;
  }

  http_client->make_async_request(
      *req, *resp, h,
      std::bind(Azure_client::download_callback, this, container, name, req,
                resp, http_client, h, callback, std::placeholders::_1,
                std::placeholders::_2, 1));

  return true;
}

Azure_client::Azure_client(const Http_client *client,
                           const std::string &storage_account,
                           const std::string &access_key,
                           bool development_storage,
                           const std::string &storage_class,
                           const ulong max_retries, const ulong max_backoff)
    : http_client(client),
      storage_account(storage_account),
      access_key(access_key),
      max_retries(max_retries),
      max_backoff(max_backoff) {
  if (development_storage) {
    host = AZURE_DEVELOPMENT_HOST + "/" + storage_account;
    endpoint = "http://" + host;
    protocol = Http_request::HTTP;
  } else {
    host = storage_account + AZURE_HOST;
    endpoint = "https://" + host;
    protocol = Http_request::HTTPS;
  }
  rtrim_slashes(host);
  signer = std::unique_ptr<Azure_signer>(new Azure_signer(
      storage_account, access_key, development_storage, storage_class));
}

bool Azure_client::list_objects_with_prefix(const std::string &container,
                                            const std::string &prefix,
                                            std::vector<std::string> &objects) {
  bool truncated = true;
  std::string next_marker;

  while (truncated) {
    Http_request req(Http_request::GET, protocol, host, "/" + container);
    req.add_param("comp", "list");
    req.add_param("restype", "container");
    req.add_param("maxresults", "1000");
    req.add_param("prefix", prefix);
    if (truncated == true) {
      req.add_param("marker", next_marker);
    }
    signer->sign_request(container, "", req, time(0));

    Http_response resp;
    if (!http_client->make_request(req, resp)) {
      return false;
    }

    if (!resp.ok()) {
      Azure_response azure_resp;
      if (!azure_resp.parse_http_response(resp)) {
        msg_ts("%s: Failed to list objects. Failed to parse XML response.\n",
               my_progname);
      } else if (azure_resp.error()) {
        msg_ts("%s: Failed to list objects. Error message: %s\n", my_progname,
               azure_resp.error_message().c_str());
      }
      return false;
    }

    rapidxml::xml_document<> doc;
    std::string resp_body(resp.body().begin(), resp.body().end());

    doc.parse<0>(&resp_body[0]);

    auto root = doc.first_node("EnumerationResults");
    if (root == nullptr) {
      msg_ts(
          "%s: Failed to parse list container result. Root node is not "
          "found.\n",
          my_progname);
      return false;
    }

    auto next_marker_node = root->first_node("NextMarker");
    if (next_marker_node == nullptr) {
      truncated = false;
    } else {
      next_marker = next_marker_node->value();
      if (!next_marker.empty()) {
        truncated = true;
      } else {
        truncated = false;
      }
    }

    auto blobs_node = root->first_node("Blobs");
    if (blobs_node == nullptr) {
      msg_ts(
          "%s: Failed to parse list container result. Blobs is not "
          "found.\n",
          my_progname);
      return false;
    }

    auto node = blobs_node->first_node("Blob");
    while (node != nullptr) {
      auto name = node->first_node("Name");
      if (name == nullptr) {
        msg_ts(
            "%s: Failed to parse list container result. Cannot find object "
            "name.\n",
            my_progname);
        return false;
      }
      objects.push_back(name->value());
      node = node->next_sibling("Blob");
    }
  }

  return true;
}

}  // namespace xbcloud
