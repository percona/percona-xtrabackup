/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#include <cstring>
#include <memory>

#include "keyring_kmip.h"

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

#include <mysql/components/services/psi_memory.h>

/* clang-format off */
/**
  @page PAGE_COMPONENT_KEYRING_KMIP component_keyring_kmip

  This is keyring component services' implementation with kmip as backend to
  store data. This component implements following keyring services:

  - keyring_aes
  - keyring_generate
  - keyring_keys_metadata_iterator
  - keyring_component_status
  - keyring_metadata_query
  - keyring_reader
  - keyring_reload
  - keyring_writer
*/
/* clang-format on */

using keyring_common::operations::Keyring_operations;
using keyring_kmip::backend::Keyring_kmip_backend;
using keyring_kmip::config::Config_pod;
using keyring_kmip::config::g_component_path;
using keyring_kmip::config::g_instance_path;

/** Dependencies */
REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);

SERVICE_TYPE(log_builtins) * log_bi;
SERVICE_TYPE(log_builtins_string) * log_bs;

namespace keyring_kmip {
/** Keyring operations object */
std::unique_ptr<Keyring_operations<Keyring_kmip_backend,
                                   keyring_common::data::Data_extension<IdExt>>>
    g_keyring_operations;

/** Keyring data source */
std::unique_ptr<Config_pod> g_config_pod;

/** Keyring state */
bool g_keyring_kmip_inited = false;

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
  1. Read configuration file
  2. Read keyring file
  3. Initialize internal cache

  @returns Status of read operations
    @retval false Read config and data
    @retval true  Error reading config or data.
                  Existing data remains as it is.
*/
bool init_or_reinit_keyring() {
  /* Get config */
  std::unique_ptr<Config_pod> new_config_pod;
  if (keyring_kmip::config::find_and_read_config_file(new_config_pod))
    return true;

  /* Initialize backend handler */
  std::unique_ptr<Keyring_kmip_backend> new_backend =
      std::make_unique<Keyring_kmip_backend>(*new_config_pod.get());
  if (!new_backend || !new_backend.get()->valid()) return true;

  /* Create new operations class */
  std::unique_ptr<Keyring_operations<
      Keyring_kmip_backend, keyring_common::data::Data_extension<IdExt>>>
      new_operations(
          new Keyring_operations<Keyring_kmip_backend,
                                 keyring_common::data::Data_extension<IdExt>>(
              true, new_backend.release()));
  if (new_operations == nullptr) return true;

  if (!new_operations->valid()) {
    return true;
  }

  g_keyring_operations.swap(new_operations);
  g_config_pod.swap(new_config_pod);

  return false;
}

/**
  Initialization function for component - Used when loading the component
*/
static mysql_service_status_t keyring_kmip_init() {
  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;

  g_component_callbacks.reset(
      new keyring_common::service_implementation::Component_callbacks());

  return false;
}

/**
  De-initialization function for component - Used when unloading the component
*/
static mysql_service_status_t keyring_kmip_deinit() {
  g_keyring_kmip_inited = false;
  if (g_component_path) free(g_component_path);
  g_component_path = nullptr;
  if (g_instance_path) free(g_instance_path);
  g_instance_path = nullptr;

  g_keyring_operations.reset();
  g_component_callbacks.reset();

  return false;
}

}  // namespace keyring_kmip

/** ======================================================================= */

/** Component declaration related stuff */

/** This component provides implementation of following component services */
KEYRING_AES_IMPLEMENTOR(component_keyring_kmip);
KEYRING_GENERATOR_IMPLEMENTOR(component_keyring_kmip);
KEYRING_LOAD_IMPLEMENTOR(component_keyring_kmip);
KEYRING_KEYS_METADATA_FORWARD_ITERATOR_IMPLEMENTOR(component_keyring_kmip);
KEYRING_COMPONENT_STATUS_IMPLEMENTOR(component_keyring_kmip);
KEYRING_COMPONENT_METADATA_QUERY_IMPLEMENTOR(component_keyring_kmip);
KEYRING_READER_IMPLEMENTOR(component_keyring_kmip);
KEYRING_WRITER_IMPLEMENTOR(component_keyring_kmip);
/* Used if log_builtins is not available */
KEYRING_LOG_BUILTINS_IMPLEMENTOR(component_keyring_kmip);
KEYRING_LOG_BUILTINS_STRING_IMPLEMENTOR(component_keyring_kmip);

REQUIRES_SERVICE_PLACEHOLDER(psi_memory_v2);

/** Component provides */
BEGIN_COMPONENT_PROVIDES(component_keyring_kmip)
PROVIDES_SERVICE(component_keyring_kmip, keyring_aes),
    PROVIDES_SERVICE(component_keyring_kmip, keyring_generator),
    PROVIDES_SERVICE(component_keyring_kmip, keyring_load),
    PROVIDES_SERVICE(component_keyring_kmip, keyring_keys_metadata_iterator),
    PROVIDES_SERVICE(component_keyring_kmip, keyring_component_status),
    PROVIDES_SERVICE(component_keyring_kmip, keyring_component_metadata_query),
    PROVIDES_SERVICE(component_keyring_kmip, keyring_reader_with_status),
    PROVIDES_SERVICE(component_keyring_kmip, keyring_writer),
    PROVIDES_SERVICE(component_keyring_kmip, log_builtins),
    PROVIDES_SERVICE(component_keyring_kmip, log_builtins_string),
    END_COMPONENT_PROVIDES();

PSI_memory_key KEY_mem_keyring_kmip;

/** List of dependencies */
BEGIN_COMPONENT_REQUIRES(component_keyring_kmip)
REQUIRES_SERVICE(registry), REQUIRES_SERVICE(log_builtins),
    REQUIRES_SERVICE(log_builtins_string), REQUIRES_PSI_MEMORY_SERVICE,
    END_COMPONENT_REQUIRES();

/** Component description */
BEGIN_COMPONENT_METADATA(component_keyring_kmip)
METADATA("mysql.author", "Percona"), METADATA("mysql.license", "GPL"),
    METADATA("component_keyring_kmip_service", "1"), END_COMPONENT_METADATA();

/** Component declaration */
DECLARE_COMPONENT(component_keyring_kmip, "component_keyring_kmip")
keyring_kmip::keyring_kmip_init,
    keyring_kmip::keyring_kmip_deinit END_DECLARE_COMPONENT();

/** Component contained in this library */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(component_keyring_kmip)
    END_DECLARE_LIBRARY_COMPONENTS
