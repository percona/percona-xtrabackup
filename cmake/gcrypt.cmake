# Copyright (c) 2013 Percona LLC and/or its affiliates
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

MACRO (FIND_GCRYPT)

  IF (NOT GCRYPT_INCLUDE_PATH)
    SET(GCRYPT_INCLUDE_PATH /usr/include /usr/local/include /opt/local/include)
  ENDIF()

  FIND_PATH(GCRYPT_INCLUDE_DIR gcrypt.h PATHS ${GCRYPT_INDCLUDE_PATH})

  IF (NOT GCRYPT_INCLUDE_DIR)
    MESSAGE(SEND_ERROR "Cannot find gcrypt.h in ${GCRYPT_INCLUDE_PATH}. You can use libgcrypt-config --cflags to get the necessary path and pass it to CMake with -DGCRYPT_INCLUDE_PATH=<path>")
  ENDIF()

  IF (NOT GCRYPT_LIB_PATH)
    SET(GCRYPT_LIB_PATH /usr/lib /usr/local/lib /opt/local/lib)
  ENDIF()

  FIND_LIBRARY(GCRYPT_LIB gcrypt PATHS ${GCRYPT_LIB_PATH})
  FIND_LIBRARY(GPG_ERROR_LIB gpg-error PATHS ${GCRYPT_LIB_PATH})

  IF (NOT GCRYPT_LIB OR NOT GPG_ERROR_LIB)
    MESSAGE(SEND_ERROR "Cannot find libgcrypt shared libraries in ${GCRYPT_LIB_PATH}. You can use libgcrypt-config --libs to get the necessary path and pass it to CMake with -DGCRYPT_LIB_PATH=<path>")
  ENDIF()

  SET(GCRYPT_LIBS ${GCRYPT_LIB} ${GPG_ERROR_LIB})

ENDMACRO()
