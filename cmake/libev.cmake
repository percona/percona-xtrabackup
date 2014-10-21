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

MACRO(FIND_EV)

FIND_PACKAGE(LibEv)
IF(LIBEV_FOUND)
    MESSAGE(STATUS "libev libraries found at: ${LIBEV_LIBRARIES}")
    MESSAGE(STATUS "libev includes found at: ${LIBEV_INCLUDE_DIRS}")
ELSE()
    MESSAGE(SEND_ERROR "Could not find libev on your system")
ENDIF(LIBEV_FOUND)

ENDMACRO()
