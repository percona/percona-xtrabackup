/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * REST API for the routing plugin.
 */

#include <array>
#include <string>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/section_config_exposer.h"
#include "mysql/harness/utility/string.h"  // ::join()

#include "mysqlrouter/component/http_server_component.h"
#include "mysqlrouter/http_constants.h"
#include "mysqlrouter/rest_api_component.h"
#include "mysqlrouter/rest_routing_export.h"
#include "mysqlrouter/supported_rest_options.h"

#include "rest_routing_blocked_hosts.h"
#include "rest_routing_config.h"
#include "rest_routing_connections.h"
#include "rest_routing_destinations.h"
#include "rest_routing_health.h"
#include "rest_routing_list.h"
#include "rest_routing_routes_status.h"
#include "rest_routing_status.h"
IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;

static const char kSectionName[]{"rest_routing"};
static const char kRequireRealm[]{"require_realm"};

// one shared setting
std::string require_realm_routing;

using StringOption = mysql_harness::StringOption;

#define GET_OPTION_CHECKED(option, section, name, value)                      \
  static_assert(                                                              \
      mysql_harness::str_in_collection(rest_plugin_supported_options, name)); \
  option = get_option(section, name, value);

class RestRoutingPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string require_realm;

  explicit RestRoutingPluginConfig(const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section) {
    GET_OPTION_CHECKED(require_realm, section, "require_realm", StringOption{});
  }

  std::string get_default(std::string_view /* option */) const override {
    return {};
  }

  bool is_required(std::string_view option) const override {
    if (option == kRequireRealm) return true;
    return false;
  }
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  try {
    std::set<std::string> known_realms;
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name == "http_auth_realm") {
        known_realms.emplace(section->key);
      }
    }
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) {
        continue;
      }

      if (!section->key.empty()) {
        log_error("[%s] section does not expect a key, found '%s'",
                  kSectionName, section->key.c_str());
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] section does not expect a key, found '%s'",
                  kSectionName, section->key.c_str());
        return;
      }

      RestRoutingPluginConfig config{section};

      if (!config.require_realm.empty() &&
          (known_realms.find(config.require_realm) == known_realms.end())) {
        std::string section_name = section->name;
        if (!section->key.empty()) section_name += ":" + section->key;

        const std::string realm_msg =
            (known_realms.empty())
                ? "No [http_auth_realm:" + config.require_realm +
                      "] section defined."
                : "Known [http_auth_realm:<...>] section" +
                      (known_realms.size() > 1 ? "s"s : ""s) + ": " +
                      mysql_harness::join(known_realms, ", ");

        throw std::invalid_argument(
            "The option 'require_realm=" + config.require_realm + "' in [" +
            section_name + "] does not match any http_auth_realm. " +
            realm_msg);
      }

      require_realm_routing = config.require_realm;
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

using JsonPointer = RestApiComponent::JsonPointer;
using JsonValue = RestApiComponent::JsonValue;

static JsonPointer::Token make_json_pointer_token(std::string_view token) {
  return {token.data(), token.size(), rapidjson::kPointerInvalidIndex};
}

static const std::array routing_status_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/routing/status"),
};

static const std::array routing_status_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RoutingGlobalStatus"),
};

static const std::array route_name_param_tokens{
    make_json_pointer_token("parameters"),
    make_json_pointer_token("routeNameParam"),
};

static const std::array routes_list_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteList"),
};

static const std::array routes_destination_list_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteDestinationList"),
};

static const std::array routes_destination_summary_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteDestinationSummary"),
};

static const std::array routes_blockedhost_list_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteBlockedHostList"),
};

static const std::array routes_blockedhost_summary_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteBlockedHostSummary"),
};

static const std::array routes_connection_list_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteConnectionsList"),
};

static const std::array routes_connection_summary_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteConnectionsSummary"),
};

static const std::array routes_summary_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteSummary"),
};

static const std::array routes_config_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteConfig"),
};

static const std::array routes_status_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteStatus"),
};

static const std::array routes_health_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouteHealth"),
};

static const std::array routes_status_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/routes/{routeName}/status"),
};

static const std::array routes_config_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/routes/{routeName}/config"),
};

static const std::array routes_health_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/routes/{routeName}/health"),
};

static const std::array routes_connection_list_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/routes/{routeName}/connections"),
};

static const std::array routes_blockedhost_list_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/routes/{routeName}/blockedHosts"),
};

static const std::array routes_destination_list_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/routes/{routeName}/destinations"),
};

static const std::array routes_list_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/routes"),
};

static const std::array tags_append_tokens{
    make_json_pointer_token("tags"),
    make_json_pointer_token("-"),
};

