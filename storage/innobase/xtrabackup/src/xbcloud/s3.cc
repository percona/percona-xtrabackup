/******************************************************
Copyright (c) 2019, 2021 Percona LLC and/or its affiliates.

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

#include "xbcloud/s3.h"
#include "xbcloud/util.h"

#include <iomanip>
#include <sstream>

#include <gcrypt.h>

#include <rapidxml/rapidxml.hpp>

#include <my_sys.h>

#include "common.h"
#include "msg.h"

#define AWS_DATE_HEADER "X-Amz-Date"
#define AWS_CONTENT_SHA256_HEADER "X-Amz-Content-SHA256"
#define AWS_SESSION_TOKEN_HEADER "X-Amz-Security-Token"
#define AWS_SIGNATURE_ALGORITHM "AWS4-HMAC-SHA256"
#define AWS_STORAGE_CLASS_HEADER "X-Amz-Storage-Class"

namespace xbcloud {


bool S3_response::parse_http_response(Http_response &http_response) {
  using namespace rapidxml;

  if (http_response.body().size() == 0) {
    return false;
  }

  /* we need to make a copy of response body because rapidxml needs mutable
  zero-terminated string */
  std::string s(http_response.body().begin(), http_response.body().end());

  if (http_response.headers().count("content-type") == 0 ||
      http_response.headers().at("content-type") != "application/xml") {
    error_ = true;
    error_message_ = s;
    return true;
  }

  xml_document<> doc;
  doc.parse<0>(&s[0]);

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

  std::string content_sha256 = hex_encode(request.payload().sha256());
  request.add_header(AWS_CONTENT_SHA256_HEADER, content_sha256);
  
  std::string content_md5 = base64_encode(request.payload().md5());
  request.add_header("Content-MD5", content_md5);
  
  /* canonical URI */
  canonical_request << request.path() << "\n";

  /* canonical query string */
  canonical_request << request.query_string() << "\n";

  std::map<std::string, std::string> keys;

  for (const auto &header : request.headers()) {
    std::string canonical_key = header.first;
    to_lower(canonical_key);
    keys[canonical_key] = header.first;
  }

  /* canonical headers */
  for (const auto &key : keys) {
    std::string canonical_name = key.first;
    to_lower(canonical_name);
    std::string canonical_value = request.headers().at(key.second);
    canonicalize_http_header_value(canonical_value);
    canonical_request << canonical_name << ":" << canonical_value << "\n";
    if (!signed_headers.empty()) signed_headers.append(";");
    signed_headers.append(canonical_name);
  }
  canonical_request << "\n" << signed_headers << "\n";

  canonical_request << content_sha256;

  return hex_encode(sha256(canonical_request.str()));
}

std::string S3_signerV4::build_string_to_sign(Http_request &request,
                                              std::string &signed_headers) {
  std::string s = AWS_SIGNATURE_ALGORITHM "\n";
  std::string date = request.header_value(AWS_DATE_HEADER);
  s.append(date);
  s.append("\n");
  s.append(date.substr(0, 8));
  s.append("/");
  s.append(region);
  s.append("/s3/aws4_request\n");
  s.append(build_hashed_canonical_request(request, signed_headers));

  return s;
}

void S3_signerV4::sign_request(const std::string &hostname,
                               const std::string &bucket, Http_request &req,
                               time_t t) {
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
  auto k_service = hmac_sha256(k_region, "s3");
  auto k_signing = hmac_sha256(k_service, "aws4_request");

  auto signature = hex_encode(hmac_sha256(k_signing, string_to_sign));

  std::string sig_header;
  sig_header.append(AWS_SIGNATURE_ALGORITHM);
  sig_header.append(" Credential=");
  sig_header.append(access_key);
  sig_header.append("/");
  sig_header.append(date);
  sig_header.append("/");
  sig_header.append(region);
  sig_header.append("/s3/aws4_request");

  sig_header.append(", SignedHeaders=");
  sig_header.append(signed_headers);

  sig_header.append(", Signature=");
  sig_header.append(signature);

  req.add_header("Authorization", sig_header);
}

