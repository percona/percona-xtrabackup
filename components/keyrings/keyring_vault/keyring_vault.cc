/* Copyright (c) 2023, Percona and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include "keyring_vault.h"

#include "backend/vault_curl.h"

/* Keyring_encryption_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_encryption_service_definition.h>
/* Keyring_generator_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_generator_service_definition.h>
/* Keyring_load_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_load_service_definition.h>
/* Keyring_keys_metadata_iterator_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_keys_metadata_iterator_service_definition.h>
/* Log_builtins_keyring */
#include <components/keyrings/common/component_helpers/include/keyring_log_builtins_definition.h>
/* Keyring_metadata_query_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_metadata_query_service_definition.h>
/* Keyring_reader_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_reader_service_definition.h>
/* Keyring_writer_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_writer_service_definition.h>

#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/psi_memory.h>

#include <cstring>
#include <iostream>
#include <memory>

using keyring_common::operations::Keyring_operations;
using keyring_vault::backend::Keyring_vault_backend;
using keyring_vault::backend::Keyring_vault_curl;
using keyring_vault::config::Config_pod;
using keyring_vault::config::g_component_path;
using keyring_vault::config::g_instance_path;

/** Dependencies */
REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);

SERVICE_TYPE(log_builtins) * log_bi;
SERVICE_TYPE(log_builtins_string) * log_bs;

namespace keyring_vault {
/** Keyring operations object */
std::unique_ptr<Keyring_operations<Keyring_vault_backend>> g_keyring_operations;

/** Keyring data source */
std::unique_ptr<Config_pod> g_config_pod;

/** Keyring state */
bool g_keyring_vault_inited = false;

/**
  Set path to component

  @param [in] component_path  Path to component library
  @param [in] instance_path   Path to instance specific config

  @returns initialization status
    @retval false Successful initialization
    @retval true  Error
*/
bool set_paths(const char *component_path, const char *instance_path) {
  char *save_c = g_component_path;
  char *save_i = g_instance_path;
  g_component_path = strdup(component_path != nullptr ? component_path : "");
  g_instance_path = strdup(instance_path != nullptr ? instance_path : "");

  if (g_component_path == nullptr || g_instance_path == nullptr) {
    g_component_path = save_c;
    g_instance_path = save_i;
    return true;
  }

  if (save_c != nullptr) free(save_c);
  if (save_i != nullptr) free(save_i);
  return false;
}

/**
  Intialize or re-initialize keyring.
  1. Read configuration vault
  2. Read keyring vault
  3. Initialize internal cache

  @returns Status of read operations
    @retval false Read config and data
    @retval true  Error reading config or data.
                  Existing data remains as it is.
*/
bool init_or_reinit_keyring() {
  /* Get config */
  std::unique_ptr<Config_pod> new_config_pod;
  if (keyring_vault::config::find_and_read_config_file(new_config_pod))
    return true;

  /* Initialize backend handler */
  auto new_curl = std::make_unique<Keyring_vault_curl>(new_config_pod.get());
  auto new_backend =
      std::make_unique<Keyring_vault_backend>(std::move(new_curl));
  if (new_backend == nullptr || new_backend->init() || !new_backend->valid())
    return true;

  /* Create new operations class */
  auto new_operations =
      std::make_unique<Keyring_operations<Keyring_vault_backend>>(
          true, new_backend.release());

  if (new_operations == nullptr || !new_operations->valid()) {
    return true;
  }

  g_keyring_operations.swap(new_operations);
  g_config_pod.swap(new_config_pod);

  return false;
}

/**
  Initialization function for component - Used when loading the component
*/
static mysql_service_status_t keyring_vault_init() {
  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;

  g_component_callbacks.reset(
      new keyring_common::service_implementation::Component_callbacks());

  return 0;
}

/**
  De-initialization function for component - Used when unloading the component
*/
static mysql_service_status_t keyring_vault_deinit() {
  g_keyring_vault_inited = false;
  if (g_component_path != nullptr) free(g_component_path);
  g_component_path = nullptr;
  if (g_instance_path != nullptr) free(g_instance_path);
  g_instance_path = nullptr;

  g_keyring_operations.reset();
  g_component_callbacks.reset();

  return 0;
}

}  // namespace keyring_vault

/** ======================================================================= */

/** Component declaration related stuff */

/** This component provides implementation of following component services */
KEYRING_AES_IMPLEMENTOR(component_keyring_vault);
KEYRING_GENERATOR_IMPLEMENTOR(component_keyring_vault);
KEYRING_LOAD_IMPLEMENTOR(component_keyring_vault);
KEYRING_KEYS_METADATA_FORWARD_ITERATOR_IMPLEMENTOR(component_keyring_vault);
KEYRING_COMPONENT_STATUS_IMPLEMENTOR(component_keyring_vault);
KEYRING_COMPONENT_METADATA_QUERY_IMPLEMENTOR(component_keyring_vault);
KEYRING_READER_IMPLEMENTOR(component_keyring_vault);
KEYRING_WRITER_IMPLEMENTOR(component_keyring_vault);
/* Used if log_builtins is not available */
KEYRING_LOG_BUILTINS_IMPLEMENTOR(component_keyring_vault);
KEYRING_LOG_BUILTINS_STRING_IMPLEMENTOR(component_keyring_vault);

/** Component provides */
BEGIN_COMPONENT_PROVIDES(component_keyring_vault)
PROVIDES_SERVICE(component_keyring_vault, keyring_aes),
    PROVIDES_SERVICE(component_keyring_vault, keyring_generator),
    PROVIDES_SERVICE(component_keyring_vault, keyring_load),
    PROVIDES_SERVICE(component_keyring_vault, keyring_keys_metadata_iterator),
    PROVIDES_SERVICE(component_keyring_vault, keyring_component_status),
    PROVIDES_SERVICE(component_keyring_vault, keyring_component_metadata_query),
    PROVIDES_SERVICE(component_keyring_vault, keyring_reader_with_status),
    PROVIDES_SERVICE(component_keyring_vault, keyring_writer),
    PROVIDES_SERVICE(component_keyring_vault, log_builtins),
    PROVIDES_SERVICE(component_keyring_vault, log_builtins_string),
    END_COMPONENT_PROVIDES();

REQUIRES_PSI_MEMORY_SERVICE_PLACEHOLDER;

/** List of dependencies */
BEGIN_COMPONENT_REQUIRES(component_keyring_vault)
REQUIRES_SERVICE(registry), REQUIRES_SERVICE(log_builtins),
    REQUIRES_SERVICE(log_builtins_string), REQUIRES_PSI_MEMORY_SERVICE,
    END_COMPONENT_REQUIRES();

/** Component description */
BEGIN_COMPONENT_METADATA(component_keyring_vault)
METADATA("mysql.author", "Percona Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("component_keyring_vault_service", "1"), END_COMPONENT_METADATA();

/** Component declaration */
DECLARE_COMPONENT(component_keyring_vault, "component_keyring_vault")
keyring_vault::keyring_vault_init,
    keyring_vault::keyring_vault_deinit END_DECLARE_COMPONENT();

/** Component contained in this library */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(component_keyring_vault)
    END_DECLARE_LIBRARY_COMPONENTS
