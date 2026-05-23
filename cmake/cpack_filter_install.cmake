# cmake/cpack_filter_install.cmake
# =============================================================================
# Post-install cleanup — removes MySQL files not shipped in XtraBackup packages.
# Replicates the rm -rf lines from the RPM spec %install section and the
# implicit file filtering from debian/*.install files.
#
# Called via: INSTALL(SCRIPT ... COMPONENT Runtime) in CMakeLists.txt
# =============================================================================

# Remove libmysqlservices.a
FILE(GLOB _unwanted_libs
  "${CMAKE_INSTALL_PREFIX}/lib/libmysqlservices.a"
  "${CMAKE_INSTALL_PREFIX}/lib64/libmysqlservices.a"
)
FOREACH(_f ${_unwanted_libs})
  IF(EXISTS "${_f}")
    FILE(REMOVE "${_f}")
    MESSAGE(STATUS "PXB filter: removed ${_f}")
  ENDIF()
ENDFOREACH()

# Remove INFO_SRC
IF(EXISTS "${CMAKE_INSTALL_PREFIX}/docs/INFO_SRC")
  FILE(REMOVE "${CMAKE_INSTALL_PREFIX}/docs/INFO_SRC")
ENDIF()

# Remove man8/ entirely
SET(_man8 "${CMAKE_INSTALL_PREFIX}/share/man/man8")
IF(IS_DIRECTORY "${_man8}")
  FILE(REMOVE_RECURSE "${_man8}")
ENDIF()

# Remove man1 pages not related to XtraBackup
# Keep: xtrabackup.1, xbstream.1, xbcrypt.1, xbcloud.1
# Remove: c* m* i* l* p* z*
SET(_man1 "${CMAKE_INSTALL_PREFIX}/share/man/man1")
IF(IS_DIRECTORY "${_man1}")
  FILE(GLOB _unwanted_man1
    "${_man1}/c*.1*"
    "${_man1}/m*.1*"
    "${_man1}/i*.1*"
    "${_man1}/l*.1*"
    "${_man1}/p*.1*"
    "${_man1}/z*.1*"
  )
  FOREACH(_f ${_unwanted_man1})
    FILE(REMOVE "${_f}")
  ENDFOREACH()
ENDIF()
