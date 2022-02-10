/******************************************************
Copyright (c) 2021 Percona LLC and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/
#include <components/keyrings/common/data_file/reader.h>
#include <components/keyrings/common/data_file/writer.h>
#include <components/keyrings/common/json_data/json_reader.h>
#include <components/keyrings/common/json_data/json_writer.h>

#include <dict0dict.h>
#include <mysqld.h>
#include <sql/server_component/mysql_server_keyring_lockable_imp.h>
#include <sql/sql_component.h>
#include "backup_copy.h"
#include "backup_mysql.h"
#include "common.h"
#include "utils.h"
#include "xtrabackup.h"

using keyring_common::data_file::File_reader;
using keyring_common::data_file::File_writer;

namespace xtrabackup {
namespace components {

const char *XTRABACKUP_KEYRING_FILE_CONFIG =
    "xtrabackup_component_keyring_file.cnf";

const char *XTRABACKUP_KEYRING_KMIP_CONFIG =
    "xtrabackup_component_keyring_kmip.cnf";

SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(keyring_reader_with_status) *keyring_reader_service = nullptr;
bool service_handler_initialized = false;
bool keyring_component_initialized = false;
rapidjson::StringBuffer component_config_data_sb;
std::string component_name;

bool inititialize_service_handles() {
  DBUG_TRACE;
  if (service_handler_initialized) return true;
  set_srv_keyring_implementation_as_default();
  auto cleanup = [&]() {
    /* Add module specific deinitialization here */
    innobase::encryption::deinit_keyring_services(reg_srv);
    mysql_plugin_registry_release(reg_srv);
  };

  reg_srv = mysql_plugin_registry_acquire();
  if (reg_srv == nullptr) {
    xb::error() << "mysql_plugin_registry_acquire failed";
    return false;
  }

  /* Add module specific initialization here */
  if (innobase::encryption::init_keyring_services(reg_srv) == false) {
    xb::error() << "init_keyring_services failed";
    cleanup();
    return false;
  }
  xb::info() << "inititialize_service_handles suceeded";
  service_handler_initialized = true;
  return true;
}

void deinitialize_service_handles() {
  DBUG_TRACE;

  innobase::encryption::deinit_keyring_services(reg_srv);
  if (reg_srv != nullptr) {
    mysql_plugin_registry_release(reg_srv);
  }
}
static bool initialize_manifest_file_components(std::string components) {
  g_deployed_components =
      new (std::nothrow) Deployed_components(my_progname, components);
  if (g_deployed_components == nullptr ||
      g_deployed_components->valid() == false) {
    /* Error would have been raised by Deployed_components constructor */
    g_deployed_components = nullptr;
    return true;
  }
  return false;
}

bool write_component_config_file() {
  if (component_name == "component_keyring_file") {
    return backup_file_print(XTRABACKUP_KEYRING_FILE_CONFIG,
                             component_config_data_sb.GetString(),
                             component_config_data_sb.GetSize());
  } else if (component_name == "component_keyring_kmip") {
    return backup_file_print(XTRABACKUP_KEYRING_KMIP_CONFIG,
                             component_config_data_sb.GetString(),
                             component_config_data_sb.GetSize());
  }
  return false;
}

void create_component_config_data(MYSQL_RES *mysql_result) {
  rapidjson::Document json;
  json.SetObject();
  rapidjson::Document::AllocatorType &allocator = json.GetAllocator();
  rapidjson::Value config_name(rapidjson::kObjectType);
  rapidjson::Value config_value(rapidjson::kObjectType);
  MYSQL_ROW row;
  unsigned long *lengths;
  while ((row = mysql_fetch_row(mysql_result)) != NULL) {
    lengths = mysql_fetch_lengths(mysql_result);
    if (component_name == "component_keyring_file") {
      if (strcmp(row[0], "data_file") == 0) {
        config_value.SetString(
            row[1], static_cast<rapidjson::SizeType>(lengths[1]), allocator);
        json.AddMember("path", config_value, allocator);
      } else if (strcmp(row[0], "read_only") == 0) {
        json.AddMember("read_only", (strcmp(row[1], "No") == 0) ? false : true,
                       allocator);
      }
    } else if (component_name == "component_keyring_kmip") {
      if (strcmp(row[0], "server_addr") == 0 ||
          strcmp(row[0], "server_port") == 0 ||
          strcmp(row[0], "client_ca") == 0 ||
          strcmp(row[0], "client_key") == 0 ||
          strcmp(row[0], "server_ca") == 0) {
        config_name.SetString(
            row[0], static_cast<rapidjson::SizeType>(lengths[0]), allocator);
        config_value.SetString(
            row[1], static_cast<rapidjson::SizeType>(lengths[1]), allocator);
        json.AddMember(config_name, config_value, allocator);
      } else if (strcmp(row[0], "object_group") == 0) {
        if (strcmp(row[1], "<NONE>") != 0) {
          config_name.SetString(
              row[0], static_cast<rapidjson::SizeType>(lengths[0]), allocator);
          config_value.SetString(
              row[1], static_cast<rapidjson::SizeType>(lengths[1]), allocator);
          json.AddMember(config_name, config_value, allocator);
        }
      }
    }
  }
  component_config_data_sb.Clear();
  rapidjson::Writer<rapidjson::StringBuffer> string_writer(
      component_config_data_sb);
  json.Accept(string_writer);
}

