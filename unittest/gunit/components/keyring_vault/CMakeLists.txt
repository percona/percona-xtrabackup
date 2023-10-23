if(NOT WITH_COMPONENT_KEYRING_VAULT)
  message(STATUS "Not building Keyring Vault Component unit tests")
  return()
endif()

message(STATUS "Building Keyring Vault Component unit tests")

set(SRC_FILES)
list(APPEND SRC_FILES
  ${CMAKE_SOURCE_DIR}/components/keyrings/common/data/meta.cc
  ${CMAKE_SOURCE_DIR}/components/keyrings/common/data/data.cc
  ${CMAKE_SOURCE_DIR}/components/keyrings/common/data/pfs_string.cpp
  ${CMAKE_SOURCE_DIR}/components/keyrings/common/utils/utils.cc
  ${CMAKE_SOURCE_DIR}/components/keyrings/keyring_vault/config/config.cc
  ${CMAKE_SOURCE_DIR}/components/keyrings/keyring_vault/backend/backend.cc
  ${CMAKE_SOURCE_DIR}/components/keyrings/keyring_vault/backend/vault_base64.cc
  ${CMAKE_SOURCE_DIR}/components/keyrings/keyring_vault/backend/vault_curl.cc
  ${CMAKE_SOURCE_DIR}/components/keyrings/keyring_vault/backend/vault_parser_composer.cc
  ${CMAKE_SOURCE_DIR}/components/keyrings/keyring_vault/backend/vault_keys_container.cc
  ${CMAKE_SOURCE_DIR}/unittest/gunit/components/keyring_vault/component_callbacks_mock.cc
)

add_library(component_keyring_vault_test STATIC ${SRC_FILES})
add_dependencies(component_keyring_vault_test GenError)
target_link_libraries(component_keyring_vault_test extra::rapidjson)
target_compile_definitions(component_keyring_vault_test PUBLIC LOG_COMPONENT_TAG="component_keyring_vault")
target_include_directories(component_keyring_vault_test SYSTEM PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${GMOCK_INCLUDE_DIRS}
  ${BOOST_PATCHES_DIR}
  ${BOOST_INCLUDE_DIR}
)
target_include_directories(component_keyring_vault_test PUBLIC
  ${CMAKE_SOURCE_DIR}/components/keyrings/keyring_vault
)

set(TESTS
  vault_backend
  vault_config
  vault_parser
)

set(ALL_COMPONENT_KEYRING_VAULT_TESTS)
foreach(test ${TESTS})
  list(APPEND ALL_COMPONENT_KEYRING_VAULT_TESTS ${test}-t.cc)
endforeach()

MYSQL_ADD_EXECUTABLE(merge_component_keyring_vault_tests-t
  ${ALL_COMPONENT_KEYRING_VAULT_TESTS}
  ENABLE_EXPORTS
  ADD_TEST merge_component_keyring_vault_tests)

target_link_libraries(merge_component_keyring_vault_tests-t
  component_keyring_vault_test
  gunit_large
  perfschema
  sql_main
  strings
  mysys
  sql_gis
  sql_dd
  minchassis
  binlog
  rpl
  rpl_source
  rpl_replica
  ext::curl
  ext::icu
)

add_dependencies(merge_component_keyring_vault_tests-t GenError)

foreach(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t ${test}-t.cc
    ENABLE_EXPORTS SKIP_INSTALL EXCLUDE_FROM_ALL)

  target_link_libraries(${test}-t component_keyring_vault_test gunit_small server_unittest_library ext::curl ext::icu)
endforeach()