std::string json_pointer_stringfy(const JsonPointer &ptr) {
  rapidjson::StringBuffer sb;
  ptr.StringifyUriFragment(sb);
  return {sb.GetString(), sb.GetSize()};
}

static void spec_adder(RestApiComponent::JsonDocument &spec_doc) {
  auto &allocator = spec_doc.GetAllocator();

  {
    JsonPointer ptr(tags_append_tokens.data(), tags_append_tokens.size());

    ptr.Set(spec_doc,
            JsonValue(rapidjson::kObjectType)
                .AddMember("name", "routes", allocator)
                .AddMember("description", "Routes", allocator),
            allocator);
  }

  // /definitions/RoutingGlobalStatus
  const RestApiComponent::JsonPointer routing_status_def_ptr(
      routing_status_def_tokens.data(), routing_status_def_tokens.size());

  routing_status_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("totalMaxConnections",
                     "number of total connections allowed", allocator)
          .AddMember("currentMaxConnections",
                     "number of current total connections", allocator),
      allocator);

  std::string routing_status_def_ptr_str =
      json_pointer_stringfy(routing_status_def_ptr);

  // /parameters/routeNameParam
  const RestApiComponent::JsonPointer route_name_param_ptr(
      route_name_param_tokens.data(), route_name_param_tokens.size());

  route_name_param_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("name", "routeName", allocator)
          .AddMember("in", "path", allocator)
          .AddMember("description", "name of a route", allocator)
          .AddMember("required", true, allocator)
          .AddMember("type", "string", allocator),
      allocator);

  std::string route_name_param_str =
      json_pointer_stringfy(route_name_param_ptr);

  // /definitions/RoutesHealth
  const RestApiComponent::JsonPointer routes_health_def_ptr(
      routes_health_def_tokens.data(), routes_health_def_tokens.size());

  routes_health_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("isAlive",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "boolean", allocator),
                             allocator),
              allocator),
      allocator);

  std::string routes_health_def_ptr_str =
      json_pointer_stringfy(routes_health_def_ptr);

  // /definitions/RoutesStatus
  const RestApiComponent::JsonPointer routes_status_def_ptr(
      routes_status_def_tokens.data(), routes_status_def_tokens.size());

  routes_status_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("activeConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("totalConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("blockedHosts",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator),
              allocator),
      allocator);

  std::string routes_status_def_ptr_str =
      json_pointer_stringfy(routes_status_def_ptr);

  // /definitions/RoutesConfig
  const RestApiComponent::JsonPointer routes_config_def_ptr(
      routes_config_def_tokens.data(), routes_config_def_tokens.size());

  routes_config_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("bindAddress",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("bindPort",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("clientConnectTimeoutInMs",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("destinationConnectTimeoutInMs",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("maxActiveConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("maxConnectErrors",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("protocol",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  // these are stubbed now
                  //                  .AddMember("destinationClusterName",
                  //                             JsonValue(rapidjson::kObjectType)
                  //                                 .AddMember("type",
                  //                                 "string", allocator),
                  //                             allocator)
                  //                  .AddMember("destinationReplicasetName",
                  //                             JsonValue(rapidjson::kObjectType)
                  //                                 .AddMember("type",
                  //                                 "string", allocator),
                  //                             allocator)
                  .AddMember("socket",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("routingStrategy",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("mode",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator),
              allocator),
      allocator);

  std::string routes_config_def_ptr_str =
      json_pointer_stringfy(routes_config_def_ptr);

  // /definitions/RoutesSummary
  const RestApiComponent::JsonPointer routes_summary_def_ptr(
      routes_summary_def_tokens.data(), routes_summary_def_tokens.size());

  routes_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("name",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string routes_summary_def_ptr_str =
      json_pointer_stringfy(routes_summary_def_ptr);

  // /definitions/RoutesList
  const RestApiComponent::JsonPointer routes_list_def_ptr(
      routes_list_def_tokens.data(), routes_list_def_tokens.size());

  routes_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          routes_summary_def_ptr_str.data(),
                                          routes_summary_def_ptr_str.size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_list_def_ptr_str =
      json_pointer_stringfy(routes_list_def_ptr);

  // /definitions/RoutesDestinationSummary
  const RestApiComponent::JsonPointer routes_destination_summary_def_ptr(
      routes_destination_summary_def_tokens.data(),
      routes_destination_summary_def_tokens.size());

  routes_destination_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("name",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string routes_destination_summary_def_ptr_str =
      json_pointer_stringfy(routes_destination_summary_def_ptr);

  // /definitions/RoutesDestinationList
  const RestApiComponent::JsonPointer routes_destination_list_def_ptr(
      routes_destination_list_def_tokens.data(),
      routes_destination_list_def_tokens.size());

  routes_destination_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          routes_destination_summary_def_ptr_str
                                              .data(),
                                          routes_destination_summary_def_ptr_str
                                              .size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_destination_list_def_ptr_str =
      json_pointer_stringfy(routes_destination_list_def_ptr);

  // /definitions/RoutesBlockedHostSummary
  const RestApiComponent::JsonPointer routes_blockedhost_summary_def_ptr(
      routes_blockedhost_summary_def_tokens.data(),
      routes_blockedhost_summary_def_tokens.size());

  routes_blockedhost_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("name",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string routes_blockedhost_summary_def_ptr_str =
      json_pointer_stringfy(routes_blockedhost_summary_def_ptr);

  // /definitions/RoutesBlockedHostList
  const RestApiComponent::JsonPointer routes_blockedhost_list_def_ptr(
      routes_blockedhost_list_def_tokens.data(),
      routes_blockedhost_list_def_tokens.size());

  routes_blockedhost_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          routes_blockedhost_summary_def_ptr_str
                                              .data(),
                                          routes_blockedhost_summary_def_ptr_str
                                              .size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_blockedhost_list_def_ptr_str =
      json_pointer_stringfy(routes_blockedhost_list_def_ptr);

  // /definitions/RoutesConnectionSummary
  const RestApiComponent::JsonPointer routes_connection_summary_def_ptr(
      routes_connection_summary_def_tokens.data(),
      routes_connection_summary_def_tokens.size());

  routes_connection_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(RestRoutingConnections::kKeyTimeStarted,
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator)
                                 .AddMember("format", "date-time", allocator)
                                 .AddMember("description",
                                            "timepoint when connection to "
                                            "server was initiated",
                                            allocator),
                             allocator)
                  .AddMember(
                      RestRoutingConnections::kKeyTimeConnectedToServer,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "string", allocator)
                          .AddMember("format", "date-time", allocator)
                          .AddMember(
                              "description",
                              "timepoint when connection to server succeeded",
                              allocator),
                      allocator)
                  .AddMember(RestRoutingConnections::kKeyTimeLastSentToServer,
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator)
                                 .AddMember("format", "date-time", allocator)
                                 .AddMember("description",
                                            "timepoint when there was last "
                                            "data sent from client to server",
                                            allocator),
                             allocator)
                  .AddMember(
                      RestRoutingConnections::kKeyTimeLastReceivedFromServer,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "string", allocator)
                          .AddMember("format", "date-time", allocator)
                          .AddMember("description",
                                     "timepoint when there was last data sent "
                                     "from server to client",
                                     allocator),
                      allocator)
                  .AddMember(
                      RestRoutingConnections::kKeyBytesToServer,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "integer", allocator)
                          .AddMember("description", "bytes sent to destination",
                                     allocator),
                      allocator)
                  .AddMember(RestRoutingConnections::kKeyBytesFromServer,
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator)
                                 .AddMember("description",
                                            "bytes received from destination",
                                            allocator),
                             allocator)
                  .AddMember(
                      RestRoutingConnections::kKeyDestinationAddress,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "string", allocator)
                          .AddMember(
                              "description",
                              "address of the destination of the connection",
                              allocator),
                      allocator)
                  .AddMember(
                      RestRoutingConnections::kKeySourceAddress,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "string", allocator)
                          .AddMember("description",
                                     "address of the source of the connection",
                                     allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_connection_summary_def_ptr_str =
      json_pointer_stringfy(routes_connection_summary_def_ptr);

  // /definitions/RoutesConnectionList
  const RestApiComponent::JsonPointer routes_connection_list_def_ptr(
      routes_connection_list_def_tokens.data(),
      routes_connection_list_def_tokens.size());

  routes_connection_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          routes_connection_summary_def_ptr_str
                                              .data(),
                                          routes_connection_summary_def_ptr_str
                                              .size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_connection_list_def_ptr_str =
      json_pointer_stringfy(routes_connection_list_def_ptr);

  // /paths/routingStatus
  {
    JsonPointer ptr(routing_status_path_tokens.data(),
                    routing_status_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routing", allocator),
                               allocator)
                    .AddMember("description",
                               "Get status of the routing plugin", allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "status of the routing plugin",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routing_status_def_ptr_str
                                                        .data(),
                                                    routing_status_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/routesConfig
  {
    JsonPointer ptr(routes_config_path_tokens.data(),
                    routes_config_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get config of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "config of a route", allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_config_def_ptr_str
                                                        .data(),
                                                    routes_config_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/routesStatus
  {
    JsonPointer ptr(routes_status_path_tokens.data(),
                    routes_status_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get status of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "status of a route", allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_status_def_ptr_str
                                                        .data(),
                                                    routes_status_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/RoutesHealth
  {
    JsonPointer ptr(routes_health_path_tokens.data(),
                    routes_health_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get health of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "health of a route", allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_health_def_ptr_str
                                                        .data(),
                                                    routes_health_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/RoutesDestinations
  {
    JsonPointer ptr(routes_destination_list_path_tokens.data(),
                    routes_destination_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get destinations of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "destinations of a route",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_destination_list_def_ptr_str
                                                        .data(),
                                                    routes_destination_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/RoutesConnections
  {
    JsonPointer ptr(routes_connection_list_path_tokens.data(),
                    routes_connection_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get connections of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "connections of a route",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_connection_list_def_ptr_str
                                                        .data(),
                                                    routes_connection_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/RoutesBlockedHosts
  {
    JsonPointer ptr(routes_blockedhost_list_path_tokens.data(),
                    routes_blockedhost_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description",
                               "Get blocked host list for a route", allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "blocked host list for a route",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_blockedhost_list_def_ptr_str
                                                        .data(),
                                                    routes_blockedhost_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/routes/
  {
    JsonPointer ptr(routes_list_path_tokens.data(),
                    routes_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get list of the routes",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "list of the routes", allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_list_def_ptr_str
                                                        .data(),
                                                    routes_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator),
        allocator);
  }
}

static void start(mysql_harness::PluginFuncEnv *env) {
  auto &rest_api_srv = RestApiComponent::get_instance();

  const bool spec_adder_executed = rest_api_srv.try_process_spec(spec_adder);

  std::array paths{
      RestApiComponentPath{
          rest_api_srv, RestRoutingStatus::path_regex,
          std::make_unique<RestRoutingStatus>(require_realm_routing)},
      RestApiComponentPath{
          rest_api_srv, RestRoutingList::path_regex,
          std::make_unique<RestRoutingList>(require_realm_routing)},
      RestApiComponentPath{
          rest_api_srv, RestRoutingBlockedHosts::path_regex,
          std::make_unique<RestRoutingBlockedHosts>(require_realm_routing)},
      RestApiComponentPath{
          rest_api_srv, RestRoutingDestinations::path_regex,
          std::make_unique<RestRoutingDestinations>(require_realm_routing)},
      RestApiComponentPath{
          rest_api_srv, RestRoutingConfig::path_regex,
          std::make_unique<RestRoutingConfig>(require_realm_routing)},
      RestApiComponentPath{
          rest_api_srv, RestRoutingRoutesStatus::path_regex,
          std::make_unique<RestRoutingRoutesStatus>(require_realm_routing)},
      RestApiComponentPath{
          rest_api_srv, RestRoutingHealth::path_regex,
          std::make_unique<RestRoutingHealth>(require_realm_routing)},
      RestApiComponentPath{
          rest_api_srv, RestRoutingConnections::path_regex,
          std::make_unique<RestRoutingConnections>(require_realm_routing)},
  };

  mysql_harness::on_service_ready(env);

  wait_for_stop(env, 0);

  // in case rest_api never initialized, ensure the rest_api_component doesn't
  // have a callback to use
  if (!spec_adder_executed) rest_api_srv.remove_process_spec(spec_adder);
}

static constexpr std::array required{
    "logger",
    // "routing",
    "rest_api",
};

namespace {

class RestRoutingConfigExposer : public mysql_harness::SectionConfigExposer {
 public:
  using DC = mysql_harness::DynamicConfig;
  RestRoutingConfigExposer(const bool initial,
                           const RestRoutingPluginConfig &plugin_config,
                           const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(
            initial, default_section,
            DC::SectionId{"rest_configs", kSectionName}),
        plugin_config_(plugin_config) {}

  void expose() override {
    expose_option("require_realm", plugin_config_.require_realm,
                  std::string(kHttpDefaultAuthRealmName));
  }

 private:
  const RestRoutingPluginConfig &plugin_config_;
};

}  // namespace

static void expose_configuration(mysql_harness::PluginFuncEnv *env,
                                 const char * /*key*/, bool initial) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (!info->config) return;

  for (const mysql_harness::ConfigSection *section : info->config->sections()) {
    if (section->name == kSectionName) {
      RestRoutingPluginConfig config{section};
      RestRoutingConfigExposer(initial, config,
                               info->config->get_default_section())
          .expose();
    }
  }
}

extern "C" {
mysql_harness::Plugin REST_ROUTING_EXPORT harness_plugin_rest_routing = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "REST_ROUTING",                          // name
    VERSION_NUMBER(0, 0, 1),
    // requires
    required.size(),
    required.data(),
    // conflicts
    0,
    nullptr,
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
    true,     // declares_readiness
    rest_plugin_supported_options.size(),
    rest_plugin_supported_options.data(),
    expose_configuration,
};
}
