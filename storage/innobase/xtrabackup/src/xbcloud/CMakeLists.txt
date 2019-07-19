# Copyright (c) 2019 Percona and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

########################################################################
# xbcloud binary
########################################################################

INCLUDE(gcrypt)
INCLUDE(curl)
INCLUDE(libev)

SET(WITH_CURL "system" CACHE STRING "Using system curl by default")

# workaround for built-in rapidjson
MY_ADD_CXX_WARNING_FLAG("Wno-error=deprecated-copy")

FIND_GCRYPT()
MYSQL_CHECK_CURL()
FIND_EV()

# check that compiler supports cxx11 and set options
INCLUDE (check_stdcxx11)
IF (!HAVE_STDCXX11)
  MESSAGE (${MYROCKS_STATUS_MODE} "${CMAKE_CXX_COMPILER} doesn't support -std=c++11, you need one that does. Not building MyRocks")
  RETURN ()
ENDIF ()

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/storage/innobase/include
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src/crc
  ${CMAKE_CURRENT_BINARY_DIR}/..
  ${GCRYPT_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
  ${LIBEV_INCLUDE_DIRS}
  )

MYSQL_ADD_EXECUTABLE(xbcloud
  xbcloud.cc
  ../xbstream_read.c
  http.cc
  s3.cc
  swift.cc)

SET_TARGET_PROPERTIES(xbcloud
        PROPERTIES LINKER_LANGUAGE CXX
        )

TARGET_LINK_LIBRARIES(xbcloud
  ${GCRYPT_LIBS}
  ${LIBEV_LIBRARIES}
  ${CURL_LIBRARIES}
  mysys
  mysys_ssl
  crc
  )

IF (WITH_DEBUG)

  ENABLE_TESTING()

  FIND_PACKAGE(GTest)

  IF (GTEST_FOUND)

    INCLUDE_DIRECTORIES(
      ${GTEST_INCLUDE_DIRECTORIES}
      )

    ADD_EXECUTABLE(xbcloud-t xbcloud-t.cc
      ../xbstream_read.c
      http.cc
      s3.cc
      swift.cc)

    TARGET_LINK_LIBRARIES(xbcloud-t
      ${GCRYPT_LIBS}
      ${LIBEV_LIBRARIES}
      ${CURL_LIBRARIES}
      ${GTEST_BOTH_LIBRARIES}
      gmock
      mysys
      mysys_ssl
      crc
      )

    ADD_TEST(xbcloud xbcloud-t)

  ENDIF()

ENDIF()
