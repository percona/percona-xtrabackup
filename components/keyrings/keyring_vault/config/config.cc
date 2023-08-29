/* Copyright (c) 2023, Oracle and/or its affiliates.

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

#include <components/keyrings/keyring_vault/keyring_vault.h>

#include <components/keyrings/common/config/config_reader.h> /* Config_reader */
#include <include/mysql/components/component_implementation.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

#include <memory>
#include <string_view>

using keyring_common::config::Config_reader;
using keyring_vault::g_config_pod;

/**
  In order to locate a shared library, we need it to export at least
  one symbol. This way dlsym/GetProcAddress will be able to find it.
*/
DLL_EXPORT int keyring_vault_component_exported_symbol() { return 0; }

namespace keyring_vault {
namespace config {

char *g_component_path = nullptr;
char *g_instance_path = nullptr;

/* Component metadata */
static const char *s_component_metadata[][2] = {
    {"Component_name", "component_keyring_vault"},
    {"Author", "Percona Corporation"},
    {"License", "GPL"},
    {"Implementation_name", "component_keyring_vault"},
    {"Version", "1.0"}};

/* Config names */
static const std::string config_options[] = {
    "read_local_config",         "timeout",  "vault_url",
    "secret_mount_point",        "vault_ca", "token",
    "secret_mount_point_version"};

namespace {

constexpr std::string_view config_file_name{"component_keyring_vault.cnf"};
constexpr uint timeout_max_val = 86400;
constexpr uint timeout_default_val = 15;

constexpr std::string_view mount_point_version_auto{"AUTO"};
constexpr std::string_view http_protocol_prefix{"http://"};
constexpr std::string_view https_protocol_prefix{"https://"};
constexpr char mount_point_path_delimiter = '/';

bool check_config_valid(Config_pod *config_pod) {
  std::ostringstream err_ss;

  // "timeout": uint
  if (config_pod->timeout > timeout_max_val) {
    err_ss << config_options[1] << " max allowed value is " << timeout_max_val
           << ".";
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                    err_ss.str().c_str());
    return false;
  }

  // "vault_url": string
  bool vault_url_is_https = false;
  bool vault_url_is_http =
      boost::starts_with(config_pod->vault_url, http_protocol_prefix);
  if (!vault_url_is_http)
    vault_url_is_https =
        boost::starts_with(config_pod->vault_url, https_protocol_prefix);
  if (!vault_url_is_http && !vault_url_is_https) {
    err_ss << config_options[2] << " must be either " << http_protocol_prefix
           << " or " << https_protocol_prefix << " URL.";
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                    err_ss.str().c_str());
    return false;
  }

  // "secret_mount_point": string
  if (config_pod->secret_mount_point[0] == mount_point_path_delimiter) {
    err_ss << config_options[3] << " must not start with "
           << mount_point_path_delimiter << ".";
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                    err_ss.str().c_str());
    return false;
  }
  if (config_pod->secret_mount_point[config_pod->secret_mount_point.size() -
                                     1] == mount_point_path_delimiter) {
    err_ss << config_options[4] << " must not end with "
           << mount_point_path_delimiter << ".";
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                    err_ss.str().c_str());
    return false;
  }

  // checks for combination op options
  if (!config_pod->vault_ca.empty() && vault_url_is_http) {
    err_ss << config_options[4] << " is specified but " << config_options[2]
           << " is " << http_protocol_prefix << ".";
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                    err_ss.str().c_str());
    return false;
  }
  if (config_pod->vault_ca.empty() && vault_url_is_https) {
    err_ss << config_options[4] << " is not specified but " << config_options[2]
           << " is " << https_protocol_prefix << ". "
           << "Please make sure that Vault's CA certificate is trusted by "
              "the machine from "
           << "which you intend to connect to Vault.";

    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                    err_ss.str().c_str());
  }

  return true;
}

bool set_config_path(std::string &full_path) {
  if (full_path.length() == 0) return true;
#ifdef _WIN32
  full_path += "\\";
#else
  full_path += "/";
#endif
  full_path.append(config_file_name);
  return false;
}

}  // namespace

bool get_global_config_path(std::string &full_path) {
  full_path = g_component_path;
  return set_config_path(full_path);
}

bool get_local_config_path(std::string &full_path) {
  full_path = g_instance_path;
  return set_config_path(full_path);
}

