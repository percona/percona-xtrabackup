# Copyright (c) 2015 Percona LLC and/or its affiliates.
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

# generate man pages
IF(WITH_MAN_PAGES)
  IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/build/man)
    FILE(GLOB MAN_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/build/man ${CMAKE_CURRENT_SOURCE_DIR}/build/man/*.1)
    FOREACH(MAN_FILE ${MAN_FILES})
      CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build/man/${MAN_FILE}
        ${CMAKE_CURRENT_BINARY_DIR}/build/man/${MAN_FILE} COPYONLY)
    ENDFOREACH()
  ELSE()
    ADD_CUSTOM_COMMAND(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build/man
      COMMAND sphinx-build -b man -d ${CMAKE_CURRENT_BINARY_DIR}/build/doctrees ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/build/man
      VERBATIM
    )
  ENDIF()

  ADD_CUSTOM_TARGET(man ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build/man
  )

  INSTALL(
    CODE "FILE( GLOB MAN1_FILES \"${CMAKE_CURRENT_BINARY_DIR}/build/man/*.1\" )"
    CODE "FILE( INSTALL \${MAN1_FILES} DESTINATION \"${INSTALL_MANDIR}/man1\" )"
  )
ENDIF()

# generate html documentation
IF(WITH_HTML_DOCS)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build/html
    # Downloads percona-theme into source dir because of sphinx bug:
    # https://github.com/sphinx-doc/sphinx/issues/925
    COMMAND wget -O ${CMAKE_CURRENT_SOURCE_DIR}/percona-theme.tar.gz https://www.percona.com/docs/theme-1-4/percona-xtrabackup/2.3
    COMMAND tar -zxf ${CMAKE_CURRENT_SOURCE_DIR}/percona-theme.tar.gz -C ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND rm -f ${CMAKE_CURRENT_SOURCE_DIR}/percona-theme.tar.gz
    COMMAND sphinx-build -b html -d ${CMAKE_CURRENT_BINARY_DIR}/build/doctrees ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/build/html
    COMMAND rm -rf ${CMAKE_CURRENT_SOURCE_DIR}/percona-theme-1-4
    VERBATIM
  )

  ADD_CUSTOM_TARGET(html ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build/html
  )
ENDIF()

# generate latex or pdf documentation
IF(WITH_LATEX_DOCS OR WITH_PDF_DOCS)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build/latex/PerconaXtraBackup-2.3.tex
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build/latex/Makefile
    COMMAND sphinx-build -b latex -d ${CMAKE_CURRENT_BINARY_DIR}/build/doctrees ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/build/latex
    VERBATIM
  )

  ADD_CUSTOM_TARGET(latex ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build/latex/PerconaXtraBackup-2.3.tex
  )

  IF(WITH_PDF_DOCS)
    ADD_CUSTOM_COMMAND(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build/latex/PerconaXtraBackup-2.3.pdf
      COMMAND make -C ${CMAKE_CURRENT_BINARY_DIR}/build/latex all-pdf
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build/latex/PerconaXtraBackup-2.3.tex
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build/latex/Makefile
      VERBATIM
    )

    ADD_CUSTOM_TARGET(latexpdf ALL
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build/latex/PerconaXtraBackup-2.3.pdf
    )
  ENDIF()

ENDIF()
