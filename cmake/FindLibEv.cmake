# Copyright (c) 2014 Percona LLC and/or its affiliates
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

# This module defines
#  LIBEV_INCLUDE_DIRS, where to find LibEv headers
#  LIBEV_LIBRARIES, LibEv libraries
#  LIBEV_FOUND, If false, do not try to use ant

find_path(LIBEV_INCLUDE_DIRS ev.h PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /usr/include/ev
  )

set(LIBEV_LIB_PATHS /usr/lib /usr/local/lib /opt/local/lib)
find_library(LIBEV_LIB NAMES ev PATHS ${LIBEV_LIB_PATHS})

if (LIBEV_LIB AND LIBEV_INCLUDE_DIRS)
  set(LIBEV_FOUND TRUE)
  set(LIBEV_LIBRARIES ${LIBEV_LIB})
else ()
  set(LIBEV_FOUND FALSE)
endif ()

if (LIBEV_FOUND)
  if (NOT LIBEV_FIND_QUIETLY)
    message(STATUS "Found libev: ${LIBEV_LIBRARIES}")
  endif ()
else ()
  message(STATUS "libev NOT found.")
endif ()

mark_as_advanced(
    LIBEV_LIB
    LIBEV_INCLUDE_DIRS
  )