std::string S3_signerV2::aws_date_format(time_t t) {
  struct tm tmp;
  char buf[40];
  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", gmtime_r(&t, &tmp));
  return buf;
}

std::string S3_signerV2::build_string_to_sign(const std::string &bucket,
                                              Http_request &request) {
  std::string s;

  switch (request.method()) {
    case Http_request::GET:
      s.append("GET\n");
      break;
    case Http_request::POST:
      s.append("POST\n");
      break;
    case Http_request::PUT:
      s.append("PUT\n");
      break;
    case Http_request::DELETE:
      s.append("DELETE\n");
      break;
    case Http_request::HEAD:
      s.append("HEAD\n");
      break;
  }
  if (request.has_header("Content-MD5")) {
    s.append(request.header_value("Content-MD5"));
  } else {
    std::string content_md5 = base64_encode(request.payload().md5());
    request.add_header("Content-MD5", content_md5);
    s.append(content_md5);
  }
  s.append("\n");
  if (request.has_header("Content-Type")) {
    s.append(request.header_value("Content-Type"));
  }
  s.append("\n");
  if (request.has_header("Date")) {
    s.append(request.header_value("Date"));
  }
  s.append("\n");

  for (const auto &header : request.headers()) {
    std::string name = header.first;
    to_lower(name);
    if (name.find("x-amz") == 0) {
      std::string val = header.second;
      to_lower(val);
      s.append(name);
      s.append("=");
      s.append(val);
      s.append("\n");
    }
  }

  if (!bucket.empty()) {
    s.append("/" + bucket);
  }
  s.append(request.path());

  return s;
}

void S3_signerV2::sign_request(const std::string &hostname,
                               const std::string &bucket, Http_request &req,
                               time_t t) {
  auto date_time = aws_date_format(t);
  req.add_header("Date", date_time);
  /* in case we updating already signed request */
  req.remove_header("Authorization");

  std::string signed_headers;
  auto string_to_sign =
      build_string_to_sign(lookup == LOOKUP_PATH ? std::string() : bucket, req);

  auto signature = base64_encode(hmac_sha1(secret_key, string_to_sign));

  req.add_header("Authorization", "AWS " + access_key + ":" + signature);
}

bool S3_client::delete_object(const std::string &bucket,
                              const std::string &name) {
  Http_request req(Http_request::DELETE, protocol, hostname(bucket),
                   bucketname(bucket) + "/" + name);
  signer->sign_request(hostname(bucket), bucket, req, time(0));

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    return true;
  }

  S3_response s3_resp;
  if (!s3_resp.parse_http_response(resp)) {
    msg_ts("%s: Failed to delete object. Failed to parse XML response.\n",
           my_progname);
    return false;
  }

  if (s3_resp.error()) {
    msg_ts("%s: Failed to delete object. Error message: %s\n", my_progname,
           s3_resp.error_message().c_str());
    return false;
  }

  return true;
}

