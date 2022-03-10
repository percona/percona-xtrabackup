/******************************************************
Copyright (c) 2021-2022 Percona LLC and/or its affiliates.

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
#include <dict0dict.h>
#include <mysqld.h>
#include <rapidjson/istreamwrapper.h>
#include <sql/server_component/mysql_server_keyring_lockable_imp.h>
#include <sql/sql_component.h>
#include "backup_copy.h"
#include "backup_mysql.h"
#include "common.h"
#include "my_rapidjson_size_t.h"
#include "rapidjson/document.h"
#include "utils.h"
#include "xtrabackup.h"

namespace xtrabackup {
namespace components {

const char *XTRABACKUP_KEYRING_FILE_CONFIG = "component_keyring_file.cnf";

const char *XTRABACKUP_KEYRING_KMIP_CONFIG = "component_keyring_kmip.cnf";

const char *XTRABACKUP_KEYRING_KMS_CONFIG = "component_keyring_kms.cnf";

SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(keyring_reader_with_status) *keyring_reader_service = nullptr;
bool service_handler_initialized = false;
bool keyring_component_initialized = false;
std::string component_name;
std::string component_config_path;

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

/** validate component fetched from keyring_component_status
to xtrabackup filename.
@return config file name or nullptr in case of not found */
const char *xb_component_config_file() {
  if (component_name == "component_keyring_file") {
    return XTRABACKUP_KEYRING_FILE_CONFIG;
  } else if (component_name == "component_keyring_kmip") {
    return XTRABACKUP_KEYRING_KMIP_CONFIG;
  } else if (component_name == "component_keyring_kms") {
    return XTRABACKUP_KEYRING_KMS_CONFIG;
  }
  return nullptr;
}

/** Check if component file exist on plugin dir or datadir setting
component_config_path to config file path.
@return true in case of file found. */
bool check_component_config_file() {
  os_file_type_t type;
  bool exists = false;
  std::string config_name = component_name + ".cnf";
  component_config_path = server_plugin_dir;
  component_config_path += OS_PATH_SEPARATOR + config_name;
  os_file_status(component_config_path.c_str(), &exists, &type);
  if (exists) {
    /* we need to read the file to validate if it is instructing to read local
     * configuration file inside datadir */
    rapidjson::Document data_;
    std::ifstream file_stream(component_config_path);
    if (!file_stream.is_open()) return false;

    rapidjson::IStreamWrapper json_fstream_reader(file_stream);
    data_.ParseStream(json_fstream_reader);
    if (data_.HasParseError()) {
      xb::error() << "error parsing json file " << component_config_path;
    }
    if (!data_.HasMember("read_local_config") ||
        (data_.HasMember("read_local_config") &&
         data_["read_local_config"].Get<bool>() == false)) {
      return true;
    }
  }

  component_config_path = mysql_real_data_home;
  component_config_path += OS_PATH_SEPARATOR + config_name;
  os_file_status(component_config_path.c_str(), &exists, &type);
  if (exists) return true;

  return false;
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
      if (!check_component_config_file()) {
        xb::error() << "Error reading " << component_name << " config file";
      }
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
  if (opt_component_keyring_config != nullptr) {
    char filepart[FN_REFLEN];
    /* extract file part of full path */
    if (fn_format(filepart, opt_component_keyring_config, "", "",
                  MY_REPLACE_DIR) == NULL) {
      return (false);
    }
    if (strcmp(filepart, component_config) == 0) {
      strncpy(fname, opt_component_keyring_config, FN_REFLEN);
      return (true);
    }
    return (false);
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
  std::string component_urn;
  os_file_type_t type;
  bool exists = false;
  /* keyring file */
  if (set_component_config_path(XTRABACKUP_KEYRING_FILE_CONFIG, fname)) {
    os_file_status(fname, &exists, &type);
    if (exists) {
      component_urn = "file://component_keyring_file";
      component_config_path = fname;
      goto init_components;
    }
  }

  /* keyring kmip */
  if (set_component_config_path(XTRABACKUP_KEYRING_KMIP_CONFIG, fname)) {
    os_file_status(fname, &exists, &type);
    if (exists) {
      component_urn = "file://component_keyring_kmip";
      component_config_path = fname;
      goto init_components;
    }
  }

  /* keyring kms */
  if (set_component_config_path(XTRABACKUP_KEYRING_KMS_CONFIG, fname)) {
    os_file_status(fname, &exists, &type);
    if (exists) {
      component_urn = "file://component_keyring_kms";
      component_config_path = fname;
      goto init_components;
    }
  }

  if (opt_component_keyring_config != nullptr) {
    /* user have set --component-keyring-config but it did not match any of know
     * componenets. Abort */
    xb::error() << "Unable to read " << opt_component_keyring_config
                << " passed as --component-keyring-config parameter";
    exit(EXIT_FAILURE);
  }
  /* no component to load */
  return true;

init_components:
  if (initialize_manifest_file_components(component_urn)) return false;
  set_srv_keyring_implementation_as_default();
  keyring_component_initialized = true;
  return true;
}

}  // namespace components
}  // namespace xtrabackup
