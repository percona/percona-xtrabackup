INCLUDE_DIRECTORIES(SYSTEM
  ${BOOST_PATCHES_DIR}
  ${BOOST_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/plugin/keyring
  ${CMAKE_SOURCE_DIR}/plugin/keyring/common
  ${CMAKE_SOURCE_DIR}/plugin/keyring_vault
  ${CMAKE_SOURCE_DIR}/unittest/gunit/keyring_vault)

MYSQL_ADD_EXECUTABLE(keyring_vault-test
               ./keyring_vault-test.cc
               ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keyring_key.cc
               ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keys_container.cc
               ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keys_iterator.cc
               ${CMAKE_SOURCE_DIR}/plugin/keyring/common/system_key_adapter.cc
               ${CMAKE_SOURCE_DIR}/plugin/keyring/common/system_keys_container.cc
               ${CMAKE_SOURCE_DIR}/plugin/keyring/common/keyring_impl.cc
               ../vault_io.cc
               ../vault_key.cc
               ../vault_base64.cc
               ../vault_parser_composer.cc
               ../vault_keys_list.cc
               ../vault_keys_container.cc
               ../vault_curl.cc
               ../vault_credentials_parser.cc
               ../vault_credentials.cc
               ${CMAKE_SOURCE_DIR}/unittest/gunit/keyring_vault/generate_credential_file.cc
               ${CMAKE_SOURCE_DIR}/unittest/gunit/keyring_vault/vault_mount.cc
               ADD_TEST)

TARGET_LINK_LIBRARIES(keyring_vault-test strings dbug regex mysys sql)
TARGET_LINK_LIBRARIES(keyring_vault-test binlog rpl master slave)
TARGET_LINK_LIBRARIES(keyring_vault-test ${CURL_LIBRARY} ${SSL_LIBRARIES})

ADD_TEST(keyring_vault-test keyring_vault-test)
