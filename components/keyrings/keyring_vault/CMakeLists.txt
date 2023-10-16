# Copyright (c) 2023, Percona and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation. The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

# keyring_vault mtr suite is a default suite and must be installed always,
# regardless if the keyring_vault component is built and installed or not.
option(WITH_COMPONENT_KEYRING_VAULT "Building Keyring Vault Component" ON)
if(NOT WITH_COMPONENT_KEYRING_VAULT)
  message(STATUS "Not building Keyring Vault Component")
  return()
endif()

include(curl)
if(NOT DEFINED CURL_FOUND OR NOT CURL_FOUND)
  message(FATAL_ERROR "Not building Keyring Vault Component, could not find CURL library")
  return()
endif()

message(STATUS "Building Keyring Vault Component")

set(KEYRING_VAULT_SOURCE
  # Component handling
  keyring_vault.cc

  # Config handling
  config/config.cc

  # Encryption handling
  service_implementation/keyring_encryption_service_definition.cc

  # Generator handling
  service_implementation/keyring_generator_service_definition.cc

  # Keyring load handling
  service_implementation/keyring_load_service_definition.cc

  # Keys metadata iterator handling
  service_implementation/keyring_keys_metadata_iterator_service_definition.cc

  # Metadata query handling
  service_implementation/keyring_metadata_query_service_definition.cc

  # Reader handling
  service_implementation/keyring_reader_service_definition.cc

  # Writer handling
  service_implementation/keyring_writer_service_definition.cc

  # Backend handling
  backend/backend.cc
  backend/vault_base64.cc
  backend/vault_curl.cc
  backend/vault_keys_container.cc
  backend/vault_parser_composer.cc

  # Component callbacks
  component_callbacks.cc
)

set(KEYRING_VAULT_LIBRARIES keyring_common ext::curl extra::rapidjson)

MYSQL_ADD_COMPONENT(keyring_vault
  ${KEYRING_VAULT_SOURCE}
  LINK_LIBRARIES ${KEYRING_VAULT_LIBRARIES}
  MODULE_ONLY
)

target_compile_definitions(component_keyring_vault PRIVATE LOG_COMPONENT_TAG="component_keyring_vault")

target_include_directories(
  component_keyring_vault
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(
  component_keyring_vault
  SYSTEM PRIVATE
  ${BOOST_PATCHES_DIR}
  ${BOOST_INCLUDE_DIR}
)

if(APPLE)
  set_target_properties(component_keyring_vault PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup")
endif()