bool keyring_init_online(MYSQL *connection) {
  bool init_components = false;
  std::string component_urn = "file://";
  const char *query =
      "SELECT lower(STATUS_KEY), STATUS_VALUE FROM "
      "performance_schema.keyring_component_status";

  MYSQL_RES *mysql_result;
  MYSQL_ROW row;

  mysql_result = xb_mysql_query(connection, query, true);
  if (mysql_num_rows(mysql_result) > 0) {
    /* First row has component Name */
    row = mysql_fetch_row(mysql_result);
    if (strcmp(row[0], "component_name") == 0) {
      init_components = true;
      component_name = row[1];
      component_urn += row[1];
      create_component_config_data(mysql_result);
    }
  }

  mysql_free_result(mysql_result);

  if (init_components) {
    if (initialize_manifest_file_components(component_urn)) return false;
    /*
      If keyring component was loaded through manifest file, services provided
      by such a component should get priority over keyring plugin. That's why
      we have to set defaults before proxy keyring services are loaded.
    */
    set_srv_keyring_implementation_as_default();
    keyring_component_initialized = true;
  }
  return true;
}

bool set_component_config_path(const char *component_config, char *fname) {
  if (opt_component_keyring_file_config != nullptr) {
    strncpy(fname, opt_component_keyring_file_config, FN_REFLEN);
  } else if (xtrabackup_stats) {
    if (fn_format(fname, component_config, mysql_real_data_home, "",
                  MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
      return (false);
    }

  } else {
    if (xtrabackup_incremental_dir != nullptr) {
      if (fn_format(fname, component_config, xtrabackup_incremental_dir, "",
                    MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
        return (false);
      }
    } else {
      if (fn_format(fname, component_config, xtrabackup_real_target_dir, "",
                    MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
        return (false);
      }
    }
  }
  return (true);
}

bool keyring_init_offline() {
  if (!xtrabackup::utils::read_server_uuid()) return (false);

  char fname[FN_REFLEN];
  std::string config;
  std::string component_name;
  /* keyring file */
  set_component_config_path(XTRABACKUP_KEYRING_FILE_CONFIG, fname);
  File_reader component_file_config(fname, false, config);
  component_name = "file://component_keyring_file";
  if (!component_file_config.valid()) {
    if (opt_component_keyring_file_config != nullptr) {
      xb::error() << "Component configuration file is not readable or "
                     "not found.";
      return false;
    }

    /* keyring kmip */
    memset(fname, 0, FN_REFLEN);
    config = "";
    set_component_config_path(XTRABACKUP_KEYRING_KMIP_CONFIG, fname);
    File_reader component_kmip_config(fname, false, config);
    component_name = "file://component_keyring_kmip";
    if (!component_kmip_config.valid()) return true;
  }

  if (xtrabackup_stats) {
    xb::error() << "Encryption is not supported with --stats";
    return false;
  }
  if (config.length() == 0) {
    xb::error() << "Component configuration file is empty.";
    return false;
  }

  rapidjson::Document config_json;
  config_json.Parse(config);
  if (config_json.HasParseError()) {
    xb::error() << "Component configuration file is not a valid "
                << "JSON.";
    return false;
  }

  component_config_data_sb.Clear();
  rapidjson::Writer<rapidjson::StringBuffer> string_writer(
      component_config_data_sb);
  config_json.Accept(string_writer);

  if (initialize_manifest_file_components(component_name)) return false;
  set_srv_keyring_implementation_as_default();
  keyring_component_initialized = true;
  return true;
}

}  // namespace components
}  // namespace xtrabackup