bool find_and_read_config_file(std::unique_ptr<Config_pod> &config_pod) {
  auto config_pod_tmp = std::make_unique<Config_pod>();
  std::string global_path;

  if (get_global_config_path(global_path)) return true;

  /* Read config file that's located at shared library location */
  auto config_reader = std::make_unique<Config_reader>(global_path);

  {
    bool read_local_config = false;
    if (!config_reader->get_element<bool>(config_options[0],
                                          read_local_config)) {
      if (read_local_config) {
        config_reader.reset();
        /*
          Read config file from current working directory
          We assume that when control reaches here, binary has set
          current working directory appropriately.
        */
        std::string instance_path;
        if (get_local_config_path(instance_path)) {
          instance_path = config_file_name;
        }
        config_reader = std::make_unique<Config_reader>(instance_path);
      }
    }
  }

  std::stringstream err_ss;
  std::string fetched_conf_value;

  if (config_reader->get_element<uint>(config_options[1],
                                       config_pod_tmp->timeout)) {
    config_pod_tmp->timeout = timeout_default_val;
  }

  if (config_reader->is_string(config_options[2]) ||
      config_reader->get_element<std::string>(config_options[2],
                                              fetched_conf_value)) {
    return true;
  }

  boost::algorithm::trim(fetched_conf_value);
  config_pod_tmp->vault_url = {fetched_conf_value.c_str(),
                               fetched_conf_value.length()};

  if (config_reader->is_string(config_options[3]) ||
      config_reader->get_element<std::string>(config_options[3],
                                              fetched_conf_value)) {
    return true;
  }

  boost::algorithm::trim(fetched_conf_value);
  config_pod_tmp->secret_mount_point = {fetched_conf_value.c_str(),
                                        fetched_conf_value.length()};

  if (!config_reader->has_element(config_options[4])) {
    // Not mandatory field
    if (config_reader->is_string(config_options[4])) {
      return true;
    }

    if (!config_reader->get_element<std::string>(config_options[4],
                                                 fetched_conf_value)) {
      boost::algorithm::trim(fetched_conf_value);
      config_pod_tmp->vault_ca = {fetched_conf_value.c_str(),
                                  fetched_conf_value.length()};
    }
  }

  if (config_reader->is_string(config_options[5]) ||
      config_reader->get_element<std::string>(config_options[5],
                                              fetched_conf_value)) {
    return true;
  }

  boost::algorithm::trim(fetched_conf_value);
  config_pod_tmp->token = {fetched_conf_value.c_str(),
                           fetched_conf_value.length()};

  // by default, when no "secret_mount_point_version" is specified explicitly,
  // it is considered to be AUTO
  config_pod_tmp->secret_mount_point_version = Vault_version_auto;
  std::string mount_point_version_raw;

  if (!config_reader->is_string(config_options[6]) &&
      !config_reader->get_element<std::string>(config_options[6],
                                               mount_point_version_raw)) {
    boost::algorithm::trim(mount_point_version_raw);

    if (mount_point_version_raw != mount_point_version_auto) {
      boost::uint32_t extracted_version = 0;

      if (boost::conversion::try_lexical_convert(mount_point_version_raw,
                                                 extracted_version)) {
        switch (extracted_version) {
          case 1:
            config_pod_tmp->secret_mount_point_version = Vault_version_v1;
            break;
          case 2:
            config_pod_tmp->secret_mount_point_version = Vault_version_v2;
            break;
          default: {
            err_ss << config_options[6]
                   << " in the configuration file must be either 1 or 2.";
            LogComponentErr(ERROR_LEVEL,
                            ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                            err_ss.str().c_str());
            return true;
          }
        }
      } else {
        err_ss << config_options[6]
               << " in the configuration file is neither AUTO nor a numeric "
                  "value.";
        LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                        err_ss.str().c_str());
        return true;
      }
    }
  }

  if (!check_config_valid(config_pod_tmp.get())) {
    return true;
  }

  config_pod.swap(config_pod_tmp);
  return false;
}

bool create_config(
    std::unique_ptr<std::vector<std::pair<std::string, std::string>>>
        &metadata) {
  metadata =
      std::make_unique<std::vector<std::pair<std::string, std::string>>>();
  keyring_vault::config::Config_pod config_pod;
  bool global_config_available = false;
  if (g_config_pod != nullptr) {
    config_pod = *g_config_pod;
    global_config_available = true;
  }

  for (auto *entry : keyring_vault::config::s_component_metadata) {
    metadata->push_back(std::make_pair(entry[0], entry[1]));
  }

  metadata->push_back(
      std::make_pair("Component_status",
                     keyring_vault::g_component_callbacks->keyring_initialized()
                         ? "Active"
                         : "Disabled"));

  metadata->push_back(std::make_pair(
      "timeout", (global_config_available ? std::to_string(config_pod.timeout)
                                          : "<NOT APPLICABLE>")));

  metadata->push_back(std::make_pair(
      "vault_url", ((global_config_available)
                        ? ((config_pod.vault_url.length() == 0)
                               ? "<NONE>"
                               : std::string{config_pod.vault_url.c_str(),
                                             config_pod.vault_url.length()})
                        : "<NOT APPLICABLE>")));

  metadata->push_back(std::make_pair(
      "secret_mount_point",
      ((global_config_available)
           ? ((config_pod.secret_mount_point.length() == 0)
                  ? "<NONE>"
                  : std::string{config_pod.secret_mount_point.c_str(),
                                config_pod.secret_mount_point.length()})
           : "<NOT APPLICABLE>")));

  metadata->push_back(std::make_pair(
      "vault_ca", ((global_config_available)
                       ? ((config_pod.vault_ca.length() == 0)
                              ? "<NONE>"
                              : std::string{config_pod.vault_ca.c_str(),
                                            config_pod.vault_ca.length()})
                       : "<NOT APPLICABLE>")));

  std::string mount_point_version_str{"<NOT APPLICABLE>"};

  if (global_config_available) {
    switch (config_pod.secret_mount_point_version) {
      case Vault_version_auto:
        mount_point_version_str = mount_point_version_auto;
        break;
      case Vault_version_v1:
        mount_point_version_str = "1";
        break;
      case Vault_version_v2:
        mount_point_version_str = "2";
        break;
      case Vault_version_unknown:
      default:
        mount_point_version_str = "<NONE>";
        break;
    }
  }

  metadata->push_back(
      std::make_pair("secret_mount_point_version", mount_point_version_str));

  return false;
}

}  // namespace config
}  // namespace keyring_vault
