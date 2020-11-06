# Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2013, 2018, Percona LLC and/or its affiliates
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)

SET(WITH_INNOBASE_STORAGE_ENGINE ON)
SET(WITHOUT_MOCK_SECONDARY_STORAGE_ENGINE ON)
IF (NOT WITH_ZLIB)
  SET(WITH_ZLIB bundled CACHE STRING "Use bundled zlib")
ENDIF()

IF(WIN32)
  IF(NOT CMAKE_USING_VC_FREE_TOOLS)
    # Sign executables with authenticode certificate
    SET(SIGNCODE 1 CACHE BOOL "")
  ENDIF()
ENDIF()

IF(UNIX)
  SET(WITH_EXTRA_CHARSETS all CACHE STRING "")

  OPTION(WITH_PIC "" ON) # Why?

  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    IF(NOT IGNORE_AIO_CHECK)
      # Ensure aio is available on Linux (required by InnoDB)
      CHECK_INCLUDE_FILES(libaio.h HAVE_LIBAIO_H)
      CHECK_LIBRARY_EXISTS(aio io_queue_init "" HAVE_LIBAIO)
      IF(NOT HAVE_LIBAIO_H OR NOT HAVE_LIBAIO)
        MESSAGE(FATAL_ERROR "
        aio is required on Linux, you need to install the required library:

          Debian/Ubuntu:              apt-get install libaio-dev
          RedHat/Fedora/Oracle Linux: yum install libaio-devel
          SuSE:                       zypper install libaio-devel

        If you really do not want it, pass -DIGNORE_AIO_CHECK to cmake.
        ")
      ENDIF()
    ENDIF()

    # Enable fast mutexes on Linux
    OPTION(WITH_FAST_MUTEXES "" ON)
    # Enable building man pages on Linux by default
    OPTION(WITH_MAN_PAGES "" ON)
  ENDIF()

ENDIF()
