# Copyright (c) 2017, Percona and/or its affiliates. All rights reserved.
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

# cmake include to wrap if compiler supports std cxx11 and alters compiler flags
# On return, HAVE_STDCXX11 will be set

INCLUDE (CheckCCompilerFlag)
INCLUDE (CheckCXXCompilerFlag)

check_cxx_compiler_flag (-std=c++11 HAVE_STDCXX11)

IF (HAVE_STDCXX11)
  IF(CMAKE_VERSION VERSION_LESS 3.1.0)
    # CMAKE_CXX_STANDARD was introduced in CMake 3.1, it will be ignored by older CMake
    STRING (REPLACE "-std=gnu++03" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    SET (CMAKE_CXX_FLAGS "--std=c++11 -Wno-deprecated-declarations ${CMAKE_CXX_FLAGS}")
  ELSE ()
    SET (CMAKE_CXX_FLAGS "-Wno-deprecated-declarations ${CMAKE_CXX_FLAGS}")
  ENDIF()
ENDIF ()


SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
