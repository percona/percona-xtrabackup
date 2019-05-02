/******************************************************
Copyright (c) 2019 Percona LLC and/or its affiliates.

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

#include "xbcloud/swift.h"
#include "xbcloud/util.h"

#include <my_sys.h>

#include <gcrypt.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "common.h"

namespace xbcloud {

const rapidjson::Value &json_get_member(const rapidjson::Document &doc,
                                        const char *name) {
  if (!doc.IsObject()) {
    msg_ts("%s: cannot get member '%s' of non object", my_progname, name);
  }
  auto member = doc.FindMember(name);
  if (member == doc.MemberEnd()) {
    msg_ts("%s: cannot find member '%s' of an object", my_progname, name);
  }
  return member->value;
}

const rapidjson::Value &json_get_member(const rapidjson::Value &obj,
                                        const char *name) {
  if (!obj.IsObject()) {
    msg_ts("%s: cannot get member '%s' of non object", my_progname, name);
  }
  auto member = obj.FindMember(name);
  if (member == obj.MemberEnd()) {
    msg_ts("%s: cannot find member '%s' of an object", my_progname, name);
  }
  return member->value;
}

bool Keystone_client::temp_auth(auth_info_t &auth_info) {
  if (user.empty()) {
    msg_ts("%s: Swift user must be sepcified for TempAuth\n", my_progname);
    return false;
  }

  if (key.empty()) {
    msg_ts("%s: Swift key must be sepcified for TempAuth\n", my_progname);
    return false;
  }

  Http_request req(Http_request::GET, protocol, host, path);
  req.add_header("X-Auth-User", user);
  req.add_header("X-Auth-Key", key);

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (!resp.ok()) {
    msg_ts("%s: TempAuth failed with code %ld\n", my_progname,
           resp.http_code());
    return false;
  }

  if (resp.headers().count("x-storage-url") < 1) {
    msg_ts("%s: TempAuth failed. Server did not respond with X-Storage-Url\n",
           my_progname);
    return false;
  }

  if (resp.headers().count("x-auth-token") < 1) {
    msg_ts("%s: TempAuth failed. Server did not respond with X-Auth-Token\n",
           my_progname);
    return false;
  }

  auth_info.token = resp.headers().at("x-auth-token");
  auth_info.url = resp.headers().at("x-storage-url");

  return true;
}

bool Keystone_client::auth_v2(const std::string &swift_region,
                              auth_info_t &auth_info) {
  /*
    {
        "auth": {
            "passwordCredentials": {
                "password": "my-password",
                "username": "my-username"
            },
            "tenantName": "project-x"
        }
    }
   */

  if (user.empty()) {
    msg_ts("%s: error: Swift user is required for keystone auth v2\n",
           my_progname);
    return false;
  }

  if (password.empty()) {
    msg_ts("%s: error: Swift password is required for keystone auth v2\n",
           my_progname);
    return false;
  }

  if (!tenant.empty() && !tenant_id.empty()) {
    msg_ts(
        "%s: error: Both tenant and tenant-id are specified for keystone auth "
        "v2\n",
        my_progname);
    return false;
  }

  rapidjson::StringBuffer s;
  rapidjson::Writer<rapidjson::StringBuffer> writer(s);

  writer.StartObject();
  writer.Key("auth");
  writer.StartObject();
  writer.Key("passwordCredentials");
  writer.StartObject();
  writer.Key("password");
  writer.String(password.c_str());
  writer.Key("username");
  writer.String(user.c_str());
  writer.EndObject();  // passwordCredentials
  if (!tenant.empty()) {
    writer.Key("tenantName");
    writer.String(tenant.c_str());
  } else if (!tenant_id.empty()) {
    writer.Key("tenantId");
    writer.String(tenant_id.c_str());
  }
  writer.EndObject();  // auth
  writer.EndObject();  // root

  Http_request req(Http_request::POST, protocol, host, path + "tokens");
  req.add_header("Content-Type", "application/json");
  req.add_header("Accept", "application/json");

  auto payload = s.GetString();
  req.append_payload(payload, s.GetSize());

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (!resp.ok()) {
    msg_ts("%s: Keystone auth v2 failed with code %ld\n", my_progname,
           resp.http_code());
    return false;
  }

  rapidjson::Document doc;
  std::string body_s(resp.body().begin(), resp.body().end());
  doc.Parse(body_s.c_str());

  if (!doc.IsObject()) {
    msg_ts(
        "%s: Keystone auth v2 failed. Response does not contain JSON object.\n",
        my_progname);
    return false;
  }

  std::string auth_url;
  auto &token = json_get_member(
      json_get_member(json_get_member(doc, "access"), "token"), "id");
  if (token.IsString()) {
    msg_ts("%s: Keystone auth v2 failed. Access token is not string.\n",
           my_progname);
    return false;
  }

  auto &service_catalog =
      json_get_member(json_get_member(doc, "access"), "serviceCatalog");
  if (service_catalog.IsArray()) {
    msg_ts("%s: Keystone auth v2 failed. ServiceCatalog is not an array.\n",
           my_progname);
    return false;
  }

  for (auto sci = service_catalog.Begin(); sci != service_catalog.End();
       ++sci) {
    if (!sci->IsObject()) {
      msg_ts(
          "%s: Keystone auth v2 failed. ServiceCatalog item is not an "
          "object.\n",
          my_progname);
      return false;
    }
    auto sc_type = sci->FindMember("type");
    if (sc_type == sci->MemberEnd()) {
      msg_ts(
          "%s: Keystone auth v2 failed. ServiceCatalog item does not contain "
          "type.\n",
          my_progname);
      return false;
    }
    if (!sc_type->value.IsString()) {
      msg_ts(
          "%s: Keystone auth v2 failed. ServiceCatalog type is not a string.\n",
          my_progname);
      return false;
    }

    if (strcmp(sc_type->value.GetString(), "object-store") != 0) {
      continue;
    }

    auto endpoints = sci->FindMember("endpoints");
    if (endpoints == sci->MemberEnd()) {
      msg_ts(
          "%s: Keystone auth v2 failed. ServiceCatalog item does not contain "
          "endpoints.\n",
          my_progname);
      return false;
    }
    if (!endpoints->value.IsArray()) {
      msg_ts("%s: Keystone auth v2 failed. 'endpoints' member is not array.\n",
             my_progname);
      return false;
    }
    for (auto endpoint_i = endpoints->value.Begin();
         endpoint_i != endpoints->value.End(); ++endpoint_i) {
      if (!endpoint_i->IsObject()) {
        msg_ts("%s: Keystone auth v2 failed. Endpoint is not an object.\n",
               my_progname);
        return false;
      }
      auto region = endpoint_i->FindMember("region");
      if (region == endpoint_i->MemberEnd()) {
        msg_ts(
            "%s: Keystone auth v2 failed. Endpoint item does not contain "
            "region.\n",
            my_progname);
        return false;
      }
      if (!region->value.IsString()) {
        msg_ts("%s: Keystone auth v2 failed. Region is not a string.\n",
               my_progname);
        return false;
      }
      if (!swift_region.empty() &&
          strcmp(region->value.GetString(), swift_region.c_str()) != 0) {
        continue;
      }
      auto url = endpoint_i->FindMember("publicURL");
      if (url == endpoint_i->MemberEnd()) {
        msg_ts(
            "%s: Keystone auth v2 failed. Endpoint item does not contain "
            "publicURL.\n",
            my_progname);
        return false;
      }
      if (!url->value.IsString()) {
        msg_ts("%s: Keystone auth v2 failed. PublicURL is not a string.\n",
               my_progname);
        return false;
      }
      auth_url = url->value.GetString();
    }
  }

  if (auth_url.empty()) {
    msg_ts(
        "%s: Keystone auth v2 failed. Cannot find endpoint URL for region "
        "%s.\n",
        my_progname, swift_region.c_str());
    return false;
  }

  auth_info.token = token.GetString();
  auth_info.url = auth_url;

  return true;
}

