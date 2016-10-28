# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
#


# avoid running system checks by using pre-cached check results
# system checks are expensive on VS since every tiny program is to be compiled in
# a VC solution.
GET_FILENAME_COMPONENT(_SCRIPT_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
INCLUDE(${_SCRIPT_DIR}/WindowsCache.cmake)

IF(MSVC)
  # Enable "Full Path of Source Code File in Diagnostics" to avoid
  # "guessing" which file was causing warnings or error
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FC")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /FC")
ENDIF()
