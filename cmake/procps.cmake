# Copyright (c) 2023 Percona LLC and/or its affiliates
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

MACRO (FIND_PROCPS)
  FIND_FILE(PROCPS_INCLUDE_DIR NAMES proc/procps.h NO_CACHE)
  IF (PROCPS_INCLUDE_DIR)
    MESSAGE("-- Found proc/sysinfo.h in ${PROCPS_INCLUDE_DIR} Procps version 3.")
    ADD_DEFINITIONS(-DHAVE_PROCPS_V3)
    SET(PROCPS_VERSION "3")
  ELSE()
  FIND_FILE(PROCPS_INCLUDE_DIR NAMES libproc2/meminfo.h NO_CACHE)
    IF (PROCPS_INCLUDE_DIR)
      MESSAGE("-- Found libproc2/meminfo.h in ${PROCPS_INCLUDE_DIR}. Procps version 4.")
      ADD_DEFINITIONS(-DHAVE_PROCPS_V4)
      SET(PROCPS_VERSION "4")
    ELSE()
      MESSAGE(SEND_ERROR "Cannot find proc/sysinfo.h or libproc2/meminfo.h in ${PROCPS_INCLUDE_DIR}. You can pass it to CMake with -DPROCPS_INCLUDE_DIR=<path> or install procps-devel/procps-ng-devel/libproc2-dev package")
    ENDIF()
  ENDIF()
ENDMACRO()
