#Copyright(c) 2021, Oracle and / or its affiliates.
#
#This program is free software; you can redistribute it and / or modify
#it under the terms of the GNU General Public License, version 2.0,
#as published by the Free Software Foundation.
#
#This program is also distributed with certain software(including
#but not limited to OpenSSL) that is licensed under separate terms,
#as designated in a particular file or component or in included license
#documentation.The authors of MySQL hereby grant you an additional
#permission to link the program and your derivative works with the
#separately licensed software that they have included with MySQL.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
#GNU General Public License, version 2.0, for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110 - 1301 USA

IF(NOT DEFINED WITH_COMPONENT_KEYRING_KMS AND NOT DEFINED
       WITHOUT_COMPONENT_KEYRING_KMS)
SET(WITH_COMPONENT_KEYRING_KMS 1)
ENDIF()

IF(NOT WITH_COMPONENT_KEYRING_KMS)
RETURN()
ENDIF()

ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="component_keyring_kms")

INCLUDE(curl)

SET(KEYRING_KMS_SOURCE
#Encryption handling
        service_implementation/keyring_encryption_service_definition.cc

#Generator handling
        service_implementation/keyring_generator_service_definition.cc

#Keyring load handling
        service_implementation/keyring_load_service_definition.cc

#Keys metadata iterator handling
        service_implementation/keyring_keys_metadata_iterator_service_definition.cc

#Metadata query handling
        service_implementation/keyring_metadata_query_service_definition.cc

#Reader handling
        service_implementation/keyring_reader_service_definition.cc

#Writer handling
        service_implementation/keyring_writer_service_definition.cc

#Backend handling
        backend/backend.cc

        backend/kms.cc

#Config handling
        config/config.cc

#Keyring kms component handling
        keyring_kms.cc

#Component callbacks
        component_callbacks.cc)

SET(KEYRING_KMS_LIBRARIES keyring_common ${CURL_LIBRARY} ${SSL_LIBRARIES})

MYSQL_ADD_COMPONENT(keyring_kms ${KEYRING_KMS_SOURCE} LINK_LIBRARIES ${KEYRING_KMS_LIBRARIES} MODULE_ONLY)

MY_CHECK_CXX_COMPILER_WARNING("-Wno-suggest-override" HAS_FLAG)
IF(HAS_FLAG)
  TARGET_COMPILE_OPTIONS(component_keyring_kms PUBLIC "-Wno-suggest-override")
ENDIF()

TARGET_INCLUDE_DIRECTORIES(
  component_keyring_kms
  PUBLIC
)
TARGET_INCLUDE_DIRECTORIES(
  component_keyring_kms
  SYSTEM
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BOOST_PATCHES_DIR} 
  ${BOOST_INCLUDE_DIR}
)


IF(APPLE)
  SET_TARGET_PROPERTIES(component_keyring_kms PROPERTIES LINK_FLAGS
                      "-undefined dynamic_lookup")

ENDIF()
