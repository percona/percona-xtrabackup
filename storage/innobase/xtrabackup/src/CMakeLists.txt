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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

INCLUDE(gcrypt)
INCLUDE(curl)
INCLUDE(libev)

ADD_SUBDIRECTORY(libarchive)

FIND_GCRYPT()
FIND_CURL()
FIND_EV()

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/storage/innobase/include
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src/libarchive/libarchive
  ${CMAKE_SOURCE_DIR}/storage/innobase/xtrabackup/src/quicklz
  ${GCRYPT_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
  ${LIBEV_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

########################################################################
# xtrabackup binary
########################################################################
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/xtrabackup_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/xtrabackup_version.h )


MYSQL_ADD_EXECUTABLE(xtrabackup
  xtrabackup.cc
  changed_page_bitmap.cc
  compact.cc
  datasink.c
  ds_archive.c
  ds_buffer.c
  ds_compress.c
  ds_encrypt.c
  ds_local.c
  ds_stdout.c
  ds_tmpfile.c
  ds_xbstream.c
  fil_cur.cc
  quicklz/quicklz.c
  read_filt.cc
  write_filt.cc
  wsrep.cc
  xbcrypt_common.c
  xbcrypt_write.c
  xbstream_write.c
  )

SET_TARGET_PROPERTIES(xtrabackup PROPERTIES ENABLE_EXPORTS TRUE)

TARGET_LINK_LIBRARIES(xtrabackup 
  mysqlserver 
  ${GCRYPT_LIBS} 
  archive_static
  )

########################################################################
# xbstream binary
########################################################################
MYSQL_ADD_EXECUTABLE(xbstream
  ds_buffer.c
  ds_local.c
  ds_stdout.c
  datasink.c
  xbstream.c
  xbstream_read.c
  xbstream_write.c
  )

SET_TARGET_PROPERTIES(xbstream
        PROPERTIES LINKER_LANGUAGE CXX
        )

TARGET_LINK_LIBRARIES(xbstream
  mysys
  mysys_ssl
  )

########################################################################
# xbcrypt binary
########################################################################
MYSQL_ADD_EXECUTABLE(xbcrypt
  xbcrypt.c
  xbcrypt_common.c
  xbcrypt_read.c
  xbcrypt_write.c
  )

SET_TARGET_PROPERTIES(xbcrypt
        PROPERTIES LINKER_LANGUAGE CXX
        )

TARGET_LINK_LIBRARIES(xbcrypt
  ${GCRYPT_LIBS}
  mysys
  mysys_ssl
  )

########################################################################
# xbcloud binary
########################################################################
MYSQL_ADD_EXECUTABLE(xbcloud
  xbcloud.cc
  )

SET_TARGET_PROPERTIES(xbcloud
        PROPERTIES LINKER_LANGUAGE CXX
        )

TARGET_LINK_LIBRARIES(xbcloud
  ${GCRYPT_LIBS}
  ${LIBEV_LIBRARIES}
  ${CURL_LIBRARIES}
  mysys
  )
