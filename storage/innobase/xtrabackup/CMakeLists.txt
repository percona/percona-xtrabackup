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