bool Keystone_client::auth_v3(const std::string &swift_region,
                              auth_info_t &auth_info) {
  /*
    { "auth": {
        "identity": {
          "methods": ["password"],
          "password": {
            "user": {
              "name": "admin",
              "domain": { "id": "default" },
              "password": "adminpwd"
            }
          }
        },

        "scope": {
          "project": {
            "name": "demo",
            "domain": { "id": "default" }
          }
        }

      }
    }
  */

  if (user.empty()) {
    msg_ts("%s: error: Swift user is required for keystone auth v3\n",
           my_progname);
    return false;
  }

  if (password.empty()) {
    msg_ts("%s: error: Swift password is required for keystone auth v3\n",
           my_progname);
    return false;
  }

  if (!project.empty() && !project_id.empty()) {
    msg_ts(
        "%s: error: Both project and project-id are specified for keystone "
        "auth "
        "v3\n",
        my_progname);
    return false;
  }

  if (!domain.empty() && !domain_id.empty()) {
    msg_ts(
        "%s: error: Both domain and domain-id are specified for keystone auth "
        "v3\n",
        my_progname);
    return false;
  }

  rapidjson::StringBuffer s;
  rapidjson::Writer<rapidjson::StringBuffer> writer(s);

  auto write_domain = [](rapidjson::Writer<rapidjson::StringBuffer> &writer,
                         const std::string &domain,
                         const std::string &domain_id) {
    writer.Key("domain");
    writer.StartObject();
    if (!domain_id.empty()) {
      writer.Key("id");
      writer.String(domain_id.c_str());
    }
    if (!domain.empty()) {
      writer.Key("name");
      writer.String(domain.c_str());
    }
    writer.EndObject();  // domain
  };

  writer.StartObject();
  writer.Key("auth");
  writer.StartObject();
  writer.Key("identity");
  writer.StartObject();
  writer.Key("methods");
  writer.StartArray();
  writer.String("password");
  writer.EndArray();  // methods
  writer.Key("password");
  writer.StartObject();
  writer.Key("user");
  writer.StartObject();
  writer.Key("name");
  writer.String(user.c_str());
  if (!domain.empty() || !domain_id.empty()) {
    write_domain(writer, domain, domain_id);
  }
  writer.Key("password");
  writer.String(password.c_str());
  writer.EndObject();  // user
  writer.EndObject();  // password
  writer.EndObject();  // identity
  if (!project.empty() || !project_id.empty()) {
    writer.Key("scope");
    writer.StartObject();
    writer.Key("project");
    writer.StartObject();
    if (!project.empty()) {
      writer.Key("name");
      writer.String(project.c_str());
    }
    if (!project_id.empty()) {
      writer.Key("id");
      writer.String(project_id.c_str());
    }
    if (!project_domain.empty() || !project_domain_id.empty()) {
      write_domain(writer, project_domain, project_domain_id);
    }
    writer.EndObject();  // project
    writer.EndObject();  // scope
  }
  writer.EndObject();  // auth
  writer.EndObject();  // root

  Http_request req(Http_request::POST, protocol, host, path + "auth/tokens/");
  req.add_header("Content-Type", "application/json");
  req.add_header("Accept", "application/json");

  std::string payload = std::string(s.GetString());
  req.append_payload(payload);

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (!resp.ok()) {
    msg_ts("%s: Keystone auth v3 failed with code %ld\n", my_progname,
           resp.http_code());
    return false;
  }

  if (resp.headers().count("x-subject-token") < 1) {
    msg_ts(
        "%s: Keystone auth v3 failed. Response does not contain "
        "X-Subject-Token header.\n",
        my_progname);
    return false;
  }

  rapidjson::Document doc;
  std::string body_s(resp.body().begin(), resp.body().end());
  doc.Parse(body_s.c_str());

  if (!doc.IsObject()) {
    msg_ts(
        "%s: Keystone auth v3 failed. Response does not contain JSON object.\n",
        my_progname);
    return false;
  }

  std::string auth_url;

  auto &catalog = json_get_member(json_get_member(doc, "token"), "catalog");
  if (!catalog.IsArray()) {
    msg_ts("%s: Keystone auth v3 failed. Catalog is not an array.\n",
           my_progname);
    return false;
  }

  for (auto ci = catalog.Begin(); ci != catalog.End(); ++ci) {
    if (!ci->IsObject()) {
      msg_ts("%s: Keystone auth v3 failed. Catalog item is not an object.\n",
             my_progname);
      return false;
    }
    auto c_type = ci->FindMember("type");
    if (c_type == ci->MemberEnd()) {
      msg_ts(
          "%s: Keystone auth v3 failed. Catalog item does not contain type.\n",
          my_progname);
      return false;
    }
    if (!c_type->value.IsString()) {
      msg_ts("%s: Keystone auth v3 failed. Catalog type is not a string.\n",
             my_progname);
      return false;
    }

    if (strcmp(c_type->value.GetString(), "object-store") != 0) {
      continue;
    }

    auto endpoints = ci->FindMember("endpoints");
    if (endpoints == ci->MemberEnd()) {
      msg_ts(
          "%s: Keystone auth v3 failed. Catalog item does not contain "
          "endpoints.\n",
          my_progname);
      return false;
    }
    if (!endpoints->value.IsArray()) {
      msg_ts("%s: Keystone auth v3 failed. 'endpoints' member is not array.\n",
             my_progname);
      return false;
    }
    for (auto endpoint_i = endpoints->value.Begin();
         endpoint_i != endpoints->value.End(); ++endpoint_i) {
      if (!endpoint_i->IsObject()) {
        msg_ts("%s: Keystone auth v3 failed. Endpoint is not an object.\n",
               my_progname);
        return false;
      }
      auto region = endpoint_i->FindMember("region");
      if (region == endpoint_i->MemberEnd()) {
        msg_ts(
            "%s: Keystone auth v3 failed. Endpoint item does not contain "
            "region.\n",
            my_progname);
        return false;
      }
      if (!region->value.IsString()) {
        msg_ts("%s: Keystone auth v3 failed. Region is not a string.\n",
               my_progname);
        return false;
      }
      if (!swift_region.empty() &&
          strcmp(region->value.GetString(), swift_region.c_str()) != 0) {
        continue;
      }
      auto interface = endpoint_i->FindMember("interface");
      if (interface == endpoint_i->MemberEnd()) {
        msg_ts(
            "%s: Keystone auth v3 failed. Endpoint item does not contain "
            "interface.\n",
            my_progname);
        return false;
      }
      if (!interface->value.IsString()) {
        msg_ts("%s: Keystone auth v3 failed. interface is not a string.\n",
               my_progname);
        return false;
      }
      if (strcmp(interface->value.GetString(), "public") != 0) {
        continue;
      }
      auto url = endpoint_i->FindMember("url");
      if (url == endpoint_i->MemberEnd()) {
        msg_ts(
            "%s: Keystone auth v3 failed. Endpoint item does not contain "
            "url.\n",
            my_progname);
        return false;
      }
      if (!url->value.IsString()) {
        msg_ts("%s: Keystone auth v3 failed. url is not a string.\n",
               my_progname);
        return false;
      }
      auth_url = url->value.GetString();
    }
  }

  if (auth_url.empty()) {
    msg_ts(
        "%s: Keystone auth v3 failed. Cannot find endpoint URL for region "
        "%s.\n",
        my_progname, swift_region.c_str());
    return false;
  }

  auth_info.token = resp.headers().at("x-subject-token");
  auth_info.url = auth_url;

  return true;
}

