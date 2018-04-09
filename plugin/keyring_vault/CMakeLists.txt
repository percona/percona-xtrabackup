IF (NOT DEFINED WITH_KEYRING_VAULT)
  SET (WITH_KEYRING_VAULT 1)
ENDIF()
IF (NOT WITH_KEYRING_VAULT)
  MESSAGE (STATUS "Not building keyring_vault plugin")
  RETURN()
ENDIF()

INCLUDE(CheckFunctionExists)
INCLUDE(curl)

SET(CMAKE_EXTRA_INCLUDE_FILES string.h)
SET(CMAKE_EXTRA_INCLUDE_FILES)

MACRO (CHECK_IF_LIB_FOUND lib_name project_name status_mode)
  SET (lib_found_variable_name "${lib_name}_FOUND")
  IF (NOT DEFINED ${lib_found_variable_name} OR NOT ${${lib_found_variable_name}})
    message(${status_mode} "Not building ${project_name}, could not find library: ${lib_name}")
    RETURN()
  ENDIF()
ENDMACRO()

CHECK_IF_LIB_FOUND(CURL "keyring_vault" FATAL_ERROR)
MESSAGE (STATUS "Building keyring_vault plugin")

INCLUDE_DIRECTORIES(${BOOST_PATCHES_DIR})
INCLUDE_DIRECTORIES(SYSTEM ${BOOST_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/keyring/common)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/keyring)

MYSQL_ADD_PLUGIN(keyring_vault
                 ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keyring_key.cc
                 ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keys_container.cc
                 ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keys_iterator.cc
                 ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keyring_impl.cc
                 ${CMAKE_SOURCE_DIR}/plugin/keyring/file_io.cc
                 ${CMAKE_SOURCE_DIR}/plugin/keyring/common/system_key_adapter.cc
                 ${CMAKE_SOURCE_DIR}/plugin/keyring/common/system_keys_container.cc
                 vault_io.cc
                 vault_key.cc
                 vault_base64.cc
                 vault_parser.cc
                 vault_keys_list.cc
                 vault_keys_container.cc
                 vault_curl.cc
                 vault_credentials_parser.cc
                 vault_credentials.cc
                 vault_keyring.cc
                 LINK_LIBRARIES ${CURL_LIBRARY}
                 MODULE_ONLY
                 MODULE_OUTPUT_NAME "keyring_vault")

# We limit symbols exported on Linux to only those required by server.
IF(LINK_FLAG_NO_UNDEFINED)
  GET_PROPERTY(keyring_vault_link_flags TARGET keyring_vault PROPERTY LINK_FLAGS)
  SET_PROPERTY(TARGET keyring_vault PROPERTY LINK_FLAGS "${keyring_vault_link_flags} -Wl,--version-script=${CMAKE_SOURCE_DIR}/plugin/keyring_vault/keyring_vault.version")
ENDIF()

IF(WITH_KEYRING_VAULT_TEST)
  ADD_SUBDIRECTORY(keyring_vault-test)
ENDIF()

IF(INSTALL_MYSQLTESTDIR)
  INSTALL(DIRECTORY tests/mtr/ DESTINATION ${INSTALL_MYSQLTESTDIR}/suite/keyring_vault COMPONENT Test)
ENDIF()
