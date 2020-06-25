/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * http auth backend plugin.
 *
 * section name
 * :  http_auth_backend
 */

#include <future>
#include <mutex>
#include <thread>

#include <sys/types.h>

// Harness interface include files
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

#include "mysqlrouter/http_auth_backend_component.h"
#include "mysqlrouter/http_auth_backend_export.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/plugin_config.h"

#include "http_auth_backend.h"
#include "http_auth_backend_metadata_cache.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"http_auth_backend"};

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::Plugin;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::PluginFuncEnv;

namespace {
class HtpasswdPluginConfig : public mysqlrouter::BasePluginConfig {
 public:
  std::string filename;

  explicit HtpasswdPluginConfig(const mysql_harness::ConfigSection *section)
      : mysqlrouter::BasePluginConfig(section),
        filename(get_option_string(section, "filename")) {}

  std::string get_default(const std::string &option) const override {
    if (option == "filename") return "users";
    return std::string();
  }

  bool is_required(const std::string & /* option */) const override {
    return false;
  }
};

class HttpAuthBackendFactory {
 public:
  static std::shared_ptr<HttpAuthBackend> create(
      const std::string &name, const mysql_harness::ConfigSection *section) {
    if (name == "file") {
      auto s = std::make_shared<HttpAuthBackendHtpasswd>();

      HtpasswdPluginConfig config(section);

      if (auto ec = s->from_file(config.filename)) {
        throw std::runtime_error("parsing " + config.filename +
                                 " failed for section [" + section->name +
                                 "]: " + ec.message());
      }

      return s;
    } else if (name == "metadata_cache") {
      auto s = std::make_shared<HttpAuthBackendMetadataCache>();
      return s;
    } else {
      throw std::invalid_argument("unknown backend=" + name +
                                  " in section: " + section->name);
    }
  }
};

class PluginConfig : public mysqlrouter::BasePluginConfig {
 public:
  std::string backend;
  std::string filename;

  explicit PluginConfig(const mysql_harness::ConfigSection *section)
      : mysqlrouter::BasePluginConfig(section),
        backend(get_option_string(section, "backend")) {}

  std::string get_default(const std::string & /* option */) const override {
    return std::string();
  }

  bool is_required(const std::string &option) const override {
    if (option == "backend") return true;
    return false;
  }
};
}  // namespace

std::shared_ptr<HttpAuthBackendComponent::value_type> auth_backends;

static void init(PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  try {
    auth_backends = std::make_shared<HttpAuthBackendComponent::value_type>();
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) {
        continue;
      }

      if (section->key.empty()) {
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "The config section [%s] requires a name, like [%s:example]",
                  kSectionName, kSectionName);
        return;
      }

      PluginConfig config(section);

      auth_backends->insert({section->key, HttpAuthBackendFactory::create(
                                               config.backend, section)});
    }
    HttpAuthBackendComponent::get_instance().init(auth_backends);
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void start(PluginFuncEnv *env) {
  const mysql_harness::ConfigSection *section = get_config_section(env);

  PluginConfig config(section);
  if (config.backend == "metadata_cache") {
    auto *cache_api = metadata_cache::MetadataCacheAPI::instance();

    if (cache_api->is_initialized()) {
      // metada_cache is already running, we need to force update cache
      cache_api->enable_fetch_auth_metadata();
      cache_api->force_cache_update();
    } else {
      while (!cache_api->is_initialized()) {
        if (!(!env || is_running(env))) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      cache_api->enable_fetch_auth_metadata();
    }

    try {
      // verify that auth_cache timers are greater than the ttl and that
      // auth_cache_refresh_interval is smaller than auth_cache_ttl
      cache_api->check_auth_metadata_timers();
    } catch (const std::invalid_argument &e) {
      log_error("%s", e.what());
      set_error(env, mysql_harness::kConfigInvalidArgument, "%s", e.what());
      clear_running(env);
    }
  }
}

extern "C" {
Plugin HTTP_AUTH_BACKEND_EXPORT harness_plugin_http_auth_backend = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "HTTP_AUTH_BACKEND",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,  // requires
    0,
    nullptr,  // conflicts
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
};
}
