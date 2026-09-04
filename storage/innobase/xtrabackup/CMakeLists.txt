# Copyright (c) 2013 Percona LLC and/or its affiliates.
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

SET(XB_DISTRIBUTION $ENV{XB_DISTRIBUTION})

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/xbcloud_osenv.sh
  ${CMAKE_CURRENT_BINARY_DIR}/xbcloud_osenv ESCAPE_QUOTES @ONLY)

INSTALL_SCRIPT(${CMAKE_CURRENT_BINARY_DIR}/xbcloud_osenv
  DESTINATION ${INSTALL_BINDIR}
  COMPONENT Server
  )

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(doc/source)

IF(INSTALL_MYSQLTESTDIR)
  INSTALL(
    DIRECTORY test
    DESTINATION ${INSTALL_MYSQLTESTDIR}
    USE_SOURCE_PERMISSIONS
    COMPONENT Test
    PATTERN "var" EXCLUDE
    PATTERN "results" EXCLUDE
    PATTERN "kewpie" EXCLUDE
    PATTERN "test_results.subunit" EXCLUDE
    PATTERN "CPack" EXCLUDE
    PATTERN "CMake*" EXCLUDE
    PATTERN "Makefile" EXCLUDE
    )
ENDIF()

ADD_CUSTOM_TARGET(link_test_dir ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/test" "${CMAKE_CURRENT_BINARY_DIR}/test")

# PXB-3862: the server's unittest tree is disabled in XtraBackup builds
# (top-level gates on "WITH_UNIT_TESTS AND NOT WITH_XTRABACKUP"), so build
# the page-grouping / storage-probe unit tests here, self-contained
# against the bundled googletest. The tested code is header-only
# (xb_page_group.h, xb_io_probe.h), so no server libraries are needed.
IF(WITH_UNIT_TESTS)
  FILE(GLOB XB_GTEST_ROOT
    ${CMAKE_SOURCE_DIR}/extra/googletest/googletest-release-*/googletest)
  ADD_EXECUTABLE(xb_page_group-t
    ${CMAKE_SOURCE_DIR}/unittest/gunit/innodb/xb_page_group-t.cc
    ${XB_GTEST_ROOT}/src/gtest-all.cc
    ${XB_GTEST_ROOT}/src/gtest_main.cc)
  TARGET_INCLUDE_DIRECTORIES(xb_page_group-t PRIVATE
    ${CMAKE_SOURCE_DIR} ${XB_GTEST_ROOT}/include ${XB_GTEST_ROOT})
  SET_TARGET_PROPERTIES(xb_page_group-t PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory)
  TARGET_COMPILE_OPTIONS(xb_page_group-t PRIVATE -Wno-undef)
  FIND_PACKAGE(Threads REQUIRED)
  TARGET_LINK_LIBRARIES(xb_page_group-t Threads::Threads)
  ADD_TEST(NAME xb_page_group COMMAND xb_page_group-t)
ENDIF()
