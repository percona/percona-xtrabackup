# Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Default install component for the files is Server here
SET(MYSQL_INSTALL_COMPONENT Server)

IF(NOT CMAKE_CROSSCOMPILING)
  MYSQL_ADD_EXECUTABLE(comp_err comp_err.cc SKIP_INSTALL)
  TARGET_LINK_LIBRARIES(comp_err mysys mysys_ssl)
ENDIF()

MYSQL_ADD_EXECUTABLE(comp_client_err
  comp_client_err.cc ${CMAKE_SOURCE_DIR}/libmysql/errmsg.cc
  SKIP_INSTALL)
TARGET_LINK_LIBRARIES(comp_client_err mysys mysys_ssl)

ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_BINARY_DIR}/include/mysqlclient_ername.h
  COMMAND comp_client_err
  --in_file=${PROJECT_SOURCE_DIR}/include/errmsg.h
  --out_file=${PROJECT_BINARY_DIR}/include/mysqlclient_ername.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/comp_client_err.cc)

ADD_CUSTOM_TARGET(GenClientError
                  ALL
                  DEPENDS ${PROJECT_BINARY_DIR}/include/mysqlclient_ername.h)


IF (BUILD_BUNDLED_LZ4)
  ADD_CONVENIENCE_LIBRARY(lz4_lib
    ../extra/lz4/lz4.c
    ../extra/lz4/lz4frame.c
    ../extra/lz4/lz4hc.c
    ../extra/lz4/xxhash.c
  )
ENDIF()

FILE(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR})

ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_BINARY_DIR}/include/mysqld_error.h 
                   ${PROJECT_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR}/english/errmsg.sys
                   COMMAND comp_err
                    --charset=${PROJECT_SOURCE_DIR}/share/charsets
                    --out-dir=${PROJECT_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR}/
                    --header_file=${PROJECT_BINARY_DIR}/include/mysqld_error.h 
                    --name_file=${PROJECT_BINARY_DIR}/include/mysqld_ername.h
                    --in_file=${PROJECT_SOURCE_DIR}/share/errmsg-utf8.txt
                   DEPENDS ${PROJECT_SOURCE_DIR}/share/errmsg-utf8.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/comp_err.cc)

ADD_CUSTOM_TARGET(GenError
                  ALL
                  DEPENDS 
                  ${PROJECT_BINARY_DIR}/include/mysqld_error.h
                  ${PROJECT_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR}/english/errmsg.sys
                  ${PROJECT_SOURCE_DIR}/share/errmsg-utf8.txt)

# Set InnoDB mutex type
ADD_DEFINITIONS(-DMUTEX_EVENT)

## Most executables depend on libeay32.dll (through mysys_ssl).
## For 'comp_err' we need it to be available during build.
IF (WIN32 AND WITH_SSL_PATH AND HAVE_CRYPTO_DLL)
  ADD_DEPENDENCIES(GenError copy_openssl_dlls)
  ADD_DEPENDENCIES(comp_client_err copy_openssl_dlls)
  ADD_DEPENDENCIES(comp_err copy_openssl_dlls)
ENDIF()

IF(APPLE AND HAVE_CRYPTO_DYLIB AND HAVE_OPENSSL_DYLIB)
  ADD_DEPENDENCIES(GenError copy_openssl_dlls)
  ADD_DEPENDENCIES(comp_client_err copy_openssl_dlls)
  ADD_DEPENDENCIES(comp_err copy_openssl_dlls)
ENDIF()

# In published release builds on Solaris, we need to bundle gcc source.
# PB2 will take care of putting it in extra/ when needed
IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gcc-4.8.1.tar.bz2)
  INSTALL(FILES gcc-4.8.1.tar.bz2 
          DESTINATION ${INSTALL_MYSQLSHAREDIR} COMPONENT Development)
ENDIF()
