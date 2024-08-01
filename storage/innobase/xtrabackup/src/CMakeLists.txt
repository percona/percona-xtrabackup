# Copyright (c) 2013, 2023 Percona LLC and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

INCLUDE(gcrypt)
INCLUDE(procps)

OPTION(WITH_VERSION_CHECK "Build with version check" ON)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

FIND_GCRYPT()

IF(NOT APPLE)
  FIND_PROCPS()
ENDIF()

CHECK_TYPE_SIZE("unsigned long" SIZEOF_UNSIGNED_LONG)

IF(WITH_VERSION_CHECK)
# xxd is needed to embed version_check script
FIND_PROGRAM(XXD_PATH xxd)

IF(NOT XXD_PATH)
  MESSAGE(FATAL_ERROR "xxd not found. Try to install vim-common.")
ENDIF(NOT XXD_PATH)

SET(HAVE_VERSION_CHECK 1)
ELSE()
SET(HAVE_VERSION_CHECK 0)
ENDIF(WITH_VERSION_CHECK)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/storage/innobase/include
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src/quicklz
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src/crc
  ${GCRYPT_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

ADD_DEFINITIONS(${SSL_DEFINES})

ADD_SUBDIRECTORY(crc)
ADD_SUBDIRECTORY(jsmn)
ADD_SUBDIRECTORY(xbcloud)

########################################################################
# xtrabackup binary
########################################################################
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/xtrabackup_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/xtrabackup_version.h)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/xtrabackup_config.h)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/plugin/keyring
  ${BOOST_PATCHES_DIR})


MY_ADD_COMPILE_DEFINITIONS(
  xtrabackup.cc
  backup_mysql.cc
  file_utils.cc
  xb_dict.cc
  COMPILE_DEFINITIONS MYSQL_CLIENT)

MYSQL_ADD_EXECUTABLE(xtrabackup
  xtrabackup.cc
  changed_page_tracking.cc
  datasink.cc
  ds_buffer.cc
  ds_compress.cc
  ds_compress_lz4.cc
  ds_compress_zstd.cc
  ds_encrypt.cc
  ds_fifo.cc
  ds_local.cc
  ds_stdout.cc
  ds_tmpfile.cc
  ds_xbstream.cc
  fil_cur.cc
  file_utils.cc
  quicklz/quicklz.c
  read_filt.cc
  write_filt.cc
  wsrep.cc
  xbcrypt_common.cc
  xbcrypt_write.cc
  xbstream_write.cc
  backup_mysql.cc
  xb_dict.cc
  backup_copy.cc
  keyring_plugins.cc
  keyring_components.cc
  utils.cc
  kdf.cc
  space_map.cc
  redo_log_consumer.cc
  redo_log.cc
  ${CMAKE_SOURCE_DIR}/sql-common/client_authentication.cc
  )

SET_TARGET_PROPERTIES(xtrabackup PROPERTIES ENABLE_EXPORTS true)

TARGET_LINK_LIBRARIES(xtrabackup
  sql_main
  innobase
  sql_dd
  sql_gis
  rpl
  rpl_source
  rpl_replica
  mysqlservices
  binlog
  mysqlclient
  minchassis
  keyring_common
  ${GCRYPT_LIBS}
  ext::icu
  crc
  )

IF(NOT APPLE)
  IF(PROCPS_VERSION EQUAL 4)
    TARGET_LINK_LIBRARIES(xtrabackup proc2)
  ELSE()
    TARGET_LINK_LIBRARIES(xtrabackup procps)
  ENDIF()
ENDIF()

 # We depend on protobuf because of the mysqlx plugin and replication.
 IF(UNIX_INSTALL_RPATH_ORIGIN_PRIV_LIBDIR)
   ADD_INSTALL_RPATH_FOR_PROTOBUF(xtrabackup)
 ENDIF()

 # KMIP depend on boost
 INCLUDE_DIRECTORIES(SYSTEM
        ${BOOST_PATCHES_DIR}
        ${BOOST_INCLUDE_DIR})

IF(WITH_VERSION_CHECK)
ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version_check_pl.h
                   COMMAND ${XXD_PATH} --include version_check.pl
                   ${CMAKE_CURRENT_BINARY_DIR}/version_check_pl.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

ADD_CUSTOM_TARGET(GenVersionCheck
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/version_check_pl.h)

ADD_DEPENDENCIES(xtrabackup GenVersionCheck)
ENDIF(WITH_VERSION_CHECK)

########################################################################
# xbstream binary
########################################################################
MYSQL_ADD_EXECUTABLE(xbstream
  ds_buffer.cc
  ds_local.cc
  ds_stdout.cc
  ds_decompress.cc
  ds_decrypt.cc
  ds_decompress_lz4.cc
  ds_decompress_zstd.cc
  datasink.cc
  file_utils.cc
  quicklz/quicklz.c
  xbstream.cc
  xbstream_read.cc
  xbstream_write.cc
  xbcrypt_common.cc
  xbcrypt_write.cc
  ${BUNDLED_LZ4_PATH}/xxhash.c
  )

MY_ADD_COMPILE_DEFINITIONS(
  ${BUNDLED_LZ4_PATH}/xxhash.c
  COMPILE_DEFINITIONS XXH_NAMESPACE=MY_
  )
ADD_COMPILE_FLAGS(
    ${BUNDLED_LZ4_PATH}/xxhash.c
    ds_decompress_lz4.cc
    ds_decompress_zstd.cc
    ds_compress_lz4.cc
    COMPILE_FLAGS -I${CMAKE_SOURCE_DIR}/extra/lz4 -I${BUNDLED_LZ4_PATH}
)

SET_TARGET_PROPERTIES(xbstream
        PROPERTIES LINKER_LANGUAGE CXX
        )

TARGET_LINK_LIBRARIES(xbstream
  ext::zstd
  ${GCRYPT_LIBS}
  mysys
  crc
  ext::lz4
  )

########################################################################
# xbcrypt binary
########################################################################
MYSQL_ADD_EXECUTABLE(xbcrypt
  file_utils.cc
  xbcrypt.cc
  xbcrypt_common.cc
  xbcrypt_read.cc
  xbcrypt_write.cc
  datasink.cc
  ds_encrypt.cc
  ds_decrypt.cc
  ds_local.cc
  ds_stdout.cc
  )

SET_TARGET_PROPERTIES(xbcrypt
        PROPERTIES LINKER_LANGUAGE CXX
        )

TARGET_LINK_LIBRARIES(xbcrypt
  ${GCRYPT_LIBS}
  mysys
  crc
  )
