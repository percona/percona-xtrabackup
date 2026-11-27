# Copyright (c) 2021, 2026, Oracle and/or its affiliates.
# Copyright (c) 2026 Percona LLC and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF (NOT DEFINED WITH_COMPONENT_PERCONA_KEYRING_ENCRYPTED_FILE AND
    NOT DEFINED WITHOUT_COMPONENT_PERCONA_KEYRING_ENCRYPTED_FILE)
  SET(WITH_COMPONENT_PERCONA_KEYRING_ENCRYPTED_FILE 1)
ENDIF()

IF(NOT WITH_COMPONENT_PERCONA_KEYRING_ENCRYPTED_FILE)
  RETURN()
ENDIF()

ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="component_percona_keyring_encrypted_file")

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BOOST_PATCHES_DIR}
  ${BOOST_INCLUDE_DIR}
)


SET(PERCONA_KEYRING_ENCRYPTED_FILE_SOURCE
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

  # Config handling
  config/config.cc

  # Keyring file component handling
        percona_keyring_encrypted_file.cc

  # Component callbacks
  component_callbacks.cc
)

SET(PERCONA_KEYRING_ENCRYPTED_FILE_LIBRARIES
  keyring_common
  OpenSSL::SSL OpenSSL::Crypto
  library_mysys
)

MYSQL_ADD_COMPONENT(percona_keyring_encrypted_file
  ${PERCONA_KEYRING_ENCRYPTED_FILE_SOURCE}
  LINK_LIBRARIES ${PERCONA_KEYRING_ENCRYPTED_FILE_LIBRARIES}
  MODULE_ONLY
)

TARGET_LINK_OPTIONS(component_percona_keyring_encrypted_file PRIVATE "${LINK_FLAG_NO_UNDEFINED}")

IF(APPLE)
  SET_TARGET_PROPERTIES(component_percona_keyring_encrypted_file PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup")
ENDIF()