bool S3_client::async_delete_object(const std::string &bucket,
                                    const std::string &name, Event_handler *h,
                                    const async_delete_callback_t callback) {
  Http_request *req =
      new Http_request(Http_request::DELETE, protocol, hostname(bucket),
                       bucketname(bucket) + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to delere object %s/%s. Out of memory.\n", my_progname,
           bucket.c_str(), name.c_str());
    return false;
  }
  signer->sign_request(hostname(bucket), bucket, *req, time(0));

  Http_response *resp = new Http_response();
  if (resp == nullptr) {
    msg_ts("%s: Failed to delete object %s/%s. Out of memory.\n", my_progname,
           bucket.c_str(), name.c_str());
    delete req;
    return false;
  }

  auto f = [callback, bucket, name, req, resp](
               CURLcode rc, const Http_connection *conn) mutable -> void {
    if (rc == CURLE_OK && !resp->ok()) {
      S3_response s3_resp;
      if (!s3_resp.parse_http_response(*resp)) {
        msg_ts(
            "%s: Failed to delete object %s/%s. Failed to parse XML "
            "response.\n",
            my_progname, bucket.c_str(), name.c_str());
      } else if (s3_resp.error()) {
        msg_ts("%s: Failed to delete object %s/%s. Error message: %s\n",
               my_progname, bucket.c_str(), name.c_str(),
               s3_resp.error_message().c_str());
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

Http_buffer S3_client::download_object(const std::string &bucket,
                                       const std::string &name, bool &success) {
  Http_request req(Http_request::GET, protocol, hostname(bucket),
                   bucketname(bucket) + "/" + name);
  signer->sign_request(hostname(bucket), bucket, req, time(0));

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

bool S3_client::create_bucket(const std::string &name) {
  Http_request req(Http_request::PUT, protocol, hostname(name),
                   bucketname(name) + "/");

  if (default_s3_region != region) {
    req.append_payload(
        "<CreateBucketConfiguration "
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
        "\"><LocationConstraint>");
    req.append_payload(region);
    req.append_payload("</LocationConstraint></CreateBucketConfiguration>");
  }
  signer->sign_request(hostname(name), name, req, time(0));

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    return true;
  }

  S3_response s3_resp;
  if (!s3_resp.parse_http_response(resp)) {
    msg_ts("%s: Failed to create bucket. Failed to parse XML response.\n",
           my_progname);
    return false;
  }

  if (s3_resp.error()) {
    msg_ts("%s: Failed to create bucket. Error message: %s\n", my_progname,
           s3_resp.error_message().c_str());
    return false;
  }

  return true;
}

bool S3_client::probe_api_version_and_lookup(const std::string &bucket) {
  for (auto lookup : {LOOKUP_DNS, LOOKUP_PATH}) {
    if (bucket_lookup != LOOKUP_AUTO && bucket_lookup != lookup) {
      continue;
    }
    for (auto version : {S3_V4, S3_V2}) {
      if (api_version != S3_V_AUTO && api_version != version) {
        continue;
      }
      if (version == S3_V2) {
        signer = std::unique_ptr<S3_signer>(
            new S3_signerV2(lookup, region, access_key, secret_key));
      } else {
        signer = std::unique_ptr<S3_signer>(
            new S3_signerV4(lookup, region, access_key, secret_key,
                            session_token, storage_class));
      }
      auto tmp_lookup = bucket_lookup;
      auto tmp_version = api_version;

      bucket_lookup = lookup;
      api_version = version;

      bool exists;
      if (bucket_exists(bucket.c_str(), exists)) {
        msg_ts("%s: Successfully connected.\n", my_progname);
        return true;
      }

      bucket_lookup = tmp_lookup;
      api_version = tmp_version;

      http_client->reset();
    }
  }

  msg_ts(
      "%s: Probe failed. Please check your credentials and endpoint "
      "settings.\n",
      my_progname);

  return false;
}

bool S3_client::bucket_exists(const std::string &name, bool &exists) {
  Http_request req(Http_request::HEAD, protocol, hostname(name),
                   bucketname(name) + "/");
  signer->sign_request(hostname(name), name, req, time(0));

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

bool S3_client::upload_object(const std::string &bucket,
                              const std::string &name,
                              const Http_buffer &contents) {
  Http_request req(Http_request::PUT, protocol, hostname(bucket),
                   bucketname(bucket) + "/" + name);
  req.add_header("Content-Type", "application/octet-stream");
  req.append_payload(contents);
  signer->sign_request(hostname(bucket), bucket, req, time(0));

  Http_response resp;

  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    return true;
  }

  S3_response s3_resp;
  if (!s3_resp.parse_http_response(resp)) {
    msg_ts("%s: Failed to upload object. Failed to parse XML response.\n",
           my_progname);
    return false;
  }

  if (s3_resp.error()) {
    msg_ts("%s: Failed to upload object. Error message: %s\n", my_progname,
           s3_resp.error_message().c_str());
    return false;
  }

  msg_ts("%s: Failed to upload object. Http error code: %lu\n", my_progname,
         resp.http_code());

  return false;
}

void S3_client::upload_callback(
    S3_client *client, std::string bucket, std::string name, Http_request *req,
    Http_response *resp, const Http_client *http_client, Event_handler *h,
    S3_client::async_upload_callback_t callback, CURLcode rc,
    const Http_connection *conn, ulong count) {
  http_client->callback(client, bucket, name, req, resp, http_client, h,
                        callback, rc, conn, count);
}

bool S3_client::async_upload_object(
    const std::string &bucket, const std::string &name,
    const Http_buffer &contents, Event_handler *h,
    async_upload_callback_t callback,
    const Http_request::headers_t &extra_http_headers) {
  Http_request *req =
      new Http_request(Http_request::PUT, protocol, hostname(bucket),
                       bucketname(bucket) + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to upload object %s/%s. Out of memory.\n", my_progname,
           bucket.c_str(), name.c_str());
    return false;
  }
  req->add_header("Content-Type", "application/octet-stream");
  for (const auto &h : extra_http_headers) {
    req->add_header(h.first, h.second);
  }
  req->append_payload(contents);
  signer->sign_request(hostname(bucket), bucket, *req, time(0));

  Http_response *resp = new Http_response();
  if (resp == nullptr) {
    msg_ts("%s: Failed to upload object %s/%s. Out of memory.\n", my_progname,
           bucket.c_str(), name.c_str());
    delete req;
    return false;
  }

  http_client->make_async_request(
      *req, *resp, h,
      std::bind(S3_client::upload_callback, this, bucket, name, req, resp,
                http_client, h, callback, std::placeholders::_1,
                std::placeholders::_2, 1));

  return true;
}

void S3_client::download_callback(
    S3_client *client, std::string bucket, std::string name, Http_request *req,
    Http_response *resp, const Http_client *http_client, Event_handler *h,
    S3_client::async_download_callback_t callback, CURLcode rc,
    const Http_connection *conn, ulong count) {
  http_client->callback(client, bucket, name, req, resp, http_client, h,
                        callback, rc, conn, count);
}

bool S3_client::async_download_object(
    const std::string &bucket, const std::string &name, Event_handler *h,
    const async_download_callback_t callback,
    const Http_request::headers_t &extra_http_headers) {
  Http_request *req =
      new Http_request(Http_request::GET, protocol, hostname(bucket),
                       bucketname(bucket) + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to download object %s/%s. Out of memory.\n", my_progname,
           bucket.c_str(), name.c_str());
    return false;
  }
  for (const auto &h : extra_http_headers) {
    req->add_header(h.first, h.second);
  }
  signer->sign_request(hostname(bucket), bucket, *req, time(0));

  Http_response *resp = new Http_response();
  if (resp == nullptr) {
    msg_ts("%s: Failed to download object %s/%s. Out of memory.\n", my_progname,
           bucket.c_str(), name.c_str());
    delete req;
    return false;
  }

  http_client->make_async_request(
      *req, *resp, h,
      std::bind(S3_client::download_callback, this, bucket, name, req, resp,
                http_client, h, callback, std::placeholders::_1,
                std::placeholders::_2, 1));

  return true;
}

bool S3_client::list_objects_with_prefix(const std::string &bucket,
                                         const std::string &prefix,
                                         std::vector<std::string> &objects) {
  bool truncated = true;
  std::string continuation_token;
  std::string next_marker;

  while (truncated) {
    Http_request req(Http_request::GET, protocol, hostname(bucket),
                     bucketname(bucket) + "/");
    req.add_param("max-keys", "1000");
    req.add_param("prefix", prefix);
    if (!next_marker.empty()) {
      req.add_param("marker", next_marker);
    }
    if (!continuation_token.empty()) {
      req.add_param("continuation-token", continuation_token);
    }
    signer->sign_request(hostname(bucket), bucket, req, time(0));

    Http_response resp;
    if (!http_client->make_request(req, resp)) {
      return false;
    }

    if (!resp.ok()) {
      S3_response s3_resp;
      if (!s3_resp.parse_http_response(resp)) {
        msg_ts("%s: Failed to list objects. Failed to parse XML response.\n",
               my_progname);
      } else if (s3_resp.error()) {
        msg_ts("%s: Failed to list objects. Error message: %s\n", my_progname,
               s3_resp.error_message().c_str());
      }
      return false;
    }

    rapidxml::xml_document<> doc;
    std::string s(resp.body().begin(), resp.body().end());

    doc.parse<0>(&s[0]);

    auto root = doc.first_node("ListBucketResult");
    if (root == nullptr) {
      msg_ts(
          "%s: Failed to parse list bucket result. Root node is not found.\n",
          my_progname);
      return false;
    }

    auto truncated_node = root->first_node("IsTruncated");
    if (truncated_node == nullptr) {
      msg_ts(
          "%s: Failed to parse list bucket result. IsTruncated is not "
          "found.\n",
          my_progname);
      return false;
    }

    truncated = strcmp(truncated_node->value(), "true") == 0;

    next_marker = continuation_token = std::string();

    if (truncated) {
      auto next_marker_node = root->first_node("NextMarker");
      if (next_marker_node == nullptr) {
        auto continuation_token_node =
            root->first_node("NextContinuationToken");
        if (continuation_token_node != nullptr) {
          continuation_token = continuation_token_node->value();
        }
      } else {
        next_marker = next_marker_node->value();
      }
    }

    auto node = root->first_node("Contents");
    bool storage_class_message_sent = false;
    while (node != nullptr) {
      auto key = node->first_node("Key");
      if (key == nullptr) {
        msg_ts(
            "%s: Failed to parse list bucket result. Cannot find object "
            "name.\n",
            my_progname);
        return false;
      }
      if (storage_class != "" && !storage_class_message_sent) {
        auto obj_storage_class = node->first_node("StorageClass");
        if (obj_storage_class != nullptr &&
            storage_class != obj_storage_class->value()) {
          storage_class_message_sent = true;
          msg_ts(
              "%s: Warning: expected objects using storage class %s but found "
              "%s\n",
              my_progname, storage_class.c_str(), obj_storage_class->value());
        }
      }
      objects.push_back(key->value());
      node = node->next_sibling("Contents");
    }

    if (truncated && next_marker.empty() && continuation_token.empty()) {
      next_marker = objects.back();
    }
  }

  return true;
}

void S3_client::retry_error(Http_response *resp, bool *retry) {
  S3_response s3_resp;
  if (!s3_resp.parse_http_response(*resp)) {
    msg_ts("%s: Failed to parse XML response.\n", my_progname);
  } else if (s3_resp.error()) {
    msg_ts("%s: S3 error message: %s\n", my_progname,
           s3_resp.error_message().c_str());
    if (s3_resp.error_code() == "RequestTimeout") {
      *retry = true;
    }
    if (s3_resp.error_code() == "ExpiredToken" ||
        s3_resp.error_code() == "InvalidToken" ||
        s3_resp.error_code() == "TokenRefreshRequired") {
      if (api_version == S3_V4 && ec2_instance != nullptr &&
          ec2_instance->get_is_ec2_instance_with_profile() &&
          ec2_instance->fetch_metadata()) {
        access_key = ec2_instance->get_access_key();
        secret_key = ec2_instance->get_secret_key();
        session_token = ec2_instance->get_session_token();
        signer.get()->update_keys(access_key, secret_key, session_token);
        *retry = true;
      }
    }
  }
}

}  // namespace xbcloud
