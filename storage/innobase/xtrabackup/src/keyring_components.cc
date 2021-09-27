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

#include <components/keyrings/keyring_file/config/config.h>
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

std::unique_ptr<keyring_file::config::Config_pod> new_config_pod;

SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(keyring_reader_with_status) *keyring_reader_service = nullptr;
bool service_handler_initialized = false;
bool keyring_component_initialized = false;
std::string component_config_data;

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
    msg("xtrabackup: mysql_plugin_registry_acquire failed\n");
    return false;
  }

  /* Add module specific initialization here */
  if (innobase::encryption::init_keyring_services(reg_srv) == false) {
    msg("xtrabackup: init_keyring_services failed\n");
    cleanup();
    return false;
  }
  msg("xtrabackup: inititialize_service_handles suceeded\n");
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
  return backup_file_print(XTRABACKUP_KEYRING_FILE_CONFIG,
                           component_config_data.c_str(),
                           component_config_data.length());
}

void create_component_config_data() {
  rapidjson::Document json;
  json.SetObject();
  rapidjson::Document::AllocatorType &allocator = json.GetAllocator();
  rapidjson::Value config_value(rapidjson::kObjectType);
  config_value.SetString(new_config_pod->config_file_path_.c_str(),
                         static_cast<rapidjson::SizeType>(
                             new_config_pod->config_file_path_.length()),
                         allocator);
  json.AddMember("path", config_value, allocator);
  json.AddMember("read_only", new_config_pod->read_only_, allocator);
  rapidjson::StringBuffer string_buffer;
  string_buffer.Clear();
  rapidjson::Writer<rapidjson::StringBuffer> string_writer(string_buffer);
  json.Accept(string_writer);
  component_config_data =
      std::string(string_buffer.GetString(), string_buffer.GetSize());
}


bool keyring_init_online(MYSQL *connection) {
  bool init_components = false;
  std::string component_urn = "file://";
  std::string component_name;
  new_config_pod = std::make_unique<keyring_file::config::Config_pod>();
  const char *query =
      "SELECT * FROM performance_schema.keyring_component_status";

  MYSQL_RES *mysql_result;
  MYSQL_ROW row;

  mysql_result = xb_mysql_query(connection, query, true);

  while ((row = mysql_fetch_row(mysql_result)) != NULL) {
    init_components = true;
    if (strcmp(row[0], "Component_name") == 0) {
      component_name = row[1];
      component_urn += row[1];
    } else if (strcmp(row[0], "Data_file") == 0)
      new_config_pod->config_file_path_ = row[1];
    else if (strcmp(row[0], "Read_only") == 0)
      new_config_pod->read_only_ = (strcmp(row[1], "No") == 0) ? false : true;
  }

  mysql_free_result(mysql_result);

  if (init_components) {
    create_component_config_data();
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

bool keyring_init_offline() {
  if (!xtrabackup::utils::read_server_uuid()) return (false);

  char fname[FN_REFLEN];
  if (opt_component_keyring_file_config != nullptr) {
    strncpy(fname, opt_component_keyring_file_config, FN_REFLEN);
  } else if (xtrabackup_stats) {
    if (fn_format(fname, XTRABACKUP_KEYRING_FILE_CONFIG, mysql_real_data_home,
                  "", MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
      return (false);
    }

  } else {
    if (xtrabackup_incremental_dir != nullptr) {
      if (fn_format(fname, XTRABACKUP_KEYRING_FILE_CONFIG,
                    xtrabackup_incremental_dir, "",
                    MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
        return (false);
      }
    } else {
      if (fn_format(fname, XTRABACKUP_KEYRING_FILE_CONFIG,
                    xtrabackup_real_target_dir, "",
                    MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
        return (false);
      }
    }
  }

  std::string config;
  File_reader component_config(fname, false, config);
  std::string component_name = "file://component_keyring_file";
  if (!component_config.valid()) {
    if (opt_component_keyring_file_config != nullptr) {
      msg("xtrabackup: Error: Component configuration file is not readable or "
          "not found.\n");
      return false;
    }
    /* XTRABACKUP_KEYRING_FILE_CONFIG not found. Attempt to init plugin. */
    return true;
  }
  if (config.length() == 0) {
    msg("xtrabackup: Error: Component configuration file is empty.\n");
    return false;
  }

  rapidjson::Document config_json;
  config_json.Parse(config);
  if (config_json.HasParseError()) {
    msg("xtrabackup: Error: Component configuration file is not a valid "
        "JSON.\n");
    return false;
  }
  if (!config_json.HasMember("path")) {
    msg("xtrabackup: Error: Component configuration does not have path "
        "member.\n");
    return false;
  }

  std::string path;
  if (opt_keyring_file_data != nullptr) {
    path = opt_keyring_file_data;
  } else {
    path = config_json["path"].GetString();
  }

  bool read_only;
  if (config_json.HasMember("read_only") && config_json["read_only"].IsBool()) {
    read_only = config_json["read_only"].GetBool();
  } else {
    read_only = false;
  }

  new_config_pod = std::make_unique<keyring_file::config::Config_pod>();
  new_config_pod->config_file_path_ = path;
  new_config_pod->read_only_ = read_only;
  if (initialize_manifest_file_components(component_name)) return false;
  set_srv_keyring_implementation_as_default();
  keyring_component_initialized = true;
  return true;
}

}  // namespace components
}  // namespace xtrabackup
