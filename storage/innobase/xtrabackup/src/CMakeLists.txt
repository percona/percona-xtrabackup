# Copyright (c) 2013, 2018 Percona LLC and/or its affiliates.
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
INCLUDE(curl)

OPTION(WITH_VERSION_CHECK "Build with version check" ON)

ADD_SUBDIRECTORY(libarchive)
ADD_SUBDIRECTORY(crc)
ADD_SUBDIRECTORY(xbcloud)

SET(WITH_CURL "system" CACHE STRING "Using system curl by default")
FIND_GCRYPT()
MYSQL_CHECK_CURL()

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
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src/libarchive/libarchive
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src/quicklz
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src/crc
  ${GCRYPT_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
  ${LIBEV_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

ADD_DEFINITIONS(${SSL_DEFINES})

########################################################################
# xtrabackup binary
########################################################################
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/xtrabackup_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/xtrabackup_version.h)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/xtrabackup_config.h)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/plugin/keyring
  ${CMAKE_SOURCE_DIR}/plugin/keyring_vault
  ${BOOST_PATCHES_DIR})

ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/plugin/keyring "${CMAKE_CURRENT_BINARY_DIR}/keyring")
ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/plugin/keyring_vault "${CMAKE_CURRENT_BINARY_DIR}/keyring_vault")

ADD_COMPILE_FLAGS(
  xtrabackup.cc
  backup_mysql.cc
  COMPILE_FLAGS -DMYSQL_CLIENT)

MYSQL_ADD_EXECUTABLE(xtrabackup
  xtrabackup.cc
  innobackupex.cc
  changed_page_bitmap.cc
  compact.cc
  datasink.c
  ds_archive.c
  ds_buffer.c
  ds_compress.c
  ds_encrypt.c
  ds_decrypt.c
  ds_local.c
  ds_stdout.c
  ds_tmpfile.c
  ds_xbstream.c
  fil_cur.cc
  quicklz/quicklz.c
  read_filt.cc
  write_filt.cc
  wsrep.cc
  xbcrypt_common.c
  xbcrypt_write.c
  xbstream_write.c
  backup_mysql.cc
  backup_copy.cc
  keyring_plugins.cc
  kdf.cc
  ${CMAKE_SOURCE_DIR}/sql-common/client_authentication.cc
  ${CMAKE_SOURCE_DIR}/sql/des_key_file.cc
  ${CMAKE_SOURCE_DIR}/sql/auth/sha2_password_common.cc
  )

SET_TARGET_PROPERTIES(xtrabackup PROPERTIES ENABLE_EXPORTS TRUE)

TARGET_LINK_LIBRARIES(xtrabackup
  mysqlserver
  ${GCRYPT_LIBS}
  archive_static
  crc
  )

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
# innobackupex symlink
########################################################################
ADD_CUSTOM_COMMAND(TARGET xtrabackup
                   COMMAND ${CMAKE_COMMAND} ARGS -E create_symlink
                           xtrabackup innobackupex)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/innobackupex DESTINATION bin)

########################################################################
# xbstream binary
########################################################################
MYSQL_ADD_EXECUTABLE(xbstream
  ds_buffer.c
  ds_local.c
  ds_stdout.c
  ds_decrypt.c
  datasink.c
  xbstream.c
  xbstream_read.c
  xbstream_write.c
  xbcrypt_common.c
  xbcrypt_write.c
  )

SET_TARGET_PROPERTIES(xbstream
        PROPERTIES LINKER_LANGUAGE CXX
        )

TARGET_LINK_LIBRARIES(xbstream
  ${GCRYPT_LIBS}
  mysys
  mysys_ssl
  crc
  )

########################################################################
# xbcrypt binary
########################################################################
MYSQL_ADD_EXECUTABLE(xbcrypt
  xbcrypt.c
  xbcrypt_common.c
  xbcrypt_read.c
  xbcrypt_write.c
  datasink.c
  ds_encrypt.c
  ds_decrypt.c
  ds_local.c
  ds_stdout.c
  )

SET_TARGET_PROPERTIES(xbcrypt
        PROPERTIES LINKER_LANGUAGE CXX
        )

TARGET_LINK_LIBRARIES(xbcrypt
  ${GCRYPT_LIBS}
  mysys
  mysys_ssl
  crc
  )