bool Swift_client::validate_response(const Http_request &req,
                                     const Http_response &resp) {
  const auto etag = resp.headers().find("etag");
  if (etag != resp.headers().end()) {
    std::string expected_etag = hex_encode(req.payload().md5());
    if (etag->second != expected_etag) {
      msg_ts("%s: Etag mismatch. Expected %s, got %s\n", my_progname,
             expected_etag.c_str(), etag->second.c_str());
      return false;
    }
  }
  return true;
}

bool Swift_client::delete_object(const std::string &container,
                                 const std::string &name) {
  Http_request req(Http_request::DELETE, protocol, host,
                   path + container + "/" + name);
  req.add_header("X-Auth-Token", token);

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    return true;
  }

  msg_ts("%s: Failed to delete object. Http code: %ld\n", my_progname,
         resp.http_code());

  return false;
}

bool Swift_client::async_delete_object(const std::string &container,
                                       const std::string &name,
                                       Event_handler *h,
                                       const async_delete_callback_t callback) {
  Http_request *req = new Http_request(Http_request::DELETE, protocol, host,
                                       path + container + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to delete object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    return false;
  }
  req->add_header("X-Auth-Token", token);

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
      msg_ts("%s: Failed to delete object. Http error code: %lu\n", my_progname,
             resp->http_code());
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

Http_buffer Swift_client::download_object(const std::string &container,
                                          const std::string &name,
                                          bool &success) {
  Http_request req(Http_request::GET, protocol, host,
                   path + container + "/" + name);
  req.add_header("X-Auth-Token", token);

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

bool Swift_client::create_container(const std::string &name) {
  Http_request req(Http_request::PUT, protocol, host, path + name);
  req.add_header("X-Auth-Token", token);

  Http_response resp;
  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    return true;
  }

  msg_ts("%s: Failed to create container. Http code: %ld\n", my_progname,
         resp.http_code());

  return true;
}

bool Swift_client::container_exists(const std::string &name, bool &exists) {
  Http_request req(Http_request::GET, protocol, host, path + name);
  req.add_header("Content-Type", "application/json");
  req.add_header("X-Auth-Token", token);

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

  msg_ts("%s: Failed to list container. Http error code: %lu\n", my_progname,
         resp.http_code());

  return false;
}

bool Swift_client::upload_object(const std::string &container,
                                 const std::string &name,
                                 const Http_buffer &contents) {
  Http_request req(Http_request::PUT, protocol, host,
                   path + container + "/" + name);
  req.append_payload(contents);
  req.add_header("Content-Type", "application/octet-stream");
  req.add_header("X-Auth-Token", token);
  req.add_header("ETag", hex_encode(req.payload().md5()));

  Http_response resp;

  if (!http_client->make_request(req, resp)) {
    return false;
  }

  if (resp.ok()) {
    if (!validate_response(req, resp)) {
      msg_ts("%s: Failed to upload object.\n", my_progname);
      return false;
    }
    return true;
  }

  msg_ts("%s: Failed to upload object. Http error code: %lu\n", my_progname,
         resp.http_code());

  return false;
}

bool Swift_client::async_upload_object(const std::string &container,
                                       const std::string &name,
                                       const Http_buffer &contents,
                                       Event_handler *h,
                                       async_upload_callback_t callback) {
  Http_request *req = new Http_request(Http_request::PUT, protocol, host,
                                       path + container + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to upload object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    return false;
  }
  req->append_payload(contents);
  req->add_header("Content-Type", "application/octet-stream");
  req->add_header("X-Auth-Token", token);
  req->add_header("ETag", hex_encode(req->payload().md5()));
  req->add_param("format", "json");

  Http_response *resp = new Http_response();
  if (resp == nullptr) {
    msg_ts("%s: Failed to upload object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    delete req;
    return false;
  }

  auto f = [callback, container, name, req, resp](
               CURLcode rc, const Http_connection *conn) mutable -> void {
    if (rc == CURLE_OK && !resp->ok()) {
      msg_ts("%s: Failed to upload object. Http error code: %lu\n", my_progname,
             resp->http_code());
    }
    bool valid = rc == CURLE_OK && validate_response(*req, *resp);
    if (callback) {
      callback(rc == CURLE_OK && valid && resp->ok());
    }
    delete req;
    delete resp;
  };

  http_client->make_async_request(*req, *resp, h, f);

  return true;
}

bool Swift_client::async_download_object(const std::string &container,
                                         const std::string &name,
                                         Event_handler *h,
                                         async_download_callback_t callback) {
  Http_request *req = new Http_request(Http_request::GET, protocol, host,
                                       path + container + "/" + name);
  if (req == nullptr) {
    msg_ts("%s: Failed to download object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    return false;
  }
  req->add_header("X-Auth-Token", token);

  Http_response *resp = new Http_response();
  if (resp == nullptr) {
    msg_ts("%s: Failed to download object %s/%s. Out of memory.\n", my_progname,
           container.c_str(), name.c_str());
    delete req;
    return false;
  }

  auto f = [callback, container, name, req, resp](
               CURLcode rc, const Http_connection *conn) mutable -> void {
    if (rc == CURLE_OK && !resp->ok()) {
      msg_ts("%s: Failed to download object. Http error code: %lu\n",
             my_progname, resp->http_code());
    }
    if (callback) {
      callback(rc == CURLE_OK && resp->ok(), resp->body());
    }
    delete req;
    delete resp;
  };

  http_client->make_async_request(*req, *resp, h, f);

  return true;
}

bool Swift_client::list_objects_with_prefix(const std::string &container,
                                            const std::string &prefix,
                                            std::vector<std::string> &objects) {
  while (true) {
    Http_request req(Http_request::GET, protocol, host, path + container);
    req.add_header("Content-Type", "application/json");
    req.add_header("X-Auth-Token", token);
    req.add_param("format", "json");
    req.add_param("limit", "10");
    req.add_param("prefix", prefix);

    if (!objects.empty()) {
      req.add_param("marker", objects.back());
    }

    Http_response resp;

    if (!http_client->make_request(req, resp)) {
      return false;
    }

    if (!resp.ok()) {
      msg_ts("%s: Failed to list container. Http error code: %lu\n",
             my_progname, resp.http_code());
      return false;
    }

    rapidjson::Document doc;
    std::string body_s(resp.body().begin(), resp.body().end());
    doc.Parse(body_s.c_str());

    if (!doc.IsArray()) {
      msg_ts(
          "%s: Failed to list container. Response does not contain JSON "
          "array.\n",
          my_progname);
      return false;
    }

    int count = 0;
    for (auto obj_i = doc.Begin(); obj_i != doc.End(); ++obj_i) {
      auto name = obj_i->FindMember("name");
      if (name == obj_i->MemberEnd()) {
        msg_ts("%s: Failed to list container. Object does not contain name.\n",
               my_progname);
        return false;
      }
      if (!name->value.IsString()) {
        msg_ts("%s: Failed to list container. Object name is not string.\n",
               my_progname);
        return false;
      }
      objects.push_back(name->value.GetString());
      ++count;
    }
    if (count == 0) {
      break;
    }
  }

  return true;
}

}  // namespace xbcloud
